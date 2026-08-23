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
    docker exec "$BUILDER_CONTAINER" mkdir -p "$(dirname "$destination")"
    docker cp "$SOURCE_ROOT/$relative" "$BUILDER_CONTAINER:$destination"
    docker exec "$BUILDER_CONTAINER" touch "$destination"
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
