#!/usr/bin/env bash
# test-value-rw.sh — P1.6 in-game smoke test (flat layout discovery + R/W).
#
# Builds + injects libred4ext.dylib, waits for TweakDB populate, then reads
# /tmp/red4ext-mac.log for the lines emitted by VerifyFlatEntry() (folded into
# the P1.2 deferred sample, after VerifyH008 + VerifyHashFunction):
#
#   [flat-layout] verdict: <buffer-via-tdbOffset|buffer-via-entryOffset|flatValue-ptr-at-entry|unknown> ...
#   [hashmap]     flats-map hash-function: <fnv1a-8B|fnv1a-5B|...>
#   [flat-rw]     read   id=.. found=yes raw=0x..
#   [flat-rw]     write  id=.. old=0x.. new=0x.. result=ok reread=0x.. match=yes
#   [flat-rw]     restore old=0x.. result=ok
#   [flat-rw]     verdict: <pass|fail>
#
# Outcomes:
#   PASS       = flat-layout verdict != 'unknown' AND flat-rw verdict = 'pass'
#                (the discovered layout reads, writes, and restores a scalar).
#   FAIL(layout) = layout verdict = 'unknown' (no value-source hypothesis matched
#                  across samples — inspect the [flat-layout] sample[i] dumps and
#                  hand to researcher to map the flats value encoding).
#   FAIL(rw)   = layout found but flat-rw verdict = 'fail' (resolve/write/verify
#                broke — see the [flat-write]/[flat-rw] lines).
#   WEAK PASS  = lines absent (DB not populated in the window — raise WAIT_SECS).
#   FAIL(hard) = no [flat-layout] line at all (chain broken upstream).
#
# The test RESTORES the original value before exit — game state is unchanged.
# Auto-kills the game. Override: GAME=/path WAIT_SECS=30 tools/test-value-rw.sh

set -uo pipefail

GAME="${GAME:-$HOME/Library/Application Support/Steam/steamapps/common/Cyberpunk 2077/Cyberpunk2077.app/Contents/MacOS/Cyberpunk2077}"
REPO="$(cd "$(dirname "$0")/.." && pwd)"
DYLIB="$REPO/build/src/red4ext-mac/libred4ext.dylib"
LOG="/tmp/red4ext-mac.log"
WAIT_SECS="${WAIT_SECS:-15}"

if [[ ! -f "$DYLIB" ]]; then
    echo "[test-value-rw] libred4ext.dylib not found — building..."
    cmake -B "$REPO/build" -S "$REPO" -DCMAKE_BUILD_TYPE=Debug 2>&1 | tail -5
    cmake --build "$REPO/build" --target red4ext 2>&1 | tail -10
fi
if [[ ! -f "$DYLIB" ]]; then echo "ERROR: build failed — $DYLIB missing" >&2; exit 2; fi
if [[ ! -f "$GAME"  ]]; then echo "ERROR: game binary not found: $GAME (set GAME=)" >&2; exit 2; fi

echo "[test-value-rw] dylib : $DYLIB"
echo "[test-value-rw] log   : $LOG"
echo "[test-value-rw] game  : $GAME"

: > "$LOG"
DYLD_INSERT_LIBRARIES="$DYLIB" "$GAME" >/dev/null 2>&1 &
GAME_PID=$!
cleanup() { kill "$GAME_PID" 2>/dev/null || true; wait "$GAME_PID" 2>/dev/null || true; }
trap cleanup EXIT INT TERM

echo "[test-value-rw] launched pid=$GAME_PID; waiting ${WAIT_SECS}s for TweakDB populate..."
sleep "$WAIT_SECS"
cleanup
trap - EXIT INT TERM

echo "----------------------------------------"
LAYOUT_LINE="$(grep -m1 '\[flat-layout\] verdict:'            "$LOG" 2>/dev/null || true)"
HASH_LINE="$(grep -m1   '\[hashmap\] flats-map hash-function:' "$LOG" 2>/dev/null || true)"
RW_VERDICT_LINE="$(grep -m1 '\[flat-rw\] verdict:'            "$LOG" 2>/dev/null || true)"

# Echo the full discovery context (sample dumps + write line) for the operator.
grep -E '\[flat-layout\]|\[hashmap\] flats-map|\[flat-rw\]|\[flat-write\]' "$LOG" 2>/dev/null \
    | sed 's/^/[test-value-rw] /'

if [[ -z "$LAYOUT_LINE" ]]; then
    echo "[test-value-rw] FAIL: no '[flat-layout]' line in $LOG (DB never sampled / chain broken)"
    echo "--- log tail ---"; tail -20 "$LOG" 2>/dev/null || echo "(log empty)"
    exit 1
fi

LAYOUT="$(printf '%s\n' "$LAYOUT_LINE" | sed -n 's/.*verdict: \([A-Za-z0-9-]*\).*/\1/p')"
RW="$(printf '%s\n' "$RW_VERDICT_LINE" | sed -n 's/.*verdict: \([a-z]*\).*/\1/p')"

if [[ "$LAYOUT" == "unknown" || -z "$LAYOUT" ]]; then
    echo "[test-value-rw] FAIL: flat value layout NOT identified (verdict='$LAYOUT')."
    echo "[test-value-rw]   No value-source hypothesis matched — see [flat-layout] sample[i] dumps."
    echo "[test-value-rw]   ACTION: hand sample dumps to researcher to map the flats value encoding."
    exit 1
fi

if [[ "$RW" == "pass" ]]; then
    echo "[test-value-rw] PASS: layout='$LAYOUT', scalar read/write/verify/restore round-trip succeeded."
    exit 0
fi

echo "[test-value-rw] FAIL: layout='$LAYOUT' but flat-rw verdict='$RW'."
echo "[test-value-rw]   The value position resolved but the write/verify did not round-trip."
echo "[test-value-rw]   Inspect the [flat-write] / [flat-rw] lines above."
exit 1
