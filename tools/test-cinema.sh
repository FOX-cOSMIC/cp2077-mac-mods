#!/usr/bin/env bash
# test-cinema.sh — cinema-demo smoke test (P1 visible-flat verification).
#
# Drops the three CINEMA_DEMO_FLATS.md candidate flats — each in its OWN file
# (apply is atomic per file: one bad op rolls back that whole file, so isolating
# them lets one bad pick fail without sinking the others) — into a temp mods dir,
# injects libtweakxl.dylib (which pulls libred4ext via its rpath dep), gives the
# game time to populate TweakDB + apply, then greps the log.
#
# PASS  = at least one `[applicator] mod applied: ... (applied=N>0)` line.
# FAIL  = no flat applied (likely `[applicator] reject op: flat not found` —
#         a record/type binding from CINEMA_DEMO_FLATS.md was wrong; not a crash).
#
# Auto-kills the game and cleans the temp dir. This is a smoke test, NOT the
# cinema recording — it does not touch the real r6/tweaks dir.
#
# Overrides:  GAME=/path  WAIT_SECS=30  tools/test-cinema.sh

set -uo pipefail

GAME="${GAME:-$HOME/Library/Application Support/Steam/steamapps/common/Cyberpunk 2077/Cyberpunk2077.app/Contents/MacOS/Cyberpunk2077}"
REPO="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="$REPO/build"
DYLIB="$BUILD_DIR/src/tweakxl-mac/libtweakxl.dylib"
LOG="/tmp/red4ext-mac.log"
MODS_DIR="/tmp/tweakxl-cinema-mods"
WAIT_SECS="${WAIT_SECS:-20}"

# ── Build if missing/stale ───────────────────────────────────────────────────
if [[ ! -f "$DYLIB" ]] || [[ "$REPO/src/tweakxl-mac/src/plugin/Plugin.cpp" -nt "$DYLIB" ]]; then
    echo "[cinema] (re)building libtweakxl.dylib ..."
    cmake -B "$BUILD_DIR" -S "$REPO" -DCMAKE_BUILD_TYPE=Debug 2>&1 | tail -3
    cmake --build "$BUILD_DIR" --target tweakxl 2>&1 | tail -6
fi
if [[ ! -f "$DYLIB" ]]; then
    echo "[cinema] ERROR: build failed — $DYLIB missing" >&2
    exit 2
fi
if [[ ! -f "$GAME" ]]; then
    echo "[cinema] ERROR: game binary not found:" >&2
    echo "  $GAME" >&2
    echo "  Set GAME=/path to override." >&2
    exit 2
fi

# ── Demo mods: each candidate flat in its own file (see CINEMA_DEMO_FLATS.md) ──
rm -rf "$MODS_DIR"
mkdir -p "$MODS_DIR"

cat > "$MODS_DIR/_cinema_vignette.yaml" <<'YAML'
Items.Preset_Nova_Default.weaponVignetteIntensity:
  $type: Float
  $value: 1.0
Items.Preset_Nova_Default.weaponVignetteRadius:
  $type: Float
  $value: 1.0
YAML

cat > "$MODS_DIR/_cinema_boom.yaml" <<'YAML'
Attacks.MissileProjectile.explosionRadius:
  $type: Float
  $value: 25.0
Attacks.MissileProjectile.explosionRange:
  $type: Float
  $value: 25.0
YAML

cat > "$MODS_DIR/_cinema_smoke.yaml" <<'YAML'
Items.GrenadeSmokeRegular.smokeEffectRadius:
  $type: Float
  $value: 30.0
YAML

echo "[cinema] dylib    : $DYLIB"
echo "[cinema] mods dir : $MODS_DIR (3 files)"
echo "[cinema] game     : $GAME"
echo "[cinema] log      : $LOG"

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

echo "[cinema] launched pid=$GAME_PID; waiting ${WAIT_SECS}s for DB populate + apply..."
sleep "$WAIT_SECS"
cleanup
trap - EXIT INT TERM

# ── Evaluate: PASS if any flat actually applied (applied=N, N>0) ──────────────
echo "----------------------------------------"
applied_lines="$(grep -E '\[applicator\] mod applied:.*applied=[1-9][0-9]*' "$LOG" 2>/dev/null || true)"
apply_count="$(printf '%s\n' "$applied_lines" | grep -c . || true)"

if [[ "$apply_count" -gt 0 ]]; then
    echo "[cinema] applied mods:"
    printf '%s\n' "$applied_lines" | sed 's/^/  /'
    echo "----------------------------------------"
    echo "[cinema] PASS — $apply_count flat file(s) applied"
    exit 0
fi

echo "[cinema] FAIL — no '[applicator] mod applied:' with applied>0 in ${WAIT_SECS}s"
echo "[cinema] --- applicator/tweakxl log lines (diagnose) ---"
grep -E '\[applicator\]|\[tweakxl\] (mods scanned|yaml applied)' "$LOG" 2>/dev/null | sed 's/^/  /' || echo "  (none — DB may not have populated; raise WAIT_SECS or get past title screen)"
echo "[cinema] If you see 'reject op: flat not found', a record/type binding in"
echo "[cinema] CINEMA_DEMO_FLATS.md was wrong — re-pick per its Fallback note."
exit 1
