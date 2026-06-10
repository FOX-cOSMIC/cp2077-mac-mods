// Unit tests for the live-edit pure parse helpers (LiveEditParse.hpp).
// Header-only, no game/mach dependencies — runs anywhere. P2.6.
#include "LiveEditParse.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>

using red4ext_mac::liveedit::ParseHexUUID;
using red4ext_mac::liveedit::FormatUUID;
using red4ext_mac::liveedit::ParseValueToken;

static int g_fail = 0;
#define CHECK(cond, msg) do { if (!(cond)) { std::printf("FAIL: %s\n", (msg)); ++g_fail; } } while (0)

static bool uuidEq(const uint8_t* a, const uint8_t* b) { return std::memcmp(a, b, 16) == 0; }

int main() {
    // ── ParseHexUUID ────────────────────────────────────────────────────────
    const uint8_t kExpected[16] = {
        0xA6,0x65,0x6A,0xDC, 0xFB,0xE2, 0x36,0xA4, 0x9B,0x9D, 0xB4,0xA9,0xDE,0x64,0x50,0x89
    };
    uint8_t out[16];

    CHECK(ParseHexUUID("A6656ADC-FBE2-36A4-9B9D-B4A9DE645089", out) && uuidEq(out, kExpected),
          "canonical dashed UUID parses to bytes");
    CHECK(ParseHexUUID("a6656adcfbe236a49b9db4a9de645089", out) && uuidEq(out, kExpected),
          "lowercase no-dash UUID parses");
    CHECK(ParseHexUUID("A6:65:6A:DC:FB:E2:36:A4:9B:9D:B4:A9:DE:64:50:89", out) && uuidEq(out, kExpected),
          "colon-separated UUID parses (non-hex ignored)");
    CHECK(!ParseHexUUID("A6656ADC", out), "too-short hex is rejected");
    CHECK(!ParseHexUUID("", out), "empty string rejected");
    CHECK(!ParseHexUUID("zz-not-a-uuid", out), "garbage rejected");
    CHECK(ParseHexUUID("A6656ADCFBE236A49B9DB4A9DE645089FFFF", out) && uuidEq(out, kExpected),
          "extra trailing hex ignored after the first 16 bytes");

    // ── FormatUUID + round-trip ─────────────────────────────────────────────
    char s[40];
    FormatUUID(kExpected, s);
    CHECK(std::strcmp(s, "A6656ADC-FBE2-36A4-9B9D-B4A9DE645089") == 0, "FormatUUID canonical output");
    uint8_t rt[16];
    CHECK(ParseHexUUID(s, rt) && uuidEq(rt, kExpected), "FormatUUID -> ParseHexUUID round-trip");

    // ── ParseValueToken ─────────────────────────────────────────────────────
    bool ir = false;
    CHECK(ParseValueToken("i310915", ir) == 310915.0 && ir == true,  "i-prefix = raw int");
    CHECK(ParseValueToken("I42", ir)     == 42.0     && ir == true,  "I-prefix (uppercase) = raw int");
    CHECK(ParseValueToken("f1.5", ir)    == 1.5      && ir == false, "f-prefix = container/float");
    CHECK(ParseValueToken("F2.25", ir)   == 2.25     && ir == false, "F-prefix = container/float");
    CHECK(ParseValueToken("100", ir)     == 100.0    && ir == false, "bare number = container/float");
    CHECK(ParseValueToken("0", ir)       == 0.0      && ir == false, "zero parses to 0 (refused upstream)");
    CHECK(ParseValueToken("1000000", ir) == 1000000.0 && ir == false, "large bare number");

    if (g_fail == 0) { std::printf("liveedit_parse_test: ALL PASSED\n"); return 0; }
    std::printf("liveedit_parse_test: %d FAILURE(S)\n", g_fail);
    return 1;
}
