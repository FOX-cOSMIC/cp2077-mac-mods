#!/usr/bin/env bash
# test-cinema-bulk.sh — bulk-mutate BaseStats Floats and launch the game.
#
# Mutates every safe Float at p10+0x54 across the ~220 gameplay-magnitude
# BaseStats records (vftable == gamedataStat_Record, detected at runtime).
# Multiplies each by FACTOR. Re-applies every REPEAT seconds to defeat any
# per-frame "reload defaults" pattern.
#
# Usage:
#   tools/test-cinema-bulk.sh              # factor=100, repeat=3s
#   FACTOR=1000 REPEAT=5 tools/test-cinema-bulk.sh
#   FACTOR=10 REPEAT=0 tools/test-cinema-bulk.sh   # one-shot, gentle
#
# Watch /tmp/red4ext-mac.log for `[cinema-bulk]` lines.
# In-game: load any save and look for visibly changed numbers in:
#   • Character menu — stats panel
#   • Inventory — weapon stat numbers
#   • Crafting — material/component values
#   • Driving — vehicle handling/speed (if Vehicle_Record is hit by accident)
# A 100× or 1000× change in even ONE visible stat = framework confirmed live.

set -euo pipefail
REPO="$(cd "$(dirname "$0")/.." && pwd)"
DYLIB="$REPO/build/src/tweakxl-mac/libtweakxl.dylib"
GAME_DIR="${GAME_DIR:-$HOME/Library/Application Support/Steam/steamapps/common/Cyberpunk 2077}"
GAME_BIN="$GAME_DIR/Cyberpunk2077.app/Contents/MacOS/Cyberpunk2077"

FACTOR="${FACTOR:-100}"
REPEAT="${REPEAT:-3}"

[[ -x "$DYLIB" ]] || { echo "missing dylib: $DYLIB — run cmake --build build" >&2; exit 1; }
[[ -x "$GAME_BIN" ]] || { echo "missing game: $GAME_BIN — set GAME_DIR" >&2; exit 1; }

: > /tmp/red4ext-mac.log
echo "[test-cinema-bulk] factor=$FACTOR repeat=${REPEAT}s"
echo "[test-cinema-bulk] launching — watch /tmp/red4ext-mac.log for [cinema-bulk]/[apply-trigger] lines"
echo "[test-cinema-bulk] in-game: load a save, look for changed stat numbers"
echo "[test-cinema-bulk] Ctrl-C to stop"

exec env \
    DYLD_INSERT_LIBRARIES="$DYLIB" \
    TWEAKXL_CINEMA_BULK=1 \
    TWEAKXL_CINEMA_BULK_FACTOR="$FACTOR" \
    TWEAKXL_CINEMA_BULK_REPEAT="$REPEAT" \
    "$GAME_BIN"
