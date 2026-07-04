#!/usr/bin/env bash
# run-h004-probe.sh — build and inject h004_probe.dylib into a given
# Cyberpunk 2077 binary (default: the LIVE Steam install, read-only test).
#
# Usage:
#   tools/run-h004-probe.sh                       # against live Steam install
#   GAME_BIN=/path/to/Cyberpunk2077 tools/run-h004-probe.sh   # against a test copy
#
# Output: /tmp/h004-probe.log  (tailed live; also printed on exit)
#
# The probe fires at dyld image-load time (very early — before the game
# window appears) and only tests+reverts __TEXT page protection; it never
# writes bytes. This script kills the game automatically ~5s after launch
# so we don't need to sit through a full boot for a diagnostic-only run.

set -uo pipefail

DEFAULT_GAME="$HOME/Library/Application Support/Steam/steamapps/common/Cyberpunk 2077/Cyberpunk2077.app/Contents/MacOS/Cyberpunk2077"
GAME="${GAME_BIN:-$DEFAULT_GAME}"
REPO="$(cd "$(dirname "$0")/.." && pwd)"
PROBE="$REPO/build/src/red4ext-mac/libh004_probe.dylib"
LOG="/tmp/h004-probe.log"
AUTO_KILL_SEC="${AUTO_KILL_SEC:-5}"

# ── Sanity checks ────────────────────────────────────────────────────────────

if [[ ! -f "$GAME" ]]; then
    echo "ERROR: game binary not found at:" >&2
    echo "  $GAME" >&2
    echo "Set GAME_BIN to override." >&2
    exit 1
fi

# ── Build probe if dylib is missing ─────────────────────────────────────────

if [[ ! -f "$PROBE" ]]; then
    echo "[run-h004] libh004_probe.dylib not found — building..."
    cmake -B "$REPO/build" -S "$REPO" -DCMAKE_BUILD_TYPE=Debug \
        2>&1 | tail -5
    cmake --build "$REPO/build" --target h004_probe \
        2>&1 | tail -10
fi

if [[ ! -f "$PROBE" ]]; then
    echo "ERROR: build failed — $PROBE still missing" >&2
    exit 1
fi

echo "[run-h004] probe : $PROBE"
echo "[run-h004] log   : $LOG"
echo "[run-h004] game  : $GAME"
echo "[run-h004] auto-kill after ${AUTO_KILL_SEC}s"
echo ""

: > "$LOG"

# ── Launch ───────────────────────────────────────────────────────────────────

DYLD_INSERT_LIBRARIES="$PROBE" "$GAME" &
GAME_PID=$!

( sleep "$AUTO_KILL_SEC"; kill "$GAME_PID" 2>/dev/null ) &
KILLER_PID=$!

wait "$GAME_PID" 2>/dev/null
kill "$KILLER_PID" 2>/dev/null || true

echo ""
echo "[run-h004] ===== log contents ====="
cat "$LOG"
echo "[run-h004] ===== end log ====="
echo "[run-h004] Log saved at: $LOG"
