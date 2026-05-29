#!/usr/bin/env bash
# test-hashmap-lookup.sh — P1.5 in-game smoke test.
#
# Builds + injects libred4ext.dylib, waits for TweakDB to populate, then reads
# /tmp/red4ext-mac.log for the two lines emitted by VerifyHashFunction()
# (folded into the P1.2 deferred sample, after VerifyH008):
#
#   [hashmap] hash-function: <fnv1a-8B|fnv1a-5B|crc32-8B|nameHash-direct|unknown> stored=.. computed=.. key=..
#   [hashmap] lookup-test:   <pass|fail> key=.. entry=.. expected=..
#
# Outcomes:
#   PASS       = hash function identified (not 'unknown') AND lookup-test=pass.
#                The bucket walker resolves a live records-map key round-trip.
#   FAIL(hash) = hash-function=unknown (no candidate matched the stored hash —
#                see the [hashmap] candidates: line; hand to researcher).
#   FAIL(walk) = hash identified but lookup-test=fail (walker logic / key-compare
#                bug, or chain unreadable).
#   WEAK PASS  = lines absent (DB not populated in the window — extend WAIT_SECS,
#                get past the title screen).
#   FAIL(hard) = no [hashmap] line at all (chain broken upstream).
#
# Auto-kills the game. Override: GAME=/path WAIT_SECS=30 tools/test-hashmap-lookup.sh

set -uo pipefail

GAME="${GAME:-$HOME/Library/Application Support/Steam/steamapps/common/Cyberpunk 2077/Cyberpunk2077.app/Contents/MacOS/Cyberpunk2077}"
REPO="$(cd "$(dirname "$0")/.." && pwd)"
DYLIB="$REPO/build/src/red4ext-mac/libred4ext.dylib"
LOG="/tmp/red4ext-mac.log"
WAIT_SECS="${WAIT_SECS:-15}"

# ── Build if missing ─────────────────────────────────────────────────────────
if [[ ! -f "$DYLIB" ]]; then
    echo "[test-hashmap] libred4ext.dylib not found — building..."
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

echo "[test-hashmap] dylib : $DYLIB"
echo "[test-hashmap] log   : $LOG"
echo "[test-hashmap] game  : $GAME"

# ── Clean log, launch injected, wait, kill ───────────────────────────────────
: > "$LOG"

DYLD_INSERT_LIBRARIES="$DYLIB" "$GAME" >/dev/null 2>&1 &
GAME_PID=$!

cleanup() {
    kill "$GAME_PID" 2>/dev/null || true
    wait "$GAME_PID" 2>/dev/null || true
}
trap cleanup EXIT INT TERM

echo "[test-hashmap] launched pid=$GAME_PID; waiting ${WAIT_SECS}s for TweakDB populate..."
sleep "$WAIT_SECS"
cleanup
trap - EXIT INT TERM

# ── Evaluate ─────────────────────────────────────────────────────────────────
echo "----------------------------------------"
HASH_LINE="$(grep -m1 '\[hashmap\] hash-function:' "$LOG" 2>/dev/null || true)"
CAND_LINE="$(grep -m1 '\[hashmap\] candidates:'    "$LOG" 2>/dev/null || true)"
TEST_LINE="$(grep -m1 '\[hashmap\] lookup-test:'   "$LOG" 2>/dev/null || true)"

[[ -n "$HASH_LINE" ]] && echo "[test-hashmap] $HASH_LINE"
[[ -n "$CAND_LINE" ]] && echo "[test-hashmap] $CAND_LINE"
[[ -n "$TEST_LINE" ]] && echo "[test-hashmap] $TEST_LINE"

if [[ -z "$HASH_LINE" && -z "$TEST_LINE" ]]; then
    echo "[test-hashmap] FAIL: no '[hashmap]' line in $LOG (DB never sampled / chain broken)"
    echo "--- log tail ---"
    tail -20 "$LOG" 2>/dev/null || echo "(log empty)"
    exit 1
fi
if [[ -z "$HASH_LINE" ]]; then
    echo "[test-hashmap] WEAK PASS: deferred sample ran but hash-function line missing"
    echo "[test-hashmap]   (DB likely not populated in ${WAIT_SECS}s). Extend WAIT_SECS."
    exit 1
fi

MODE="$(printf '%s\n' "$HASH_LINE" | sed -n 's/.*hash-function: \([A-Za-z0-9-]*\).*/\1/p')"
VERDICT="$(printf '%s\n' "$TEST_LINE" | sed -n 's/.*lookup-test: \([a-z]*\).*/\1/p')"

if [[ "$MODE" == "unknown" || -z "$MODE" ]]; then
    echo "[test-hashmap] FAIL: hash function NOT identified (mode='$MODE')."
    echo "[test-hashmap]   No candidate matched the stored hash — see candidates line above."
    echo "[test-hashmap]   ACTION: hand stored-vs-candidate values to researcher/Scope."
    exit 1
fi

if [[ "$VERDICT" == "pass" ]]; then
    echo "[test-hashmap] PASS: hash function = '$MODE', records-map lookup round-trip succeeded."
    exit 0
fi

echo "[test-hashmap] FAIL: hash function = '$MODE' but lookup-test='$VERDICT'."
echo "[test-hashmap]   Walker located the entry's hash but the round-trip lookup did not"
echo "[test-hashmap]   return it — investigate bucket index / key-compare / chain walk."
exit 1
