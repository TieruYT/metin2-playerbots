#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
REPO_ROOT="$(CDPATH= cd -- "$SCRIPT_DIR/../.." && pwd)"
SOURCE_ROOT="$REPO_ROOT/linux-port/docker/game/src/server/game/src"
BUILDER_CONTAINER="${M2_FAST_BUILDER_CONTAINER:-metin2-game-builder}"
RUNTIME_BASE="${M2_FAST_RUNTIME_BASE:-metin2/game-runtime-base:incremental}"
OUTPUT_IMAGE="${M2_FAST_OUTPUT_IMAGE:-metin2/game:40250}"
MAKE_JOBS="${M2_MAKE_JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)}"

if ! docker container inspect "$BUILDER_CONTAINER" >/dev/null 2>&1; then
    echo "Brak kontenera $BUILDER_CONTAINER. Najpierw uruchom setup.sh." >&2
    exit 1
fi
docker start "$BUILDER_CONTAINER" >/dev/null

# This build replaces the engine binary and nothing else. Everything else in the
# image - the CONFIG renderer, the entrypoint, the rates, the quests, the share
# tree - comes from the runtime base, which setup.sh built once. So a change to
# any of those is invisible here, and worse than invisible: rebuilding puts the
# old copy back over a correct image.
#
# That is not hypothetical. The map split that moved Orc Valley onto the core
# the bots live on landed on 2 September; the base on the machine where this was
# found dated from 30 August, and every fast build since had been restoring the
# old split behind it. Bots stood at the teleporter making seven thousand
# impossible warp requests a minute while the fix sat in the repository.
#
# The base cannot be refreshed from here: it is built on top of the builder
# stage, so rebuilding it is the full engine compile this tool exists to skip.
# Saying so is enough - the operator runs setup.sh when it matters.
GAME_CONTEXT="$REPO_ROOT/linux-port/docker/game"
if base_created="$(docker image inspect "$RUNTIME_BASE" --format '{{.Created}}' 2>/dev/null)"; then
    base_epoch="$(date -d "$base_created" +%s 2>/dev/null || echo 0)"
    newest=0
    newest_file=""
    while IFS= read -r candidate; do
        stamp="$(date -r "$candidate" +%s 2>/dev/null || echo 0)"
        if [ "$stamp" -gt "$newest" ]; then
            newest="$stamp"
            newest_file="$candidate"
        fi
    done <<EOF
$(find "$GAME_CONTEXT" -type f -not -path "$GAME_CONTEXT/src/*" 2>/dev/null)
EOF
    if [ "$base_epoch" -gt 0 ] && [ "$newest" -gt "$base_epoch" ]; then
        echo "UWAGA: baza $RUNTIME_BASE jest starsza niz kontekst obrazu." >&2
        echo "  baza:  $base_created" >&2
        echo "  nowszy plik: ${newest_file#$REPO_ROOT/}" >&2
        echo "  Ten build podmienia tylko silnik, wiec ta zmiana NIE trafi do obrazu" >&2
        echo "  - a jesli obraz juz ja mial, zostanie cofnieta. Uruchom setup.sh." >&2
    fi
fi

if [ "$#" -eq 0 ]; then
    set -- playerbot_manager.cpp
fi

for relative in "$@"; do
    case "$relative" in
        /*|*..*)
            echo "Niedozwolona ścieżka: $relative" >&2
            exit 1
            ;;
    esac
    if [ ! -f "$SOURCE_ROOT/$relative" ]; then
        echo "Nie znaleziono $SOURCE_ROOT/$relative" >&2
        exit 1
    fi
    destination="/src/server/game/src/$relative"
    # Every container path goes through "sh -lc" with the path inside the quoted
    # string, and that is not a style choice. Git Bash rewrites a bare argument
    # that looks like a Unix path into a Windows one, so "docker exec ... touch
    # /src/server/game/src/input_db.cpp" touched C:/Program Files/Git/src/...
    # instead - silently, with a zero exit status. docker cp is unaffected and
    # preserves the source mtime, which for a file checked out of git is older
    # than the object built from it, so make declared the file up to date and
    # the build produced a byte-identical binary.
    #
    # That is how a fixed autospawn sat in the repository for four days while
    # the running world spawned bots from the contiguous PID range the fix had
    # replaced: 362 bots started out of 750 asked for, and the 262 identities at
    # the end of the cohort could never start at all. The touch is what makes a
    # rebuild a rebuild; it has to actually land.
    docker exec "$BUILDER_CONTAINER" sh -lc "mkdir -p \"\$(dirname '$destination')\""
    docker cp "$SOURCE_ROOT/$relative" "$BUILDER_CONTAINER:$destination"
    docker exec "$BUILDER_CONTAINER" sh -lc "touch '$destination'"
    stamped="$(docker exec "$BUILDER_CONTAINER" sh -lc "date -r '$destination' +%s" 2>/dev/null || echo 0)"
    now="$(docker exec "$BUILDER_CONTAINER" sh -lc "date +%s" 2>/dev/null || echo 0)"
    if [ "$stamped" -gt 0 ] && [ "$now" -gt 0 ] && [ $((now - stamped)) -gt 60 ]; then
        echo "BLAD: $relative nie zostal odswiezony w builderze - make go pominie." >&2
        exit 1
    fi
done

docker exec "$BUILDER_CONTAINER" sh -lc \
    "cd /src/server/game/src && make -j'$MAKE_JOBS' M2_EXTERN_LINUX=/opt/m2extern EXTERN_DIR=/opt/m2extern && strip --strip-unneeded ../game"

TEMP_DIR="$(mktemp -d)"
cleanup() {
    rm -rf -- "$TEMP_DIR"
}
trap cleanup EXIT INT TERM

docker cp "$BUILDER_CONTAINER:/src/server/game/game" "$TEMP_DIR/game"
docker build \
    --build-arg "RUNTIME_BASE=$RUNTIME_BASE" \
    -f "$SCRIPT_DIR/Dockerfile.runtime" \
    -t "$OUTPUT_IMAGE" \
    "$TEMP_DIR"

echo "Gotowy obraz: $OUTPUT_IMAGE"
sha256sum "$TEMP_DIR/game"
echo "Wdrożenie: cd linux-port/docker && docker compose up -d --no-deps --force-recreate game"
