// TweakDB.cpp — P1.4 implementation: H-008 runtime verification.
//
// The typed struct + read-only accessors live entirely in TweakDB.hpp (inline,
// header-only — they're just offset arithmetic). The only out-of-line code is
// VerifyH008(), which reads the two candidate maps' entry counts from the live
// instance and prints a decisive, machine-parseable verdict.
//
// Safety: every read of the live struct goes through mach_vm_read_overwrite, so
// a null/partially-constructed/unmapped instance yields 0 rather than a crash
// (same model as SingletonAccess.cpp — SOUL.md: "never crash on bad pointers").
//
// No mutation, no mprotect, no __TEXT writes (FA-001).

#include "TweakDB.hpp"

#include <cstdarg>
#include <cstdio>
#include <mutex>

#include <mach/mach.h>
#include <mach/mach_vm.h>

namespace red4ext_mac {
namespace {

constexpr const char* kLogPath = "/tmp/red4ext-mac.log";

// Same dual-sink logging as Loader.cpp: dedicated file (stable grep target for
// the smoke test) + stderr.
void log_line(const char* fmt, ...) {
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    if (FILE* f = std::fopen(kLogPath, "a")) {
        std::fputs(buf, f);
        std::fputc('\n', f);
        std::fclose(f);
    }
    std::fprintf(stderr, "%s\n", buf);
    std::fflush(stderr);
}

// Safe 4-byte read from an arbitrary process address. Returns false (and leaves
// out untouched) if the read faults or is short. Never dereferences directly.
bool SafeReadU32(uintptr_t addr, uint32_t* out) {
    if (addr == 0) return false;
    uint32_t value = 0;
    mach_vm_size_t got = 0;
    kern_return_t kr = mach_vm_read_overwrite(
        mach_task_self(),
        (mach_vm_address_t)addr,
        (mach_vm_size_t)sizeof(value),
        (mach_vm_address_t)&value,
        &got);
    if (kr != KERN_SUCCESS || got != sizeof(value)) return false;
    *out = value;
    return true;
}

// Read a HashMap's count field (+0x08) from the live struct, safely.
// 'mapOffset' is the map's offset within TweakDB (e.g. 0x58, 0x108).
uint32_t ReadMapCount(const TweakDB* db, size_t mapOffset) {
    uint32_t v = 0;
    const uintptr_t base = reinterpret_cast<uintptr_t>(db);
    SafeReadU32(base + mapOffset + offsetof(HashMap, count), &v);
    return v;
}

// Read the alternate +0x0c slot (bucketCount) of a map — used only for the
// F-015 map-C count-offset cross-check (see below).
uint32_t ReadMapSlot0c(const TweakDB* db, size_t mapOffset) {
    uint32_t v = 0;
    const uintptr_t base = reinterpret_cast<uintptr_t>(db);
    SafeReadU32(base + mapOffset + offsetof(HashMap, bucketCount), &v);
    return v;
}

std::once_flag g_h008_once;

void DoVerifyH008(const TweakDB* db) {
    if (db == nullptr) {
        log_line("[tweakdb] H-008 verification: SKIPPED (db=null)");
        return;
    }

    // Structurally-confirmed count slot (+0x08) for both candidate maps.
    const uint32_t n1 = ReadMapCount(db, offsetof(TweakDB, hashMapA)); // +0x58 (flats?)
    const uint32_t n2 = ReadMapCount(db, offsetof(TweakDB, hashMapC)); // +0x108 (queries?)

    // Verdict: flats outnumber queries by orders of magnitude (H-008). Require
    // a 10x margin so a not-yet-fully-populated DB reads as indeterminate
    // rather than producing a false confirmation.
    const char* verdict;
    if (n1 == 0 && n2 == 0) {
        verdict = "indeterminate"; // DB not populated yet — counts both zero
    } else if (n1 > 10u * n2) {
        verdict = "flats-is-A";    // +0x58 dominates → confirms H-008
    } else if (n2 > 10u * n1) {
        verdict = "flats-is-C";    // +0x108 dominates → REFUTES H-008 (swap offsets)
    } else {
        verdict = "indeterminate"; // counts too close — DB may be mid-populate
    }

    log_line("[tweakdb] H-008 verification: mapA(+0x58).count=%u mapC(+0x108).count=%u verdict=%s",
             n1, n2, verdict);

    // Cross-check the one F-015 ambiguity: F-015 lists map C's count at +0x114
    // (= block+0x0c, the bucketCount slot) whereas F-012/F-019 place count at
    // block+0x08 for the other two maps. Log both candidate slots for every map
    // so the researcher can resolve the discrepancy from in-game evidence
    // without another build. (We do NOT silently pick one for map C — the
    // verdict above uses the structurally-confirmed +0x08 slot.)
    const uint32_t a08 = n1;
    const uint32_t a0c = ReadMapSlot0c(db, offsetof(TweakDB, hashMapA));
    const uint32_t r08 = ReadMapCount(db, offsetof(TweakDB, hashMapB_records));
    const uint32_t r0c = ReadMapSlot0c(db, offsetof(TweakDB, hashMapB_records));
    const uint32_t c08 = n2;
    const uint32_t c0c = ReadMapSlot0c(db, offsetof(TweakDB, hashMapC));
    log_line("[tweakdb] H-008 raw: mapA{@08=%u,@0c=%u} records{@08=%u,@0c=%u} mapC{@08=%u,@0c=%u} "
             "(F-015 lists mapC.count@0c=+0x114; F-012/F-019 use @08)",
             a08, a0c, r08, r0c, c08, c0c);

    // Also surface the value-buffer state — non-zero size after load is the
    // P1.4 acceptance signal alongside the records count.
    uint32_t bufSize = 0;
    SafeReadU32(reinterpret_cast<uintptr_t>(db) + offsetof(TweakDB, flatDataBufferSize), &bufSize);
    log_line("[tweakdb] state: records.count=%u valueBufferSize=%u", r08, bufSize);
}

} // namespace

void VerifyH008(const TweakDB* db) {
    std::call_once(g_h008_once, [db] { DoVerifyH008(db); });
}

} // namespace red4ext_mac
