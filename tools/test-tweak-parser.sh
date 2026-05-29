#!/usr/bin/env bash
# tools/test-tweak-parser.sh — smoke test for P1.7 (scanner) + P1.9 (.tweak parser).
#
# 1. Builds the tweakxl target (libtweakxl.dylib) + scanner_test.
# 2. Finds a real .tweak in reference/example-mods/, else generates a synthetic one.
# 3. Runs the mod scanner over a synthetic tree (verifies priority tiers + types).
# 4. Links a tiny driver against libtweakxl.dylib and parses the .tweak file.
#
# PASS if the scanner discovers the expected files AND ParseTweakFile returns non-null.

set -euo pipefail

REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="$REPO/build"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

echo "==> [1/4] Building tweakxl + scanner_test"
cmake -S "$REPO" -B "$BUILD" >/dev/null
cmake --build "$BUILD" --target tweakxl scanner_test >/dev/null

DYLIB="$BUILD/src/tweakxl-mac/libtweakxl.dylib"
SCANNER="$BUILD/src/tweakxl-mac/scanner_test"
[[ -f "$DYLIB"   ]] || { echo "FAIL: libtweakxl.dylib not built"; exit 1; }
[[ -f "$SCANNER" ]] || { echo "FAIL: scanner_test not built"; exit 1; }

echo "==> [2/4] Locating a .tweak fixture"
TWEAK_FILE="$(find "$REPO/reference/example-mods" -iname '*.tweak' 2>/dev/null | head -1 || true)"
if [[ -z "${TWEAK_FILE:-}" ]]; then
    echo "    no .tweak in example-mods/ — generating a synthetic one"
    TWEAK_FILE="$TMP/sample.tweak"
    cat > "$TWEAK_FILE" <<'TWEAK'
package SmokeTest

[EP1] Items.MyGun : Items.Preset_Ajax_Default {
    int   damageMin = 80;
    float damageMax = 120.5f;
    string label    = "hello world";
    tags += ["Tier5", "Iconic"];
}
TWEAK
fi
echo "    using: $TWEAK_FILE"

echo "==> [3/4] Scanner: priority tiers + extension filter"
SCANROOT="$TMP/mods"
mkdir -p "$SCANROOT/sub"
cp "$TWEAK_FILE"            "$SCANROOT/sub/normal.tweak"
printf 'A.b: 1\n'        > "$SCANROOT/_first.yaml"
printf 'A.b: 1\n'        > "$SCANROOT/^last.yml"
printf 'ignore me\n'     > "$SCANROOT/readme.txt"      # must be skipped
printf 'A.b: 1\n'        > "$SCANROOT/UPPER.YAML"       # case-insensitive ext
SCAN_OUT="$("$SCANNER" "$SCANROOT")"
echo "$SCAN_OUT" | sed 's/^/    /'
COUNT="$(echo "$SCAN_OUT" | grep -c . || true)"
[[ "$COUNT" -eq 4 ]] || { echo "FAIL: expected 4 discovered mods, got $COUNT (readme.txt must be skipped)"; exit 1; }
# First line must be the tier-0 file, last must be tier-2.
[[ "$(echo "$SCAN_OUT" | head -1 | cut -d' ' -f1)" == "0" ]] || { echo "FAIL: first entry is not tier 0"; exit 1; }
[[ "$(echo "$SCAN_OUT" | tail -1 | cut -d' ' -f1)" == "2" ]] || { echo "FAIL: last entry is not tier 2"; exit 1; }
echo "    scanner OK (4 files, tiers ordered, readme.txt skipped, UPPER.YAML matched)"

echo "==> [4/4] Parser: ParseTweakFile must return non-null"
DRIVER="$TMP/driver.cpp"
cat > "$DRIVER" <<'CPP'
#include "Tweak.hpp"
#include <cstdio>
int main(int argc, char** argv) {
    auto h = tweakxl_mac::ParseTweakFile(argv[1]);
    if (!h) { std::printf("PARSE: null\n"); return 1; }
    std::printf("PARSE: ok\n");
    return 0;
}
CPP
clang++ -std=c++17 "$DRIVER" -o "$TMP/driver" \
    -I"$REPO/src/tweakxl-mac/src/parsers" \
    -L"$BUILD/src/tweakxl-mac" -ltweakxl \
    -Wl,-rpath,"$BUILD/src/tweakxl-mac"

if "$TMP/driver" "$TWEAK_FILE"; then
    echo "    parser OK (non-null source)"
else
    echo "FAIL: ParseTweakFile returned null for $TWEAK_FILE"; exit 1
fi

# Negative check: a malformed .tweak must return null (logged, not crash).
BAD="$TMP/bad.tweak"
printf 'package X\n  @@@ not valid {' > "$BAD"
if "$TMP/driver" "$BAD"; then
    echo "FAIL: malformed .tweak unexpectedly parsed non-null"; exit 1
else
    echo "    parser correctly rejected malformed .tweak (null + logged)"
fi

echo
echo "PASS: scanner + .tweak parser smoke test green"
