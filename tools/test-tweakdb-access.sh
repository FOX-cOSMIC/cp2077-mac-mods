#!/usr/bin/env bash
# test-tweakdb-access.sh — P1.4 in-game smoke test + H-008 resolution.
#
# Builds libred4ext.dylib, injects it, gives the game time to construct AND
# populate TweakDB (F-018 load path), then inspects /tmp/red4ext-mac.log for the
# H-008 verification line emitted by VerifyH008() (folded into the P1.2 deferred
# sample in Loader.cpp).
#
# The dylib logs (once):
#   [tweakdb] H-008 verification: mapA(+0x58).count=N1 mapC(+0x108).count=N2 verdict=<v>
#   [tweakdb] H-008 raw: mapA{...} records{...} mapC{...} ...
#   [tweakdb] state: records.count=R valueBufferSize=S
#
# Outcomes:
#   PASS       = verdict=flats-is-A AND both candidate counts non-zero
#                → confirms H-008 (+0x58 = flats). Downstream offsets stand.
#   FAIL       = verdict=flats-is-C
#                → H-008 REFUTED (+0x108 = flats). Downstream code must swap the
#                  flats/queries map offsets before any flat-write lands.
#   WEAK PASS  = verdict=indeterminate (DB not fully populated in the window)
#                → extend WAIT_SECS and/or get past the title screen, re-run.
#   FAIL(hard) = no [tweakdb] line at all (chain broken — DB never sampled).
#
# Auto-kills the game. Override path: GAME=/path tools/test-tweakdb-access.sh
# Tune the populate window:           WAIT_SECS=30 tools/test-tweakdb-access.sh

set -uo pipefail

GAME="${GAME:-$HOME/Library/Application Support/Steam/steamapps/common/Cyberpunk 2077/Cyberpunk2077.app/Contents/MacOS/Cyberpunk2077}"
REPO="$(cd "$(dirname "$0")/.." && pwd)"
DYLIB="$REPO/build/src/red4ext-mac/libred4ext.dylib"
LOG="/tmp/red4ext-mac.log"
# TweakDB populate completes a few seconds after construction (F-018); give it
# more headroom than the singleton test's 10s.
WAIT_SECS="${WAIT_SECS:-15}"

# ── Build if missing ─────────────────────────────────────────────────────────
if [[ ! -f "$DYLIB" ]]; then
    echo "[test-tweakdb] libred4ext.dylib not found — building..."
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

echo "[test-tweakdb] dylib : $DYLIB"
echo "[test-tweakdb] log   : $LOG"
echo "[test-tweakdb] game  : $GAME"

# ── Clean log, launch injected, wait, kill ───────────────────────────────────
: > "$LOG"

DYLD_INSERT_LIBRARIES="$DYLIB" "$GAME" >/dev/null 2>&1 &
GAME_PID=$!

cleanup() {
    kill "$GAME_PID" 2>/dev/null || true
    wait "$GAME_PID" 2>/dev/null || true
}
trap cleanup EXIT INT TERM

echo "[test-tweakdb] launched pid=$GAME_PID; waiting ${WAIT_SECS}s for TweakDB populate..."
sleep "$WAIT_SECS"
cleanup
trap - EXIT INT TERM

# ── Evaluate ─────────────────────────────────────────────────────────────────
echo "----------------------------------------"
VERIFY_LINE="$(grep -m1 '\[tweakdb\] H-008 verification:' "$LOG" 2>/dev/null || true)"
RAW_LINE="$(grep -m1 '\[tweakdb\] H-008 raw:'            "$LOG" 2>/dev/null || true)"
STATE_LINE="$(grep -m1 '\[tweakdb\] state:'             "$LOG" 2>/dev/null || true)"

[[ -n "$VERIFY_LINE" ]] && echo "[test-tweakdb] $VERIFY_LINE"
[[ -n "$RAW_LINE"    ]] && echo "[test-tweakdb] $RAW_LINE"
[[ -n "$STATE_LINE"  ]] && echo "[test-tweakdb] $STATE_LINE"

if [[ -z "$VERIFY_LINE" ]]; then
    echo "[test-tweakdb] FAIL: no '[tweakdb] H-008 verification' line in $LOG"
    echo "[test-tweakdb]   (singleton never sampled non-null, or chain broken)"
    echo "--- log tail ---"
    tail -20 "$LOG" 2>/dev/null || echo "(log empty)"
    exit 1
fi

VERDICT="$(printf '%s\n' "$VERIFY_LINE" | sed -n 's/.*verdict=\([A-Za-z-]*\).*/\1/p')"
N1="$(printf '%s\n' "$VERIFY_LINE" | sed -n 's/.*mapA(+0x58)\.count=\([0-9]*\).*/\1/p')"
N2="$(printf '%s\n' "$VERIFY_LINE" | sed -n 's/.*mapC(+0x108)\.count=\([0-9]*\).*/\1/p')"
N1="${N1:-0}"; N2="${N2:-0}"

case "$VERDICT" in
    flats-is-A)
        if [[ "$N1" -gt 0 && "$N2" -gt 0 ]]; then
            echo "[test-tweakdb] PASS: H-008 CONFIRMED — +0x58 is flats (mapA=$N1, mapC=$N2)."
            echo "[test-tweakdb]   Downstream flats=+0x58 / queries=+0x108 offsets are correct."
            exit 0
        fi
        echo "[test-tweakdb] WEAK PASS: verdict=flats-is-A but a candidate count was zero"
        echo "[test-tweakdb]   (mapA=$N1 mapC=$N2). Re-run past the title screen to confirm."
        exit 1
        ;;
    flats-is-C)
        echo "[test-tweakdb] FAIL: H-008 REFUTED — +0x108 is flats (mapA=$N1, mapC=$N2)."
        echo "[test-tweakdb]   ACTION: swap flats/queries map offsets before enabling flat writes"
        echo "[test-tweakdb]   (GetFlatsMapCandidate must point at hashMapC). Notify researcher/Scope."
        exit 1
        ;;
    indeterminate|*)
        echo "[test-tweakdb] WEAK PASS: verdict=indeterminate (mapA=$N1 mapC=$N2)."
        echo "[test-tweakdb]   DB likely not fully populated in ${WAIT_SECS}s, or stalled at a"
        echo "[test-tweakdb]   launcher/EULA/title screen. Re-run with a longer WAIT_SECS and"
        echo "[test-tweakdb]   past the title screen. The inject→slide→read→struct chain is intact."
        exit 1
        ;;
esac
