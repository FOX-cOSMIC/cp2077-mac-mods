// hashmap_test.cpp — P1.5 unit test for the hash primitives.
//
// Verifies Fnv1a32 and Crc32 against published test vectors, and the TweakDBID
// packing. This is a pure-logic test (no game, no live struct); the in-game
// hash-function discovery + lookup round-trip is tools/test-hashmap-lookup.sh.

#include "HashMap.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>

using namespace red4ext_mac;

namespace {

int g_fail = 0;

void check_u32(const char* what, uint32_t got, uint32_t want) {
    const bool ok = (got == want);
    std::printf("[hashmap_test] %-28s got=0x%08x want=0x%08x  %s\n",
                what, got, want, ok ? "OK" : "FAIL");
    if (!ok) ++g_fail;
}

uint32_t fnv(const char* s) { return Fnv1a32(s, std::strlen(s)); }
uint32_t crc(const char* s) { return Crc32(s, std::strlen(s)); }

} // namespace

int main() {
    // TweakDBID must stay 8 bytes (binary-format spec §6).
    static_assert(sizeof(TweakDBID) == 8, "TweakDBID must be 8 bytes");

    // ── FNV-1a 32-bit canonical vectors (basis 0x811c9dc5, prime 0x01000193) ──
    check_u32("fnv1a32(\"\")",       fnv(""),       0x811c9dc5u); // empty == offset basis
    check_u32("fnv1a32(\"a\")",      fnv("a"),      0xe40c292cu);
    check_u32("fnv1a32(\"foobar\")", fnv("foobar"), 0xbf9cf968u);

    // ── CRC32 (zlib/ISO-3309, reflected poly 0xEDB88320) check value ──────────
    check_u32("crc32(\"123456789\")", crc("123456789"), 0xcbf43926u); // standard CRC-32 check
    check_u32("crc32(\"\")",          crc(""),          0x00000000u);

    // ── TweakDBID packing: nameHash at +0, length at +4, 24-bit offset at +5 ──
    {
        TweakDBID id;
        std::memset(&id, 0, sizeof(id));
        id.nameHash     = 0xdeadbeefu;
        id.nameLength   = 0x11;
        id.tdbOffset[0] = 0x33; // LE: low byte
        id.tdbOffset[1] = 0x22;
        id.tdbOffset[2] = 0x01;
        uint8_t raw[8];
        std::memcpy(raw, &id, sizeof(raw));
        // Expect little-endian nameHash then length then offset bytes.
        uint32_t le = (uint32_t)raw[0] | ((uint32_t)raw[1] << 8) |
                      ((uint32_t)raw[2] << 16) | ((uint32_t)raw[3] << 24);
        check_u32("TweakDBID.nameHash@+0", le, 0xdeadbeefu);
        check_u32("TweakDBID.length@+4",   raw[4], 0x11u);
        uint32_t off = (uint32_t)raw[5] | ((uint32_t)raw[6] << 8) | ((uint32_t)raw[7] << 16);
        check_u32("TweakDBID.offset@+5",   off, 0x012233u);
    }

    std::printf("[hashmap_test] %s\n", g_fail == 0 ? "PASS: all hash vectors match"
                                                   : "FAIL: hash mismatch");
    return g_fail == 0 ? 0 : 1;
}
