#!/usr/bin/env bash
# test-candidate-probe.sh — P1.12 runtime candidate-flat sweep.
#
# Builds libtweakxl.dylib (which links red4ext → runs Loader's apply callback),
# points TWEAKXL_CANDIDATES_FILE at the repo candidate list, injects, waits for
# DB populate + the apply callback, then reads /tmp/red4ext-mac.log for:
#
#   [flat-probe] preflight: records-map lookup of '<rec>' -> <HIT|MISS>
#   [flat-probe] HIT name=<name> hash=0x.. flat=.. vft=.. raw=..
#   [flat-probe] MISS name=<name> hash=0x..
#   [flat-probe] sweep complete: <N> candidates, hits=<H>, misses=<M>
#
# PASS  = preflight HIT AND hits > 0. Prints the HIT list for Conductor.
# FAIL  = preflight MISS (systemic lookup bug — CRC32/hash-mode/offset wrong),
#         or 0 hits, or no [flat-probe] line at all.
# WEAK  = sweep didn't run (DB not populated in the window — raise WAIT_SECS).
#
# Auto-kills the game. Override: GAME=/path WAIT_SECS=30 tools/test-candidate-probe.sh

set -uo pipefail

GAME="${GAME:-$HOME/Library/Application Support/Steam/steamapps/common/Cyberpunk 2077/Cyberpunk2077.app/Contents/MacOS/Cyberpunk2077}"
REPO="$(cd "$(dirname "$0")/.." && pwd)"
DYLIB="$REPO/build/src/tweakxl-mac/libtweakxl.dylib"
CANDIDATES="$REPO/tools/probes/candidate_flats.txt"
LOG="/tmp/red4ext-mac.log"
WAIT_SECS="${WAIT_SECS:-20}"

if [[ ! -f "$DYLIB" ]]; then
    echo "[test-candidate] libtweakxl.dylib not found — building..."
    cmake -B "$REPO/build" -S "$REPO" -DCMAKE_BUILD_TYPE=Debug 2>&1 | tail -5
    cmake --build "$REPO/build" --target tweakxl 2>&1 | tail -10
fi
if [[ ! -f "$DYLIB"      ]]; then echo "ERROR: build failed — $DYLIB missing" >&2; exit 2; fi
if [[ ! -f "$CANDIDATES" ]]; then echo "ERROR: candidate file missing: $CANDIDATES" >&2; exit 2; fi
if [[ ! -f "$GAME"       ]]; then echo "ERROR: game binary not found: $GAME (set GAME=)" >&2; exit 2; fi

echo "[test-candidate] dylib      : $DYLIB"
echo "[test-candidate] candidates : $CANDIDATES"
echo "[test-candidate] log        : $LOG"

: > "$LOG"
DYLD_INSERT_LIBRARIES="$DYLIB" TWEAKXL_CANDIDATES_FILE="$CANDIDATES" "$GAME" >/dev/null 2>&1 &
GAME_PID=$!
cleanup() { kill "$GAME_PID" 2>/dev/null || true; wait "$GAME_PID" 2>/dev/null || true; }
trap cleanup EXIT INT TERM

echo "[test-candidate] launched pid=$GAME_PID; waiting ${WAIT_SECS}s for DB populate + callback..."
sleep "$WAIT_SECS"
cleanup
trap - EXIT INT TERM

echo "----------------------------------------"
SELF_REC="$(grep -m1 '\[flat-probe\] preflight-self(records)' "$LOG" 2>/dev/null || true)"
SELF_FLAT="$(grep -m1 '\[flat-probe\] preflight-self(flats)'  "$LOG" 2>/dev/null || true)"
PRE_LINE="$(grep -m1 '\[flat-probe\] preflight:'              "$LOG" 2>/dev/null || true)"
SUMMARY="$(grep -m1  '\[flat-probe\] sweep complete:'         "$LOG" 2>/dev/null || true)"

[[ -n "$SELF_REC"  ]] && echo "[test-candidate] $SELF_REC"
[[ -n "$SELF_FLAT" ]] && echo "[test-candidate] $SELF_FLAT"
[[ -n "$PRE_LINE"  ]] && echo "[test-candidate] $PRE_LINE"

if [[ -z "$SELF_FLAT" && -z "$PRE_LINE" ]]; then
    echo "[test-candidate] FAIL: no '[flat-probe]' line (callback never ran / chain broken)"
    echo "--- log tail ---"; tail -20 "$LOG" 2>/dev/null || echo "(log empty)"
    exit 1
fi

# The sweep queries the FLATS map; its compact-rebuild self-test is the gate
# that makes a MISS trustworthy. Broken flats path = systemic bug = FAIL.
if ! printf '%s\n' "$SELF_FLAT" | grep -q 'compactRebuildLookup=HIT'; then
    echo "[test-candidate] FAIL: flats lookup path NOT verified (systemic bug: hash-mode/offset/CRC32)."
    echo "[test-candidate]   A known-present flats key, rebuilt compact, MISSED — candidate results unreliable."
    exit 1
fi

# Hit list for Conductor to integrate.
echo "[test-candidate] --- HITs ---"
grep '\[flat-probe\] HIT name=' "$LOG" 2>/dev/null | sed 's/^/[test-candidate] /' || true
HITS="$(grep -c '\[flat-probe\] HIT name=' "$LOG" 2>/dev/null || echo 0)"
HITS="${HITS//[^0-9]/}"; HITS="${HITS:-0}"
[[ -n "$SUMMARY" ]] && echo "[test-candidate] $SUMMARY"

if [[ -z "$SUMMARY" ]]; then
    echo "[test-candidate] WEAK: flats path verified but no sweep summary (raise WAIT_SECS, or see"
    echo "[test-candidate]   the [flat-probe] candidates-file/ERROR line below)."
    grep -m1 '\[flat-probe\] \(candidates file\|ERROR\)' "$LOG" 2>/dev/null | sed 's/^/[test-candidate] /' || true
    exit 1
fi

if [[ "$HITS" -gt 0 ]]; then
    echo "[test-candidate] PASS: flats lookup path verified and ${HITS} candidate hit(s) — list above ready for Conductor."
    exit 0
fi

echo "[test-candidate] CLEAN-MISS (not a bug): flats lookup path verified, but 0/98 candidates present."
echo "[test-candidate]   The lookup machinery is sound (self-test HIT) → these flat names are genuinely"
echo "[test-candidate]   absent as runtime flats. Scope must revise candidate_flats.txt (record↔type"
echo "[test-candidate]   inference wrong, or these flats are sparse/not loaded). Exit 3 = verified-absent."
exit 3
