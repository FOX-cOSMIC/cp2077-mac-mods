#!/usr/bin/env bash
# test-yaml-parser.sh — build + run the YAML parser on-host test (P1.8).
#
# Unlike the in-game smoke tests (test-slide-capture / test-singleton-access /
# test-tweakdb-access / test-hashmap-lookup / test-value-rw / test-apply-trigger),
# this one is fully on-host: yaml_test runs as a standalone process with
# synthetic YAML fixtures embedded in the test binary.
#
# PASS = yaml_test exits 0 (all assertions hold).

set -uo pipefail

REPO="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="$REPO/build"
TEST_BIN="$BUILD_DIR/src/tweakxl-mac/yaml_test"

echo "[test-yaml] repo  : $REPO"
echo "[test-yaml] build : $BUILD_DIR"

# Build if missing or stale relative to sources.
if [[ ! -x "$TEST_BIN" ]] || [[ "$REPO/src/tweakxl-mac/src/parsers/YAML.cpp" -nt "$TEST_BIN" ]]; then
    echo "[test-yaml] (re)building yaml_test ..."
    cmake -B "$BUILD_DIR" -S "$REPO" -DCMAKE_BUILD_TYPE=Debug 2>&1 | tail -3
    cmake --build "$BUILD_DIR" --target yaml_test 2>&1 | tail -5
fi

if [[ ! -x "$TEST_BIN" ]]; then
    echo "[test-yaml] FAIL: build did not produce $TEST_BIN" >&2
    exit 1
fi

echo "[test-yaml] running ..."
echo "----------------------------------------"
if "$TEST_BIN"; then
    echo "----------------------------------------"
    echo "[test-yaml] PASS"
    exit 0
else
    rc=$?
    echo "----------------------------------------"
    echo "[test-yaml] FAIL (exit $rc)"
    exit "$rc"
fi
