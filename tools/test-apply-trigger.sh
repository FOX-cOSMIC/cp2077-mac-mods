#!/usr/bin/env bash
# test-apply-trigger.sh — P1.3 in-game smoke test.
#
# Injects libred4ext.dylib, gives the game ~15s to construct & populate TweakDB,
# then verifies the apply-trigger polling loop fired and the folded-in bring-up
# probes still PASS.
#
# PASS requires ALL of:
#   [apply-trigger] singleton non-null at poll N (TweakDB=0x...)
#   [apply-trigger] count stable at <records.count> after <N consecutive polls>
#   [apply-trigger] firing callbacks (db=0x..., poll #N)
#   [apply-trigger] system callbacks done; user mods can apply now
# AND no regression in the existing probes (H-008 / hash-function / flat-entry).
#
# FAIL if polling times out, callbacks don't fire, or a probe regresses.
# Auto-kills the game. Override path: GAME=/path tools/test-apply-trigger.sh

set -uo pipefail

GAME="${GAME:-$HOME/Library/Application Support/Steam/steamapps/common/Cyberpunk 2077/Cyberpunk2077.app/Contents/MacOS/Cyberpunk2077}"
REPO="$(cd "$(dirname "$0")/.." && pwd)"
DYLIB="$REPO/build/src/red4ext-mac/libred4ext.dylib"
LOG="/tmp/red4ext-mac.log"
WAIT_SECS=15

# ── Build if missing ─────────────────────────────────────────────────────────
if [[ ! -f "$DYLIB" ]]; then
    echo "[test-apply] libred4ext.dylib not found — building..."
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

echo "[test-apply] dylib : $DYLIB"
echo "[test-apply] log   : $LOG"
echo "[test-apply] game  : $GAME"

# ── Clean log, launch injected, wait, kill ───────────────────────────────────
: > "$LOG"

DYLD_INSERT_LIBRARIES="$DYLIB" "$GAME" >/dev/null 2>&1 &
GAME_PID=$!
cleanup() { kill "$GAME_PID" 2>/dev/null || true; wait "$GAME_PID" 2>/dev/null || true; }
trap cleanup EXIT INT TERM

echo "[test-apply] launched pid=$GAME_PID; waiting ${WAIT_SECS}s for TweakDB populate..."
sleep "$WAIT_SECS"
cleanup
trap - EXIT INT TERM

# ── Evaluate ─────────────────────────────────────────────────────────────────
echo "----------------------------------------"

fails=0
require() {  # require <label> <regex>
    local label="$1" rx="$2" line
    line="$(grep -m1 -E "$rx" "$LOG" 2>/dev/null || true)"
    if [[ -n "$line" ]]; then
        echo "  [PASS] $label: $line"
    else
        echo "  [FAIL] $label: no line matching /$rx/"
        fails=$((fails + 1))
    fi
}

echo "Apply-trigger lifecycle:"
require "singleton non-null" '\[apply-trigger\] singleton non-null at poll [0-9]+ \(TweakDB=0x[0-9a-fA-F]+\)'
require "count stable"       '\[apply-trigger\] count stable at [0-9]+ after [0-9]+ consecutive polls'
require "firing callbacks"   '\[apply-trigger\] firing callbacks \(db=0x[0-9a-fA-F]+, poll #[0-9]+\)'
require "system cb done"     '\[apply-trigger\] system callbacks done; user mods can apply now'

# Timeout must NOT appear.
if grep -qE '\[apply-trigger\] timeout' "$LOG" 2>/dev/null; then
    echo "  [FAIL] polling timed out (singleton/count never settled)"
    fails=$((fails + 1))
fi

echo "Folded-in bring-up probes (must not regress):"
# These are the verdict lines emitted by VerifyH008 / VerifyHashFunction /
# VerifyFlatEntry. Match the stable prefixes; a missing line = regression.
# Match only the SUCCESS verdict lines — the "skipped (...)" variants (db null,
# map unreadable, etc.) must NOT count as a pass.
require "H-008 verdict"       '\[tweakdb\] H-008 verification: .*verdict=flats-is'
require "records hash fn"     '\[hashmap\] hash-function: .*stored=0x'
require "flat-layout verdict" '\[flat-layout\] verdict: .*type-tag-offset:'

echo "----------------------------------------"
if [[ "$fails" -eq 0 ]]; then
    echo "[test-apply] PASS: apply-trigger fired and all probes intact"
    exit 0
fi
echo "[test-apply] FAIL: $fails check(s) failed"
echo "--- log tail ---"
tail -40 "$LOG" 2>/dev/null || echo "(log empty)"
exit 1
