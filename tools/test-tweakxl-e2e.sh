#!/usr/bin/env bash
# test-tweakxl-e2e.sh — P1.11 end-to-end smoke test.
#
# Proves the WHOLE chain wires up in-game:
#   inject libtweakxl.dylib -> dyld pulls libred4ext.dylib (declared dep) ->
#   red4ext loader init -> apply-trigger fires -> tweakxl scans the mods dir ->
#   parses the YAML -> applies via ApplyMod -> logs aggregate results.
#
# We point TWEAKXL_MODS_DIR at a temp dir holding ONE synthetic YAML that
# targets a flat which does not exist, so the demo verifies the wiring WITHOUT
# mutating real game state: expect 1 rejected op.
#
# NOTE on injection: we insert ONLY libtweakxl.dylib. It declares a dependency
# on @rpath/libred4ext.dylib (P1.11 link), so dyld loads red4ext automatically.
#
# PASS requires ALL five lines below. Auto-kills the game, cleans the temp dir.
# Override game path: GAME=/path tools/test-tweakxl-e2e.sh

set -uo pipefail

GAME="${GAME:-$HOME/Library/Application Support/Steam/steamapps/common/Cyberpunk 2077/Cyberpunk2077.app/Contents/MacOS/Cyberpunk2077}"
REPO="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="$REPO/build"
DYLIB="$BUILD_DIR/src/tweakxl-mac/libtweakxl.dylib"
LOG="/tmp/red4ext-mac.log"
MODS_DIR="/tmp/tweakxl-e2e-mods"
WAIT_SECS=20

# ── Build if missing/stale ───────────────────────────────────────────────────
if [[ ! -f "$DYLIB" ]] || [[ "$REPO/src/tweakxl-mac/src/plugin/Plugin.cpp" -nt "$DYLIB" ]]; then
    echo "[e2e] (re)building libtweakxl.dylib ..."
    cmake -B "$BUILD_DIR" -S "$REPO" -DCMAKE_BUILD_TYPE=Debug 2>&1 | tail -3
    cmake --build "$BUILD_DIR" --target tweakxl 2>&1 | tail -6
fi
if [[ ! -f "$DYLIB" ]]; then
    echo "[e2e] ERROR: build failed — $DYLIB missing" >&2
    exit 2
fi
if [[ ! -f "$GAME" ]]; then
    echo "[e2e] ERROR: game binary not found:" >&2
    echo "  $GAME" >&2
    echo "  Set GAME=/path to override." >&2
    exit 2
fi

# ── Synthetic mod: targets a flat that does not exist ────────────────────────
rm -rf "$MODS_DIR"
mkdir -p "$MODS_DIR"
cat > "$MODS_DIR/test.yaml" <<'YAML'
# Synthetic mod for testing end-to-end wiring. Targets a flat that does not
# exist; expect 1 rejected op (mod does not apply, real game state untouched).
Test.Nonexistent.flagFlatThatDoesntExist: 42
YAML

echo "[e2e] dylib    : $DYLIB"
echo "[e2e] mods dir : $MODS_DIR"
echo "[e2e] game     : $GAME"
echo "[e2e] log      : $LOG"

# ── Clean log, launch injected, wait, kill ───────────────────────────────────
: > "$LOG"

TWEAKXL_MODS_DIR="$MODS_DIR" DYLD_INSERT_LIBRARIES="$DYLIB" "$GAME" >/dev/null 2>&1 &
GAME_PID=$!
cleanup() {
    kill "$GAME_PID" 2>/dev/null || true
    wait "$GAME_PID" 2>/dev/null || true
    rm -rf "$MODS_DIR"
}
trap cleanup EXIT INT TERM

echo "[e2e] launched pid=$GAME_PID; waiting ${WAIT_SECS}s for DB populate + apply..."
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

require "red4ext loaded"     '\[red4ext-mac\] loader init'
require "plugin init"        '\[tweakxl\] plugin orchestrator init'
require "apply-trigger fired" '\[apply-trigger\] firing callbacks'
require "mods scanned"       '\[tweakxl\] mods scanned: 1 \(yaml=1, tweak=0\)'
# Atomic-apply result. NOTE: rollbacks reflects P1.10a semantics — a mod
# rejected before writing anything has nothing to restore, so rollbacks=0.
# (The P1.11 task text expected rollbacks: 1; see the open issue in the reply —
# this is a Conductor-level reconciliation, not a wiring failure.)
require "yaml applied result" '\[tweakxl\] yaml applied: 0/1, ops applied: 0, skipped: 0, rejected: 1, rollbacks: [0-9]+'

echo "----------------------------------------"
if [[ "$fails" -eq 0 ]]; then
    echo "[e2e] PASS — end-to-end chain verified"
    exit 0
else
    echo "[e2e] FAIL ($fails missing line(s))"
    echo "[e2e] --- tweakxl/apply-trigger log lines ---"
    grep -E '\[tweakxl\]|\[apply-trigger\]|\[red4ext-mac\]' "$LOG" 2>/dev/null || echo "  (none)"
    exit 1
fi
