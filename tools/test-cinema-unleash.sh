#!/usr/bin/env bash
# test-cinema-unleash.sh — sledgehammer cinema. Multiplies every plausible
# gameplay-magnitude float in every named record by FACTOR (default 1000).
#
# Usage:
#   tools/test-cinema-unleash.sh                       # factor=1000, all records
#   FACTOR=10000 tools/test-cinema-unleash.sh
#   FILTER=Damage tools/test-cinema-unleash.sh          # only records w/ "Damage" in name
#   FACTOR=100 RANGE_MIN=0.1 RANGE_MAX=500 tools/test-cinema-unleash.sh

set -euo pipefail
REPO="$(cd "$(dirname "$0")/.." && pwd)"
DYLIB="$REPO/build/src/tweakxl-mac/libtweakxl.dylib"
GAME_DIR="${GAME_DIR:-$HOME/Library/Application Support/Steam/steamapps/common/Cyberpunk 2077}"
GAME_BIN="$GAME_DIR/Cyberpunk2077.app/Contents/MacOS/Cyberpunk2077"

FACTOR="${FACTOR:-1000}"
RANGE_MIN="${RANGE_MIN:-1.0}"
RANGE_MAX="${RANGE_MAX:-200.0}"
OFF_START="${OFF_START:-0x48}"
OFF_END="${OFF_END:-0x100}"
REPEAT="${REPEAT:-2}"
FILTER="${FILTER:-}"

: > /tmp/red4ext-mac.log
echo "[unleash] factor=$FACTOR range=[$RANGE_MIN, $RANGE_MAX] offsets=$OFF_START..$OFF_END"
echo "[unleash] filter='$FILTER' repeat=${REPEAT}s"
echo "[unleash] launching — load a save and look for visibly broken stats"

exec env \
    DYLD_INSERT_LIBRARIES="$DYLIB" \
    TWEAKXL_UNLEASH=1 \
    TWEAKXL_UNLEASH_FACTOR="$FACTOR" \
    TWEAKXL_UNLEASH_MIN="$RANGE_MIN" \
    TWEAKXL_UNLEASH_MAX="$RANGE_MAX" \
    TWEAKXL_UNLEASH_OFFSET_START="$OFF_START" \
    TWEAKXL_UNLEASH_OFFSET_END="$OFF_END" \
    TWEAKXL_UNLEASH_REPEAT="$REPEAT" \
    TWEAKXL_UNLEASH_NAME_SUBSTR="$FILTER" \
    "$GAME_BIN"
