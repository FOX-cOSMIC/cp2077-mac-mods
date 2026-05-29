#!/usr/bin/env bash
# test-singleton-access.sh — P1.2 injection smoke test.
#
# Builds libred4ext.dylib, injects it, gives the game ~10s to construct
# TweakDB, then inspects /tmp/red4ext-mac.log for the singleton-access lines.
#
# The dylib logs two lines:
#   [singleton-access] initial poll:  <ptr>   (at load — expected null, F-011)
#   [singleton-access] deferred sample: <ptr> (after a short delay — the live DB)
#
# PASS  = a deferred-sample pointer that is non-null AND in the arm64 process
#         heap range (0x1xxxxxxxx).
# WEAK  = lines present, accessor ran without crashing, but DB still null
#         (game may not have reached TweakDB construction in the window).
# FAIL  = no singleton-access line at all (chain broken).
#
# Auto-kills the game. Override path: GAME=/path tools/test-singleton-access.sh

set -uo pipefail

GAME="${GAME:-$HOME/Library/Application Support/Steam/steamapps/common/Cyberpunk 2077/Cyberpunk2077.app/Contents/MacOS/Cyberpunk2077}"
REPO="$(cd "$(dirname "$0")/.." && pwd)"
DYLIB="$REPO/build/src/red4ext-mac/libred4ext.dylib"
LOG="/tmp/red4ext-mac.log"
WAIT_SECS=10

# ── Build if missing ─────────────────────────────────────────────────────────
if [[ ! -f "$DYLIB" ]]; then
    echo "[test-singleton] libred4ext.dylib not found — building..."
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

echo "[test-singleton] dylib : $DYLIB"
echo "[test-singleton] log   : $LOG"
echo "[test-singleton] game  : $GAME"

# ── Clean log, launch injected, wait, kill ───────────────────────────────────
: > "$LOG"

DYLD_INSERT_LIBRARIES="$DYLIB" "$GAME" >/dev/null 2>&1 &
GAME_PID=$!

cleanup() {
    kill "$GAME_PID" 2>/dev/null || true
    wait "$GAME_PID" 2>/dev/null || true
}
trap cleanup EXIT INT TERM

echo "[test-singleton] launched pid=$GAME_PID; waiting ${WAIT_SECS}s for TweakDB construction..."
sleep "$WAIT_SECS"
cleanup
trap - EXIT INT TERM

# ── Evaluate ─────────────────────────────────────────────────────────────────
echo "----------------------------------------"
INIT_LINE="$(grep -m1 '\[singleton-access\] initial poll'    "$LOG" 2>/dev/null || true)"
SAMP_LINE="$(grep -m1 '\[singleton-access\] deferred sample' "$LOG" 2>/dev/null || true)"

[[ -n "$INIT_LINE" ]] && echo "[test-singleton] $INIT_LINE"
[[ -n "$SAMP_LINE" ]] && echo "[test-singleton] $SAMP_LINE"

if [[ -z "$INIT_LINE" && -z "$SAMP_LINE" ]]; then
    echo "[test-singleton] FAIL: no '[singleton-access]' line in $LOG (chain broken)"
    echo "--- log tail ---"
    tail -20 "$LOG" 2>/dev/null || echo "(log empty)"
    exit 1
fi

# Prefer the deferred sample (taken after the construction window) for non-null.
PTR="$(printf '%s\n' "$SAMP_LINE" | grep -oE '0x[0-9a-fA-F]+' | head -1)"
[[ -z "$PTR" ]] && PTR="$(printf '%s\n' "$INIT_LINE" | grep -oE '0x[0-9a-fA-F]+' | head -1)"

# Non-null AND in the arm64 process heap range: 0x1________ (9+ hex digits,
# leading nibble 1). 0x0 / short values fail this.
if [[ -n "$PTR" && "$PTR" =~ ^0x1[0-9a-fA-F]{8,}$ ]]; then
    echo "[test-singleton] PASS: live TweakDB singleton = $PTR (non-null, heap range)"
    exit 0
fi

echo "[test-singleton] WEAK PASS: accessor ran without crashing but singleton was null"
echo "[test-singleton]   (TweakDB not constructed within ${WAIT_SECS}s, or game stalled at"
echo "[test-singleton]    a launcher/EULA screen). The inject→slide→read chain is intact."
echo "[test-singleton]   Re-run with a longer WAIT_SECS or past the title screen to confirm non-null."
exit 1
