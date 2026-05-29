#!/usr/bin/env bash
# run-h001-probe.sh — build and inject h001_probe.dylib into Cyberpunk 2077
#
# Usage:  tools/run-h001-probe.sh
# Output: /tmp/h001-probe.log  (tailed live; also printed on exit)
#
# Ctrl+C kills the game cleanly and prints the log path.

set -uo pipefail

GAME="$HOME/Library/Application Support/Steam/steamapps/common/Cyberpunk 2077/Cyberpunk2077.app/Contents/MacOS/Cyberpunk2077"
REPO="$(cd "$(dirname "$0")/.." && pwd)"
PROBE="$REPO/build/src/red4ext-mac/libh001_probe.dylib"
LOG="/tmp/h001-probe.log"

# ── Sanity checks ────────────────────────────────────────────────────────────

if [[ ! -f "$GAME" ]]; then
    echo "ERROR: game binary not found at:" >&2
    echo "  $GAME" >&2
    echo "Set GAME env var to override." >&2
    exit 1
fi

# ── Build probe if dylib is missing ─────────────────────────────────────────

if [[ ! -f "$PROBE" ]]; then
    echo "[run-h001] libh001_probe.dylib not found — building..."
    cmake -B "$REPO/build" -S "$REPO" -DCMAKE_BUILD_TYPE=Debug \
        2>&1 | tail -5
    cmake --build "$REPO/build" --target h001_probe \
        2>&1 | tail -10
fi

if [[ ! -f "$PROBE" ]]; then
    echo "ERROR: build failed — $PROBE still missing" >&2
    exit 1
fi

echo "[run-h001] probe : $PROBE"
echo "[run-h001] log   : $LOG"
echo "[run-h001] game  : $GAME"
echo "[run-h001] Ctrl+C to stop"
echo ""

# ── Clean state ──────────────────────────────────────────────────────────────

: > "$LOG"   # Truncate/create so tail -f can start immediately.

# ── Launch ───────────────────────────────────────────────────────────────────

DYLD_INSERT_LIBRARIES="$PROBE" "$GAME" &
GAME_PID=$!

tail -f "$LOG" &
TAIL_PID=$!

# ── Cleanup on exit / Ctrl+C ─────────────────────────────────────────────────

cleanup() {
    echo ""
    echo "[run-h001] Stopping..."
    kill "$TAIL_PID" 2>/dev/null || true
    kill "$GAME_PID" 2>/dev/null || true
    wait "$GAME_PID" 2>/dev/null || true
    echo "[run-h001] Log: $LOG"
}
trap cleanup EXIT INT TERM

wait "$GAME_PID" || true
