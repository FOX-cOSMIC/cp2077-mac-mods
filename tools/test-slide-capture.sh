#!/usr/bin/env bash
# test-slide-capture.sh — P1.1 injection smoke test.
#
# Builds libred4ext.dylib, injects it into Cyberpunk 2077, waits a few seconds
# for the slide-capture constructor to fire, then greps the red4ext-mac log for
# the "loader init ... slide=0x..." line and reports pass/fail + the slide.
#
# Auto-kills the game after ~5 seconds. Use override: GAME=/path tools/test-slide-capture.sh
#
# Exit 0 = slide captured; non-zero = failure.

set -uo pipefail

GAME="${GAME:-$HOME/Library/Application Support/Steam/steamapps/common/Cyberpunk 2077/Cyberpunk2077.app/Contents/MacOS/Cyberpunk2077}"
REPO="$(cd "$(dirname "$0")/.." && pwd)"
DYLIB="$REPO/build/src/red4ext-mac/libred4ext.dylib"
LOG="/tmp/red4ext-mac.log"
WAIT_SECS=5

# ── Build if missing ─────────────────────────────────────────────────────────
if [[ ! -f "$DYLIB" ]]; then
    echo "[test-slide] libred4ext.dylib not found — building..."
    cmake -B "$REPO/build" -S "$REPO" -DCMAKE_BUILD_TYPE=Debug 2>&1 | tail -5
    cmake --build "$REPO/build" --target red4ext 2>&1 | tail -10
fi
if [[ ! -f "$DYLIB" ]]; then
    echo "ERROR: build failed — $DYLIB still missing" >&2
    exit 2
fi

if [[ ! -f "$GAME" ]]; then
    echo "ERROR: game binary not found:" >&2
    echo "  $GAME" >&2
    echo "Set GAME env var to override." >&2
    exit 2
fi

echo "[test-slide] dylib : $DYLIB"
echo "[test-slide] log   : $LOG"
echo "[test-slide] game  : $GAME"

# ── Clean log, launch injected, kill after WAIT_SECS ─────────────────────────
: > "$LOG"

DYLD_INSERT_LIBRARIES="$DYLIB" "$GAME" >/dev/null 2>&1 &
GAME_PID=$!

cleanup() {
    kill "$GAME_PID" 2>/dev/null || true
    wait "$GAME_PID" 2>/dev/null || true
}
trap cleanup EXIT INT TERM

echo "[test-slide] launched pid=$GAME_PID; waiting ${WAIT_SECS}s for slide capture..."
sleep "$WAIT_SECS"
cleanup
trap - EXIT INT TERM

# ── Evaluate ─────────────────────────────────────────────────────────────────
echo "----------------------------------------"
LINE="$(grep -m1 '\[red4ext-mac\] loader init' "$LOG" 2>/dev/null || true)"

if [[ -z "$LINE" ]]; then
    echo "[test-slide] FAIL: no '[red4ext-mac] loader init' line in $LOG"
    echo "--- log tail ---"
    tail -20 "$LOG" 2>/dev/null || echo "(log empty)"
    exit 1
fi

echo "[test-slide] found: $LINE"

SLIDE="$(printf '%s\n' "$LINE" | grep -oE 'slide=0x[0-9a-fA-F]+' | head -1)"
if [[ -z "$SLIDE" ]]; then
    echo "[test-slide] FAIL: line present but no slide value (main image not detected?)"
    exit 1
fi

echo "[test-slide] PASS: captured $SLIDE"
exit 0
