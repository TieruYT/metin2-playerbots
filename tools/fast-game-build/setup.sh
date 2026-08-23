#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
REPO_ROOT="$(CDPATH= cd -- "$SCRIPT_DIR/../.." && pwd)"
GAME_CONTEXT="$REPO_ROOT/linux-port/docker/game"
BUILDER_IMAGE="${M2_FAST_BUILDER_IMAGE:-metin2/game-builder:40250-incremental}"
RUNTIME_BASE="${M2_FAST_RUNTIME_BASE:-metin2/game-runtime-base:incremental}"
BUILDER_CONTAINER="${M2_FAST_BUILDER_CONTAINER:-metin2-game-builder}"

if [ ! -f "$GAME_CONTEXT/src/server/game/src/Makefile" ]; then
    echo "Brak przygotowanego kontekstu game/src. Najpierw uruchom linux-port/fetch-sources.sh." >&2
    exit 1
fi

echo "Buduję jednorazowy obraz kompilatora..."
docker build --target builder -t "$BUILDER_IMAGE" "$GAME_CONTEXT"

echo "Buduję bazę obrazu runtime..."
docker build --target runtime -t "$RUNTIME_BASE" "$GAME_CONTEXT"

if docker container inspect "$BUILDER_CONTAINER" >/dev/null 2>&1; then
    docker start "$BUILDER_CONTAINER" >/dev/null
    echo "Kontener $BUILDER_CONTAINER już istnieje; zachowuję jego cache obiektów."
else
    docker create --name "$BUILDER_CONTAINER" "$BUILDER_IMAGE" sleep infinity >/dev/null
    docker start "$BUILDER_CONTAINER" >/dev/null
fi

echo "Szybki builder jest gotowy."
