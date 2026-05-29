#!/usr/bin/env bash
# test-cinema-scan.sh — mutate a single Float to a distinctive value, sleep,
# then scan all writable game memory for both the original and mutated values.
# Use to find downstream READERS of the TweakDB source — those are the
# addresses the gameplay system actually consults.

set -euo pipefail
REPO="$(cd "$(dirname "$0")/.." && pwd)"
DYLIB="$REPO/build/src/tweakxl-mac/libtweakxl.dylib"
GAME_DIR="${GAME_DIR:-$HOME/Library/Application Support/Steam/steamapps/common/Cyberpunk 2077}"
GAME_BIN="$GAME_DIR/Cyberpunk2077.app/Contents/MacOS/Cyberpunk2077"

# Mutate BaseStats.AccumulatedDoTDecayRate (orig 0.0961745) to a distinctive
# float that's exceedingly unlikely to appear naturally elsewhere.
CINEMA_NAME="${CINEMA_NAME:-BaseStats.AccumulatedDoTDecayRate}"
CINEMA_VALUE="${CINEMA_VALUE:-13.371337}"
ORIG_VALUE="${ORIG_VALUE:-0.0961745}"
DELAY="${DELAY:-45}"   # seconds to wait so user can load a save before scan
MAX_HITS="${MAX_HITS:-200}"

[[ -x "$DYLIB" ]] || { echo "missing dylib: $DYLIB" >&2; exit 1; }
[[ -x "$GAME_BIN" ]] || { echo "missing game: $GAME_BIN" >&2; exit 1; }

: > /tmp/red4ext-mac.log
: > /tmp/scan_hits.tsv 2>/dev/null || true
echo "[test-cinema-scan] mutate '$CINEMA_NAME' → $CINEMA_VALUE (orig $ORIG_VALUE)"
echo "[test-cinema-scan] scan delay=${DELAY}s, max hits per target=$MAX_HITS"
echo "[test-cinema-scan] LOAD A SAVE within $DELAY seconds of game launch"
echo "[test-cinema-scan] watching /tmp/red4ext-mac.log for [scan] lines"

exec env \
    DYLD_INSERT_LIBRARIES="$DYLIB" \
    TWEAKXL_CINEMA_NAME="$CINEMA_NAME" \
    TWEAKXL_CINEMA_VALUE="$CINEMA_VALUE" \
    TWEAKXL_SCAN_FLOAT_A="$CINEMA_VALUE" \
    TWEAKXL_SCAN_FLOAT_B="$ORIG_VALUE" \
    TWEAKXL_SCAN_DELAY_SEC="$DELAY" \
    TWEAKXL_SCAN_MAX_HITS="$MAX_HITS" \
    TWEAKXL_SCAN_OUT="/tmp/scan_hits.tsv" \
    TWEAKXL_SCAN_MUTATE_B="${MUTATE_B:-1}" \
    "$GAME_BIN"
