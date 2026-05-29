#!/usr/bin/env bash
# test-applicator.sh — build + run the P1.10a applicator on-host test.
#
# Like test-yaml-parser.sh this is fully on-host: applicator_test runs as a
# standalone process and never needs the game. It builds synthetic ModFiles in
# code and applies them to an invalid (zeroed) TweakDB*, asserting the
# skip/reject/rollback branch logic. Real in-game validation is P1.11's job.
#
# PASS = applicator_test exits 0 (all assertions hold).

set -uo pipefail

REPO="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="$REPO/build"
TEST_BIN="$BUILD_DIR/src/tweakxl-mac/applicator_test"
SRC="$REPO/src/tweakxl-mac/src/applicator/Applicator.cpp"

echo "[test-applicator] repo  : $REPO"
echo "[test-applicator] build : $BUILD_DIR"

# Build if missing or stale relative to the applicator source.
if [[ ! -x "$TEST_BIN" ]] || [[ "$SRC" -nt "$TEST_BIN" ]]; then
    echo "[test-applicator] (re)building applicator_test ..."
    cmake -B "$BUILD_DIR" -S "$REPO" -DCMAKE_BUILD_TYPE=Debug 2>&1 | tail -3
    cmake --build "$BUILD_DIR" --target applicator_test 2>&1 | tail -8
fi

if [[ ! -x "$TEST_BIN" ]]; then
    echo "[test-applicator] FAIL: build did not produce $TEST_BIN" >&2
    exit 1
fi

echo "[test-applicator] running ..."
echo "----------------------------------------"
if "$TEST_BIN"; then
    echo "----------------------------------------"
    echo "[test-applicator] PASS"
    exit 0
else
    rc=$?
    echo "----------------------------------------"
    echo "[test-applicator] FAIL (exit $rc)"
    exit "$rc"
fi
