// tweakdb_test.cpp — P1.4 standalone layout test.
//
// The load-bearing checks are compile-time static_asserts in TweakDB.hpp (and
// duplicated here so this TU fails to compile if the layout ever drifts from
// F-015). main() prints the resolved offsets for human inspection and exits 0.
//
// This is a pure layout/offset exerciser — it does NOT touch the live game or
// call VerifyH008 (that needs an injected process; see tools/test-tweakdb-access.sh).

#include "TweakDB.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>

using red4ext_mac::HashMap;
using red4ext_mac::TweakDB;

// ── Compile-time layout guarantees (mirror TweakDB.hpp; redundancy is the point) ──
static_assert(sizeof(TweakDB) == 0x168, "F-013: TweakDB must be exactly 0x168 bytes");
static_assert(sizeof(HashMap) == 0x30,  "HashMap must be 0x30 bytes");
static_assert(offsetof(TweakDB, hashMapA)         == 0x58,  "F-015: hashMapA @ +0x58");
static_assert(offsetof(TweakDB, hashMapB_records) == 0x88,  "F-012: records map @ +0x88");
static_assert(offsetof(TweakDB, hashMapC)         == 0x108, "F-015: hashMapC @ +0x108");
static_assert(offsetof(TweakDB, flatDataBuffer)   == 0x148, "F-015: flat buffer ptr @ +0x148");
// Absolute count offset of the records map (F-012 says +0x90).
static_assert(offsetof(TweakDB, hashMapB_records) + offsetof(HashMap, count) == 0x90,
              "F-012: records count @ +0x90");

int main() {
    std::printf("[tweakdb_test] sizeof(TweakDB)            = 0x%zx (expect 0x168)\n", sizeof(TweakDB));
    std::printf("[tweakdb_test] sizeof(HashMap)            = 0x%zx (expect 0x30)\n",  sizeof(HashMap));
    std::printf("[tweakdb_test] offsetof(hashMapA)         = 0x%zx (expect 0x58)\n",  offsetof(TweakDB, hashMapA));
    std::printf("[tweakdb_test] offsetof(hashMapB_records) = 0x%zx (expect 0x88)\n",  offsetof(TweakDB, hashMapB_records));
    std::printf("[tweakdb_test] offsetof(hashMapC)         = 0x%zx (expect 0x108)\n", offsetof(TweakDB, hashMapC));
    std::printf("[tweakdb_test] offsetof(flatDataBuffer)   = 0x%zx (expect 0x148)\n", offsetof(TweakDB, flatDataBuffer));
    std::printf("[tweakdb_test] records count abs offset   = 0x%zx (expect 0x90)\n",
                offsetof(TweakDB, hashMapB_records) + offsetof(HashMap, count));

    // Runtime re-assert (defensive; the static_asserts above already gate the build).
    bool ok =
        sizeof(TweakDB) == 0x168 &&
        sizeof(HashMap) == 0x30 &&
        offsetof(TweakDB, hashMapA) == 0x58 &&
        offsetof(TweakDB, hashMapB_records) == 0x88 &&
        offsetof(TweakDB, hashMapC) == 0x108 &&
        offsetof(TweakDB, flatDataBuffer) == 0x148;

    std::printf("[tweakdb_test] %s\n", ok ? "PASS: all offsets match F-012/F-013/F-015"
                                          : "FAIL: offset mismatch");
    return ok ? 0 : 1;
}
