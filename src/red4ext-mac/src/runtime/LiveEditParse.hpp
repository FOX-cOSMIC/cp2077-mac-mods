#pragma once
// Pure, dependency-free parse helpers for the live-edit tool (Phase 2). Kept in a
// header so they can be unit-tested in isolation (liveedit_parse_test) without the
// game/mach dependencies of TweakDB.cpp. Logic must match the tool's behaviour.

#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace red4ext_mac {
namespace liveedit {

// Format 16 bytes as a canonical UUID string. `s` must be >= 40 chars.
inline void FormatUUID(const uint8_t u[16], char* s) {
    std::snprintf(s, 40, "%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X",
                  u[0], u[1], u[2], u[3], u[4], u[5], u[6], u[7],
                  u[8], u[9], u[10], u[11], u[12], u[13], u[14], u[15]);
}

// Parse 16 bytes from a hex string; non-hex characters (e.g. dashes) are ignored.
// Returns true iff exactly 16 bytes were read (reads the first 16; ignores any extra).
inline bool ParseHexUUID(const char* s, uint8_t out[16]) {
    int got = 0, hi = -1;
    for (const char* p = s; *p && got < 16; ++p) {
        if (!std::isxdigit((unsigned char)*p)) continue;
        const int v = (*p <= '9') ? (*p - '0') : (std::tolower((unsigned char)*p) - 'a' + 10);
        if (hi < 0) hi = v;
        else { out[got++] = (uint8_t)((hi << 4) | v); hi = -1; }
    }
    return got == 16;
}

// Parse a value token "[i|f]<number>" -> numeric value; sets intRaw for the 'i'/'I'
// prefix (raw-int counter), clears it for 'f'/'F' or no prefix (container/float).
inline double ParseValueToken(const char* tok, bool& intRaw) {
    const char* p = tok;
    intRaw = false;
    if (*p == 'i' || *p == 'I') { intRaw = true; ++p; }
    else if (*p == 'f' || *p == 'F') { intRaw = false; ++p; }
    return std::strtod(p, nullptr);
}

} // namespace liveedit
} // namespace red4ext_mac
