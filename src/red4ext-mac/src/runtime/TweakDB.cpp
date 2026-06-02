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
#include "Symbols.hpp"          // StaticToRuntime — F-031 per-type FlatValue vtables

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <dlfcn.h>
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

// ═══════════════════════════════════════════════════════════════════════════
// P1.6 — flat value buffer R/W + Phase A layout discovery.
// ═══════════════════════════════════════════════════════════════════════════

namespace {

constexpr uint32_t kEmptyBucket = 0xFFFFFFFFu;
constexpr size_t   kEntryHashOff = 0x04;
constexpr size_t   kEntryKeyOff  = 0x08;

// Generic safe reads (the P1.4 block only had U32).
bool SafeRead(uintptr_t addr, void* out, size_t n) {
    if (addr == 0) return false;
    mach_vm_size_t got = 0;
    kern_return_t kr = mach_vm_read_overwrite(
        mach_task_self(), (mach_vm_address_t)addr, (mach_vm_size_t)n,
        (mach_vm_address_t)out, &got);
    return kr == KERN_SUCCESS && got == n;
}
bool SafeReadU64(uintptr_t a, uint64_t* v) { return SafeRead(a, v, sizeof(*v)); }
bool SafeReadPtr(uintptr_t a, uintptr_t* v) { return SafeRead(a, v, sizeof(*v)); }

// Safe write: mach_vm_write returns an error (not a signal) on a read-only or
// unmapped target, so a bad write fails cleanly instead of crashing the game.
bool SafeWrite(uintptr_t addr, const void* data, size_t n) {
    if (addr == 0) return false;
    kern_return_t kr = mach_vm_write(
        mach_task_self(), (mach_vm_address_t)addr,
        (vm_offset_t)data, (mach_msg_type_number_t)n);
    return kr == KERN_SUCCESS;
}

// A pointer that plausibly points into slid game/heap memory (filters 0/garbage).
bool PlausiblePtr(uintptr_t p) {
    return p >= 0x100000000ull && p < 0x0001000000000000ull;
}

// ── Global discovered layout (guarded; defaults to Unknown/unconfirmed) ──────
std::mutex g_layoutMtx;
FlatLayout g_layout;   // .confirmed == false until VerifyFlatEntry validates one

// Snapshot of a flats map's load-bearing fields.
struct FlatMapView {
    uint32_t  bucketCount = 0, stride = 0, count = 0;
    uintptr_t bucketArray = 0, entries = 0;
};
bool ReadFlatsMap(const TweakDB* db, FlatMapView* mv) {
    const HashMap* m = GetFlatsMapCandidate(db);   // +0x58 (H-008 confirmed flats)
    if (!m) return false;
    const uintptr_t mb = reinterpret_cast<uintptr_t>(m);
    if (!SafeReadU32(mb + offsetof(HashMap, bucketCount), &mv->bucketCount)) return false;
    if (!SafeReadU32(mb + offsetof(HashMap, stride),      &mv->stride))      return false;
    if (!SafeReadU32(mb + offsetof(HashMap, count),       &mv->count))       return false;
    if (!SafeReadPtr(mb + offsetof(HashMap, bucketArray), &mv->bucketArray)) return false;
    if (!SafeReadPtr(mb + offsetof(HashMap, entries),     &mv->entries))     return false;
    return mv->bucketCount && mv->stride && mv->bucketArray && mv->entries;
}

void ReadBuffer(const TweakDB* db, uintptr_t* base, uint32_t* size) {
    const uintptr_t dbA = reinterpret_cast<uintptr_t>(db);
    *base = 0; *size = 0;
    SafeReadPtr(dbA + offsetof(TweakDB, flatDataBuffer),     base);
    SafeReadU32(dbA + offsetof(TweakDB, flatDataBufferSize), size);
}

// Given an entry and a layout, resolve the value-OBJECT base address (the thing
// whose +valueDataOff holds the scalar). Reads the entry's STORED key, so the
// caller's id need not carry a tdbOffset.
bool ResolveValueObj(const TweakDB* db, uintptr_t entry,
                     const FlatLayout& lay, uintptr_t* out) {
    uintptr_t bufBase = 0; uint32_t bufSize = 0;
    ReadBuffer(db, &bufBase, &bufSize);

    switch (lay.source) {
        case FlatValueSource::BufferViaTdbOffset: {
            uint64_t key = 0;
            if (!SafeReadU64(entry + kEntryKeyOff, &key)) return false;
            const uint32_t off = (uint32_t)((key >> 40) & 0xFFFFFFu); // key bytes 5..7
            if (!bufBase || off == 0 || off >= bufSize) return false;
            *out = bufBase + off;
            return true;
        }
        case FlatValueSource::BufferViaEntryOffset: {
            uint32_t off = 0;
            if (!SafeReadU32(entry + lay.entryFieldOff, &off)) return false;
            if (!bufBase || off == 0 || off >= bufSize) return false;
            *out = bufBase + off;
            return true;
        }
        case FlatValueSource::FlatValuePtrAtEntry: {
            uintptr_t p = 0;
            if (!SafeReadPtr(entry + lay.entryFieldOff, &p) || !PlausiblePtr(p)) return false;
            *out = p;
            return true;
        }
        case FlatValueSource::InlineEntry:
            *out = entry + lay.entryFieldOff;
            return true;
        case FlatValueSource::Unknown:
        default:
            return false;
    }
}

// Collect the head entries of up to `maxN` distinct non-empty buckets.
int CollectSampleEntries(const FlatMapView& mv, uintptr_t* outEntries, int maxN) {
    int n = 0;
    for (uint32_t i = 0; i < mv.bucketCount && n < maxN; ++i) {
        uint32_t idx = kEmptyBucket;
        if (SafeReadU32(mv.bucketArray + (uint64_t)i * 4, &idx) && idx != kEmptyBucket)
            outEntries[n++] = mv.entries + (uint64_t)idx * mv.stride;
    }
    return n;
}

std::once_flag g_flatEntry_once;

// Round-trip R/W on a chosen sample, restoring the original. Logs a verdict.
void RunValueRwSelfTest(const TweakDB* db, const uintptr_t* samples, int nSamples);

void DoVerifyFlatEntry(const TweakDB* db) {
    if (!db) { log_line("[flat-layout] verdict: skipped (db=null)"); return; }

    FlatMapView mv;
    if (!ReadFlatsMap(db, &mv)) {
        log_line("[flat-layout] verdict: skipped (flats map unreadable)");
        return;
    }
    uintptr_t bufBase = 0; uint32_t bufSize = 0;
    ReadBuffer(db, &bufBase, &bufSize);
    log_line("[flat-layout] flats: count=%u stride=0x%x bufferBase=%p bufferSize=%u",
             mv.count, mv.stride, (void*)bufBase, bufSize);

    uintptr_t samples[6];
    const int nS = CollectSampleEntries(mv, samples, 6);
    if (nS == 0) { log_line("[flat-layout] verdict: skipped (no non-empty buckets)"); return; }

    // Per-sample raw dump + per-hypothesis plausibility tally.
    int okTdb = 0, okEntryOff = 0, okPtr10 = 0, okPtr18 = 0;
    for (int i = 0; i < nS; ++i) {
        const uintptr_t e = samples[i];
        uint32_t storedHash = 0; uint64_t key = 0;
        SafeReadU32(e + kEntryHashOff, &storedHash);
        SafeReadU64(e + kEntryKeyOff, &key);
        const uint32_t nameHash = (uint32_t)(key & 0xFFFFFFFFu);
        const uint8_t  len      = (uint8_t)((key >> 32) & 0xFFu);
        const uint32_t tdbOff   = (uint32_t)((key >> 40) & 0xFFFFFFu);

        // Candidate value objects.
        uintptr_t vTdb = (bufBase && tdbOff && tdbOff < bufSize) ? bufBase + tdbOff : 0;
        uint32_t  eOff = 0; SafeReadU32(e + 0x10, &eOff);
        uintptr_t vEntryOff = (bufBase && eOff && eOff < bufSize) ? bufBase + eOff : 0;
        uintptr_t p10 = 0, p18 = 0;
        SafeReadPtr(e + 0x10, &p10);
        SafeReadPtr(e + 0x18, &p18);

        // For the tdbOffset candidate, peek the value object: [vft][value@+8].
        uint64_t vftTdb = 0; uint32_t raw32 = 0;
        if (vTdb) { SafeReadU64(vTdb, &vftTdb); SafeReadU32(vTdb + 0x08, &raw32); }

        if (vTdb && PlausiblePtr((uintptr_t)vftTdb)) ++okTdb;
        if (vEntryOff) ++okEntryOff;
        if (PlausiblePtr(p10)) ++okPtr10;
        if (PlausiblePtr(p18)) ++okPtr18;

        float asF; std::memcpy(&asF, &raw32, 4);
        log_line("[flat-layout] sample[%d]: hash=0x%08x key=<TweakDBID(0x%08x,len=%u,off=0x%06x)> "
                 "viaTdb.vft=0x%llx viaTdb.raw32=0x%08x asI32=%d asF32=%g "
                 "entry+0x10.i32=%u ptr@0x10=%p ptr@0x18=%p",
                 i, storedHash, nameHash, len, tdbOff,
                 (unsigned long long)vftTdb, raw32, (int)raw32, (double)asF,
                 eOff, (void*)p10, (void*)p18);
    }

    // Decide the layout: prefer the hypothesis plausible across the most samples.
    FlatLayout chosen;
    const int majority = (nS + 1) / 2;
    if (okTdb >= majority) {
        chosen.source = FlatValueSource::BufferViaTdbOffset;
        chosen.valueDataOff = 0x08;
    } else if (okPtr18 >= majority) {
        chosen.source = FlatValueSource::FlatValuePtrAtEntry;  // F-019 payload @ +0x18
        chosen.entryFieldOff = 0x18; chosen.valueDataOff = 0x08;
    } else if (okPtr10 >= majority) {
        chosen.source = FlatValueSource::FlatValuePtrAtEntry;
        chosen.entryFieldOff = 0x10; chosen.valueDataOff = 0x08;
    } else if (okEntryOff >= majority) {
        chosen.source = FlatValueSource::BufferViaEntryOffset;
        chosen.entryFieldOff = 0x10; chosen.valueDataOff = 0x08;
    } else {
        chosen.source = FlatValueSource::Unknown;
    }
    chosen.confirmed = (chosen.source != FlatValueSource::Unknown);

    { std::lock_guard<std::mutex> lk(g_layoutMtx); g_layout = chosen; }

    const char* srcName =
        chosen.source == FlatValueSource::BufferViaTdbOffset   ? "buffer-via-tdbOffset" :
        chosen.source == FlatValueSource::BufferViaEntryOffset ? "buffer-via-entryOffset" :
        chosen.source == FlatValueSource::FlatValuePtrAtEntry  ? "flatValue-ptr-at-entry" :
        chosen.source == FlatValueSource::InlineEntry          ? "inline-entry" : "unknown";
    log_line("[flat-layout] verdict: %s type-tag-offset:+0x00(vft) value-data-offset:+0x%x "
             "buffer-offset-source:%s tally{tdb=%d ptr@0x18=%d ptr@0x10=%d entryOff=%d}/%d",
             srcName, chosen.valueDataOff,
             chosen.source == FlatValueSource::BufferViaTdbOffset   ? "tdbOffset" :
             chosen.source == FlatValueSource::BufferViaEntryOffset ? "entry+0x10" :
             chosen.source == FlatValueSource::FlatValuePtrAtEntry  ? "entry-ptr" : "N/A",
             okTdb, okPtr18, okPtr10, okEntryOff, nS);

    // ── Flats-map hash function (tdbOffset is non-zero here → 8B vs 5B distinct).
    {
        const uintptr_t e = samples[0];
        uint32_t storedHash = 0; uint8_t kb[8] = {0};
        SafeReadU32(e + kEntryHashOff, &storedHash);
        SafeRead(e + kEntryKeyOff, kb, 8);
        const uint32_t cF8 = Fnv1a32(kb, 8), cF5 = Fnv1a32(kb, 5), cC8 = Crc32(kb, 8);
        const uint32_t cName = (uint32_t)kb[0] | ((uint32_t)kb[1] << 8) |
                               ((uint32_t)kb[2] << 16) | ((uint32_t)kb[3] << 24);
        HashMode    fmode = HashMode::Unknown;
        const char* m     = "unknown";
        if      (storedHash == cF8)   { fmode = HashMode::Fnv1a8B;        m = "fnv1a-8B"; }
        else if (storedHash == cF5)   { fmode = HashMode::Fnv1a5B;        m = "fnv1a-5B"; }
        else if (storedHash == cC8)   { fmode = HashMode::Crc32_8B;       m = "crc32-8B"; }
        else if (storedHash == cName) { fmode = HashMode::NameHashDirect; m = "nameHash-direct"; }

        // P1.6b: feed the verdict into the per-map state so flats lookups use it.
        // If nothing matched, fall back to nameHash-direct (the in-game finding).
        SetFlatsHashMode(fmode == HashMode::Unknown ? HashMode::NameHashDirect : fmode);

        log_line("[hashmap] flats-map hash-function: %s stored=0x%08x "
                 "fnv8=0x%08x fnv5=0x%08x crc8=0x%08x name=0x%08x",
                 m, storedHash, cF8, cF5, cC8, cName);
    }

    if (chosen.confirmed)
        RunValueRwSelfTest(db, samples, nS);
    else
        log_line("[flat-rw] verdict: skipped (layout unconfirmed)");
}

void RunValueRwSelfTest(const TweakDB* db, const uintptr_t* samples, int nSamples) {
    const FlatLayout lay = GetFlatLayout();

    // Prefer a sample whose value decodes as a small non-negative int — likely a
    // genuine Int32/enum flat, minimizing risk if anything reads it mid-test.
    uintptr_t chosenEntry = 0; uint32_t chosenRaw = 0; uint64_t chosenKey = 0;
    for (int i = 0; i < nSamples; ++i) {
        uintptr_t vobj = 0;
        if (!ResolveValueObj(db, samples[i], lay, &vobj)) continue;
        uint32_t raw = 0;
        if (!SafeReadU32(vobj + lay.valueDataOff, &raw)) continue;
        uint64_t key = 0; SafeReadU64(samples[i] + kEntryKeyOff, &key);
        if (chosenEntry == 0) { chosenEntry = samples[i]; chosenRaw = raw; chosenKey = key; }
        if (raw <= 1000000u) { chosenEntry = samples[i]; chosenRaw = raw; chosenKey = key; break; }
    }
    if (chosenEntry == 0) {
        log_line("[flat-rw] verdict: fail (no resolvable sample value)");
        return;
    }

    // Build the compact lookup key (offset stripped — exercises the by-name path).
    TweakDBID id;
    std::memset(&id, 0, sizeof(id));
    id.nameHash   = (uint32_t)(chosenKey & 0xFFFFFFFFu);
    id.nameLength = (uint8_t)((chosenKey >> 32) & 0xFFu);

    auto rd = ReadFlat(db, id);
    log_line("[flat-rw] read id=<TweakDBID(0x%08x,len=%u)> found=%s raw=0x%08x",
             id.nameHash, id.nameLength, rd ? "yes" : "no", chosenRaw);
    if (!rd) { log_line("[flat-rw] verdict: fail (ReadFlat miss)"); return; }

    const uint32_t newRaw = chosenRaw + 1u;
    FlatValue nv; nv.type = FlatType::Int32; nv.value = (int32_t)newRaw;
    const bool wrote = WriteFlat(const_cast<TweakDB*>(db), id, nv);

    auto rd2 = ReadFlat(db, id);
    uint32_t after = 0;
    if (rd2 && std::holds_alternative<uint32_t>(rd2->value)) after = std::get<uint32_t>(rd2->value);
    const bool match = wrote && rd2 && (after == newRaw);
    log_line("[flat-rw] write id=<0x%08x> old=0x%08x new=0x%08x result=%s reread=0x%08x match=%s",
             id.nameHash, chosenRaw, newRaw, wrote ? "ok" : "fail", after, match ? "yes" : "no");

    // Restore the original value so game state is left untouched.
    FlatValue rv; rv.type = FlatType::Int32; rv.value = (int32_t)chosenRaw;
    const bool restored = WriteFlat(const_cast<TweakDB*>(db), id, rv);
    log_line("[flat-rw] restore old=0x%08x result=%s", chosenRaw, restored ? "ok" : "fail");

    log_line("[flat-rw] verdict: %s", match ? "pass" : "fail");
}

} // namespace

FlatLayout GetFlatLayout() {
    std::lock_guard<std::mutex> lk(g_layoutMtx);
    return g_layout;
}

std::optional<FlatValue> ReadFlat(const TweakDB* db, TweakDBID id) {
    if (!db) return std::nullopt;
    const FlatLayout lay = GetFlatLayout();
    if (!lay.confirmed) return std::nullopt;

    const uint8_t* entryP = Lookup(GetFlatsMapCandidate(db), id, GetFlatsHashMode());
    if (!entryP) return std::nullopt;

    uintptr_t vobj = 0;
    if (!ResolveValueObj(db, reinterpret_cast<uintptr_t>(entryP), lay, &vobj))
        return std::nullopt;

    uint32_t raw = 0;
    if (!SafeReadU32(vobj + lay.valueDataOff, &raw)) return std::nullopt;

    // The vft→FlatType map is a pending follow-up; report the raw 4 bytes as an
    // untyped uint32 so callers (who already know the declared type) can use it.
    FlatValue fv;
    fv.type  = FlatType::Unknown;
    fv.value = raw;
    return fv;
}

bool WriteFlat(TweakDB* db, TweakDBID id, FlatValue newValue) {
    if (!db) return false;
    const FlatLayout lay = GetFlatLayout();
    if (!lay.confirmed) {
        log_line("[flat-write] REFUSED id=<0x%08x>: flat layout unconfirmed "
                 "(run VerifyFlatEntry first)", id.nameHash);
        return false;
    }
    if (!IsScalarFlatType(newValue.type)) {
        log_line("[flat-write] REFUSED id=<0x%08x>: type %s not supported at this "
                 "layer (scalar only; String/CName/arrays are P1.6+/P1.10)",
                 id.nameHash, FlatTypeName(newValue.type));
        return false;
    }

    // Marshal the caller-declared scalar into bytes (NO coercion: the active
    // variant alternative must agree with the declared type).
    uint8_t bytes[4] = {0};
    uint32_t width = ScalarFlatTypeSize(newValue.type);
    switch (newValue.type) {
        case FlatType::Int32: {
            int32_t v;
            if      (std::holds_alternative<int32_t>(newValue.value))  v = std::get<int32_t>(newValue.value);
            else if (std::holds_alternative<uint32_t>(newValue.value)) v = (int32_t)std::get<uint32_t>(newValue.value);
            else { log_line("[flat-write] REFUSED id=<0x%08x>: Int32 value variant mismatch", id.nameHash); return false; }
            std::memcpy(bytes, &v, 4);
            break;
        }
        case FlatType::Float: {
            if (!std::holds_alternative<float>(newValue.value)) {
                log_line("[flat-write] REFUSED id=<0x%08x>: Float value variant mismatch", id.nameHash); return false;
            }
            float v = std::get<float>(newValue.value);
            std::memcpy(bytes, &v, 4);
            break;
        }
        case FlatType::Bool: {
            if (!std::holds_alternative<bool>(newValue.value)) {
                log_line("[flat-write] REFUSED id=<0x%08x>: Bool value variant mismatch", id.nameHash); return false;
            }
            bytes[0] = std::get<bool>(newValue.value) ? 1 : 0;
            break;
        }
        default:
            return false;
    }

    const uint8_t* entryP = Lookup(GetFlatsMapCandidate(db), id, GetFlatsHashMode());
    if (!entryP) { log_line("[flat-write] REFUSED id=<0x%08x>: not found in flats map", id.nameHash); return false; }

    uintptr_t vobj = 0;
    if (!ResolveValueObj(db, reinterpret_cast<uintptr_t>(entryP), lay, &vobj)) {
        log_line("[flat-write] REFUSED id=<0x%08x>: could not resolve value object", id.nameHash);
        return false;
    }
    const uintptr_t valueAddr = vobj + lay.valueDataOff;

    // Read old bytes (for the audit log + atomicity: confirm the slot is live).
    uint32_t oldRaw = 0;
    if (!SafeReadU32(valueAddr, &oldRaw)) {
        log_line("[flat-write] REFUSED id=<0x%08x>: value slot unreadable @%p", id.nameHash, (void*)valueAddr);
        return false;
    }

    if (!SafeWrite(valueAddr, bytes, width)) {
        log_line("[flat-write] FAILED id=<0x%08x>: mach_vm_write rejected @%p (read-only?)",
                 id.nameHash, (void*)valueAddr);
        return false;
    }

    // Verify the write landed (atomic-or-reject: a partial/failed write is caught).
    uint32_t check = 0;
    SafeReadU32(valueAddr, &check);
    uint32_t newRaw = 0; std::memcpy(&newRaw, bytes, width < 4 ? width : 4);
    const uint32_t mask = (width >= 4) ? 0xFFFFFFFFu : ((1u << (width * 8)) - 1u);
    const bool ok = (check & mask) == (newRaw & mask);

    log_line("[flat-write] id=<TweakDBID(0x%08x,len=%u)> type=%s @%p old=0x%08x new=0x%08x "
             "width=%u verify=%s",
             id.nameHash, id.nameLength, FlatTypeName(newValue.type),
             (void*)valueAddr, oldRaw, newRaw, width, ok ? "ok" : "MISMATCH");
    return ok;
}

void VerifyFlatEntry(const TweakDB* db) {
    std::call_once(g_flatEntry_once, [db] { DoVerifyFlatEntry(db); });
}

// ═══════════════════════════════════════════════════════════════════════════
// P1.12 — runtime candidate-flat verifier.
// ═══════════════════════════════════════════════════════════════════════════

namespace {

std::once_flag g_candidateFlats_once;

// A known-present RECORD from the candidate file's verified set (first weapon
// entry). Used by the pre-flight to prove the CRC32 + records hash path works
// before trusting flats-map MISSes.
constexpr const char* kPreflightRecord = "Items.Preset_Nova_Default";

TweakDBID MakeCompactId(const std::string& name) {
    TweakDBID id;
    std::memset(&id, 0, sizeof(id));
    id.nameHash   = Crc32(name.data(), name.size());   // zlib/ISO-3309 CRC32 (HashMap.cpp)
    id.nameLength = (uint8_t)name.size();              // tdbOffset left {0,0,0}
    return id;
}

// Drop everything from the first '#', then trim surrounding whitespace.
std::string StripLine(std::string s) {
    const auto h = s.find('#');
    if (h != std::string::npos) s.erase(h);
    const auto b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return "";
    const auto e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

// TWEAKXL_CANDIDATES_FILE wins; otherwise derive from this dylib's own path.
std::string ResolveCandidatesPath(bool* fromEnv) {
    if (const char* env = std::getenv("TWEAKXL_CANDIDATES_FILE"); env && *env) {
        *fromEnv = true;
        return env;
    }
    *fromEnv = false;
    Dl_info info;
    if (dladdr(reinterpret_cast<const void*>(&VerifyCandidateFlats), &info) && info.dli_fname) {
        std::string p = info.dli_fname;
        const auto slash = p.find_last_of('/');
        const std::string dir = (slash == std::string::npos) ? "." : p.substr(0, slash);
        return dir + "/../../../tools/probes/candidate_flats.txt";
    }
    return "tools/probes/candidate_flats.txt";
}

// Prove a map's lookup PATH works independent of CRC32: round-trip its own
// first stored entry, once with the full stored key, once with a compact
// {nameHash,len,0} rebuild. Sets *full / *compact to the HIT results.
void SelfTestMapLookup(const HashMap* map, HashMode mode, const char* label,
                       bool* full, bool* compact) {
    *full = false; *compact = false;
    if (!map) { log_line("[flat-probe] preflight-self(%s): null map", label); return; }
    const uintptr_t mb = reinterpret_cast<uintptr_t>(map);
    uint32_t bucketCount = 0, stride = 0;
    uintptr_t bucketArray = 0, entries = 0;
    SafeReadU32(mb + offsetof(HashMap, bucketCount), &bucketCount);
    SafeReadU32(mb + offsetof(HashMap, stride),      &stride);
    SafeReadPtr(mb + offsetof(HashMap, bucketArray), &bucketArray);
    SafeReadPtr(mb + offsetof(HashMap, entries),     &entries);

    uint32_t headIdx = 0xFFFFFFFFu;
    for (uint32_t i = 0; i < bucketCount; ++i) {
        uint32_t v = 0xFFFFFFFFu;
        if (SafeReadU32(bucketArray + (uint64_t)i * 4, &v) && v != 0xFFFFFFFFu) { headIdx = v; break; }
    }
    if (headIdx == 0xFFFFFFFFu || !entries || !stride) {
        log_line("[flat-probe] preflight-self(%s): could not sample an entry", label);
        return;
    }
    const uintptr_t e = entries + (uint64_t)headIdx * stride;
    uint64_t storedKey = 0;
    SafeReadU64(e + 0x08, &storedKey);

    TweakDBID fullKey; std::memcpy(&fullKey, &storedKey, sizeof(fullKey));
    *full = (Lookup(map, fullKey, mode) != nullptr);

    TweakDBID cmp; std::memset(&cmp, 0, sizeof(cmp));
    cmp.nameHash   = (uint32_t)(storedKey & 0xFFFFFFFFu);
    cmp.nameLength = (uint8_t)((storedKey >> 32) & 0xFFu);
    *compact = (Lookup(map, cmp, mode) != nullptr);

    log_line("[flat-probe] preflight-self(%s): storedKey=0x%016llx fullKeyLookup=%s "
             "compactRebuildLookup=%s", label, (unsigned long long)storedKey,
             *full ? "HIT" : "MISS", *compact ? "HIT" : "MISS");
}

void DoVerifyCandidateFlats(const TweakDB* db) {
    if (!db) { log_line("[flat-probe] skipped (db=null)"); return; }

    const HashMap* recMap   = GetRecordsMap(db);
    const HashMode rmode     = GetRecordsHashMode();
    const HashMap* flatsMap0 = GetFlatsMapCandidate(db);
    const HashMode fmode0     = GetFlatsHashMode();

    // ── Pre-flight self-tests: prove BOTH map lookup paths work independent of
    // CRC32. The candidate sweep queries the FLATS map, so its path is what
    // makes a MISS trustworthy; the records path is the secondary cross-check.
    bool recFull = false, recCompact = false, flatFull = false, flatCompact = false;
    SelfTestMapLookup(recMap,   rmode,  "records", &recFull,  &recCompact);
    SelfTestMapLookup(flatsMap0, fmode0, "flats",   &flatFull, &flatCompact);

    // ── Pre-flight: the named known RECORD (tests CRC32-of-name end to end). ──
    const TweakDBID recId = MakeCompactId(kPreflightRecord);
    const bool recHit = (Lookup(recMap, recId, rmode) != nullptr);
    log_line("[flat-probe] preflight: records-map lookup of '%s' (hash=0x%08x) -> %s",
             kPreflightRecord, recId.nameHash, recHit ? "HIT" : "MISS");

    // Gate on the FLATS path (that's what the sweep uses). If a known-present
    // flats key, rebuilt compact, cannot be found, the flats lookup machinery is
    // broken → MISSes would be noise → skip. Otherwise the sweep result is sound.
    if (!flatCompact) {
        log_line("[flat-probe] Flats-map lookup failing — compact-key lookup of a known-present "
                 "flats entry MISSED (full-key=%s). Systemic bug (hash-mode/offset). Skipping sweep.",
                 flatFull ? "HIT" : "MISS");
        return;
    }
    if (!recHit) {
        log_line("[flat-probe] NOTE: '%s' absent from records map (records path %s). Flats lookup "
                 "path IS verified (self-test compact HIT) — candidate MISSes are trustworthy.",
                 kPreflightRecord, recCompact ? "verified" : "ALSO failing — flag to researcher");
    }

    // ── Resolve + open the candidate file. ───────────────────────────────────
    bool fromEnv = false;
    const std::string path = ResolveCandidatesPath(&fromEnv);
    log_line("[flat-probe] candidates file: %s (source=%s)",
             path.c_str(), fromEnv ? "TWEAKXL_CANDIDATES_FILE" : "dylib-relative");
    std::ifstream in(path);
    if (!in) {
        log_line("[flat-probe] ERROR: cannot open candidates file '%s' "
                 "(set TWEAKXL_CANDIDATES_FILE)", path.c_str());
        return;
    }

    const HashMap*  flats = GetFlatsMapCandidate(db);   // +0x58 (H-008 confirmed)
    const HashMode  fmode = GetFlatsHashMode();         // nameHash-direct (P1.6/F-019)

    unsigned total = 0, hits = 0, misses = 0;
    std::string line;
    while (std::getline(in, line)) {
        const std::string name = StripLine(line);
        if (name.empty()) continue;
        ++total;

        const TweakDBID id = MakeCompactId(name);
        const uint8_t*  hit = Lookup(flats, id, fmode);
        if (!hit) {
            ++misses;
            log_line("[flat-probe] MISS name=%s hash=0x%08x", name.c_str(), id.nameHash);
            continue;
        }
        ++hits;
        // Confirmed layout (P1.6): FlatValue ptr @ entry+0x18; vft @ +0x00; value @ +0x08.
        const uintptr_t entry = reinterpret_cast<uintptr_t>(hit);
        uintptr_t flatPtr = 0;
        SafeReadPtr(entry + 0x18, &flatPtr);
        uint64_t vft = 0;
        uint32_t raw = 0;
        if (flatPtr) {
            SafeReadU64(flatPtr + 0x00, &vft);
            SafeReadU32(flatPtr + 0x08, &raw);
        }
        log_line("[flat-probe] HIT name=%s hash=0x%08x flat=%p vft=0x%llx raw=0x%08x",
                 name.c_str(), id.nameHash, (void*)flatPtr,
                 (unsigned long long)vft, raw);
    }

    log_line("[flat-probe] sweep complete: %u candidates, hits=%u, misses=%u",
             total, hits, misses);
}

} // namespace

void VerifyCandidateFlats(const TweakDB* db) {
    std::call_once(g_candidateFlats_once, [db] { DoVerifyCandidateFlats(db); });
}

// ── P1.13: flats map walker ──────────────────────────────────────────────────
namespace {

std::once_flag g_dumpFlats_once;

// Treat any address in canonical user-space as "plausibly a pointer". The game
// image is mapped near 0x100000000 (base) + slide; heap allocations land in the
// 0x100000000–0x800000000000 region. Anything outside that is almost certainly
// an inline value or junk we read while a slot was being recycled.
bool IsPlausibleUserPtr(uint64_t v) {
    return v >= 0x100000000ull && v < 0x800000000000ull;
}

// Per-vft tracker: counts entries that look like each scalar type by byte
// signature of the buffer-stored value. The dominant classification gives the
// type of the vftable (one of FlatValueFloat/Int32/Bool/CName/TweakDBID/String).
struct VftCluster {
    uint32_t freq         = 0;
    uint32_t aligned4     = 0;
    uint32_t aligned8     = 0;
    uint32_t bytesAllZero = 0;          // value all-zero (Bool false / default)
    uint32_t bytesAscii   = 0;          // first 4 bytes printable ASCII (likely String)
    uint32_t floatRange   = 0;          // 4-aligned, float ∈ [0.001, 1e7], nonzero
    uint32_t intRange     = 0;          // 4-aligned, value as i32 ∈ [1, 1e8]
    uint32_t cnameLike    = 0;          // 8-aligned, top byte ≥ 0x40 (CName hash)
    uint32_t boolLike     = 0;          // value byte 0x00 or 0x01, rest 0
    std::vector<uint32_t> sampleIdx;
    std::vector<uint32_t> typedSamples; // entries flagged for the dominant non-zero type
};

// Heuristic byte-signature classifier. Returns flags packed in `out` for the
// cluster to accumulate.
void ClassifyBufferBytes(uint64_t bytes, uint32_t bufOff, VftCluster& out) {
    if (bufOff == 0) return;
    const bool a4 = ((bufOff & 0x3) == 0);
    const bool a8 = ((bufOff & 0x7) == 0);
    if (a4) ++out.aligned4;
    if (a8) ++out.aligned8;
    if (bytes == 0) { ++out.bytesAllZero; return; }
    // Bool: low byte is 0/1 and rest is zero.
    const uint8_t b0 = (uint8_t)(bytes & 0xFFu);
    if ((b0 == 0x00 || b0 == 0x01) && (bytes >> 8) == 0) { ++out.boolLike; return; }
    // ASCII string: first 4 bytes printable.
    bool asc = true;
    for (int i = 0; i < 4; ++i) {
        const uint8_t b = (uint8_t)((bytes >> (i*8)) & 0xFFu);
        if (b < 0x20 || b >= 0x7f) { asc = false; break; }
    }
    if (asc) { ++out.bytesAscii; return; }
    // Numeric: 4-aligned, interpret low-32 as float and int.
    if (a4) {
        const uint32_t raw = (uint32_t)(bytes & 0xFFFFFFFFu);
        float f; std::memcpy(&f, &raw, 4);
        const int32_t  iv = (int32_t)raw;
        if (std::isfinite(f) && f != 0.0f) {
            const float af = std::fabs(f);
            if (af >= 0.001f && af <= 1.0e7f) { ++out.floatRange; return; }
        }
        if (iv >= 1 && iv <= 100000000) { ++out.intRange; return; }
    }
    // CName: 8-aligned + top byte set.
    if (a8) {
        const uint8_t topB = (uint8_t)((bytes >> 56) & 0xFFu);
        if (topB >= 0x40) { ++out.cnameLike; return; }
    }
}

constexpr size_t kSamplesPerCluster = 3;
constexpr size_t kTopClustersToDump  = 30;
// Range that catches most gameplay floats: 0.01 .. 1e6. Anything outside is
// almost certainly an int / hash / garbage misinterpreted as float.
bool LooksLikeGameplayFloat(uint64_t val64) {
    float f; std::memcpy(&f, &val64, sizeof(f));
    if (!std::isfinite(f)) return false;
    const float af = std::fabs(f);
    return af >= 0.01f && af <= 1.0e6f;
}

// Print "..." for non-ASCII bytes — diagnostic-readable, not lossless.
void HexAscii(uint64_t v, char* out) {
    for (int i = 0; i < 8; ++i) {
        uint8_t b = (uint8_t)((v >> (i*8)) & 0xFF);
        out[i] = (b >= 0x20 && b < 0x7f) ? (char)b : '.';
    }
    out[8] = 0;
}

// Cached at the start of DoDumpFlatsSample so DumpEntry can reach the buffer.
uintptr_t g_dumpBufferBase = 0;
uint32_t  g_dumpBufferSize = 0;

void DumpEntry(uint32_t i, uintptr_t e) {
    uint64_t storedKey = 0;
    uintptr_t ptr10 = 0;
    SafeReadU64(e + 0x08, &storedKey);
    SafeReadPtr(e + 0x10, &ptr10);
    // Dump 9 consecutive 8-byte words from p10 (+0x00..+0x40). Real value
    // slots — Float/Int/CName — must live SOMEWHERE in this range; finding
    // them is just a matter of looking.
    uint64_t p10w[9] = {0};
    if (IsPlausibleUserPtr(ptr10)) {
        for (int k = 0; k < 9; ++k) SafeReadU64(ptr10 + 0x00 + 8*k, &p10w[k]);
    }
    const uint32_t nameHash = (uint32_t)(storedKey & 0xFFFFFFFFu);
    const uint8_t  nameLen  = (uint8_t)((storedKey >> 32) & 0xFFu);
    // Interpret each 8-byte word as float-at-low-32 to see which slot holds
    // gameplay magnitudes.
    char fl[9][24] = {{0}};
    for (int k = 0; k < 9; ++k) {
        float f; std::memcpy(&f, &p10w[k], sizeof(f));
        std::snprintf(fl[k], sizeof(fl[k]), "%.4g", (double)f);
    }
    log_line("[flat-dump] e[%u] hash=0x%08x len=%u p10=%p vft=0x%llx "
             "+8=%016llx +10=%016llx +18=%016llx +20=%016llx +28=%016llx "
             "+30=%016llx +38=%016llx +40=%016llx "
             "[asF: +8=%s +10=%s +18=%s +20=%s +28=%s +30=%s +38=%s +40=%s]",
             i, nameHash, nameLen, (void*)ptr10,
             (unsigned long long)p10w[0],
             (unsigned long long)p10w[1], (unsigned long long)p10w[2],
             (unsigned long long)p10w[3], (unsigned long long)p10w[4],
             (unsigned long long)p10w[5], (unsigned long long)p10w[6],
             (unsigned long long)p10w[7], (unsigned long long)p10w[8],
             fl[1], fl[2], fl[3], fl[4], fl[5], fl[6], fl[7], fl[8]);
}

void DoDumpFlatsSample(const TweakDB* db, uint32_t maxEntries) {
    if (!db) { log_line("[flat-dump] skipped (db=null)"); return; }
    const HashMap* flats = GetFlatsMapCandidate(db);   // +0x58
    if (!flats) { log_line("[flat-dump] no flats map"); return; }

    // Snapshot the flat data buffer pointer + size so DumpEntry can
    // resolve p10[+0x28] (a byte offset) into typed scalar bytes.
    const uintptr_t dbBase = reinterpret_cast<uintptr_t>(db);
    uintptr_t bufBasePtr = 0;
    SafeReadPtr(dbBase + offsetof(TweakDB, flatDataBuffer), &bufBasePtr);
    SafeReadU32(dbBase + offsetof(TweakDB, flatDataBufferSize), &g_dumpBufferSize);
    g_dumpBufferBase = bufBasePtr;
    log_line("[flat-dump] flat-buffer base=%p size=%u",
             (void*)g_dumpBufferBase, g_dumpBufferSize);

    const uintptr_t mb = reinterpret_cast<uintptr_t>(flats);
    uint32_t  count = 0, stride = 0, bucketCount = 0;
    uintptr_t entries = 0, bucketArray = 0;
    SafeReadU32(mb + offsetof(HashMap, count),       &count);
    SafeReadU32(mb + offsetof(HashMap, stride),      &stride);
    SafeReadU32(mb + offsetof(HashMap, bucketCount), &bucketCount);
    SafeReadPtr(mb + offsetof(HashMap, entries),     &entries);
    SafeReadPtr(mb + offsetof(HashMap, bucketArray), &bucketArray);

    log_line("[flat-dump] map=+0x58 count=%u bucketCount=%u stride=%u entries=%p",
             count, bucketCount, stride, (void*)entries);
    if (!entries || !stride || count == 0) {
        log_line("[flat-dump] map not initialized — bailing"); return;
    }

    // Walk EVERY entry (bounded by maxEntries) to get the full type histogram.
    // Default cap is 500 (light), but deep-mode (TWEAKXL_DUMP_FLATS_MAX=200000)
    // sweeps the whole map so we see the rare-but-cinema-meaningful clusters
    // (Float, Int32, Bool, CName) that the uniform-vft head doesn't expose.
    const uint32_t cap = std::min(count, maxEntries);
    log_line("[flat-dump] deep-walking %u of %u entries", cap, count);

    // Walk every entry, classify its buffer bytes, and aggregate per-vft.
    std::map<uint64_t, VftCluster> clusters;

    for (uint32_t i = 0; i < cap; ++i) {
        const uintptr_t e = entries + (uint64_t)i * stride;
        uint64_t storedKey = 0;
        SafeReadU64(e + 0x08, &storedKey);
        if (storedKey == 0) continue;

        uintptr_t p10 = 0;
        SafeReadPtr(e + 0x10, &p10);
        if (!IsPlausibleUserPtr(p10)) continue;

        uint64_t vft = 0, p10_28 = 0;
        SafeReadU64(p10 + 0x00, &vft);
        SafeReadU64(p10 + 0x28, &p10_28);
        const uint32_t bufOff = (uint32_t)(p10_28 & 0xFFFFFFFFu);

        auto& c = clusters[vft];
        c.freq++;
        if (c.sampleIdx.size() < kSamplesPerCluster) c.sampleIdx.push_back(i);

        if (g_dumpBufferBase && bufOff != 0 && bufOff + 8 <= g_dumpBufferSize) {
            uint64_t bv = 0;
            SafeReadU64(g_dumpBufferBase + bufOff, &bv);
            const uint32_t before = c.floatRange + c.intRange + c.boolLike +
                                    c.bytesAscii + c.cnameLike;
            ClassifyBufferBytes(bv, bufOff, c);
            const bool flagged = (c.floatRange + c.intRange + c.boolLike +
                                  c.bytesAscii + c.cnameLike) > before;
            if (flagged && c.typedSamples.size() < 5) c.typedSamples.push_back(i);
        }
    }

    log_line("[flat-dump] walked %u entries, %u unique vfts",
             cap, (unsigned)clusters.size());

    // One-shot deep dump: read the first non-empty entry's p10 + 256 bytes
    // raw — the value MUST live in there if it's stored on the object at all.
    for (uint32_t i = 0; i < cap && i < 200; ++i) {
        const uintptr_t e = entries + (uint64_t)i * stride;
        uint64_t storedKey = 0;
        SafeReadU64(e + 0x08, &storedKey);
        if (storedKey == 0) continue;
        uintptr_t p10 = 0;
        SafeReadPtr(e + 0x10, &p10);
        if (!IsPlausibleUserPtr(p10)) continue;
        log_line("[flat-dump] === deep p10 dump (256 bytes) entry[%u] key=0x%016llx p10=%p ===",
                 i, (unsigned long long)storedKey, (void*)p10);
        for (uint32_t off = 0; off < 256; off += 16) {
            uint64_t a = 0, b = 0;
            SafeReadU64(p10 + off,     &a);
            SafeReadU64(p10 + off + 8, &b);
            float fa, fb; std::memcpy(&fa, &a, 4); std::memcpy(&fb, &b, 4);
            log_line("[flat-dump]   p10[+0x%02x] = %016llx %016llx | asF: %.4g %.4g | asI: %d %d",
                     off, (unsigned long long)a, (unsigned long long)b,
                     (double)fa, (double)fb,
                     (int32_t)(a & 0xffffffffu), (int32_t)(b & 0xffffffffu));
        }
        break;
    }

    // Reverse search: scan the flat data buffer for 4-byte aligned floats in
    // a tight gameplay range, then trace each hit back to the cluster (vft)
    // that owns it. A type whose buffer offsets disproportionately land on
    // these float-magnitude slots IS the FlatValueFloat type.
    if (g_dumpBufferBase && g_dumpBufferSize > 0) {
        std::map<uint32_t, std::pair<uint32_t, float>> bufOffFloats; // bufOff → (count, sample)
        const uint32_t scanBytes = std::min<uint32_t>(g_dumpBufferSize, 4u * 1024 * 1024);
        for (uint32_t o = 0; o + 4 <= scanBytes; o += 4) {
            uint32_t raw = 0;
            SafeReadU32(g_dumpBufferBase + o, &raw);
            if (raw == 0) continue;
            float f; std::memcpy(&f, &raw, 4);
            if (!std::isfinite(f)) continue;
            const float af = std::fabs(f);
            // Tight gameplay-magnitude window: catches damage, range, speed,
            // multiplier — excludes denormals, large hash-like ints
            // reinterpreted as float, and most random data.
            if (af >= 5.0f && af <= 500.0f) {
                auto it = bufOffFloats.find(o);
                if (it == bufOffFloats.end()) bufOffFloats[o] = {1, f};
                else it->second.first++;
            }
        }
        log_line("[flat-dump] buffer-scan: %zu distinct 4-aligned offsets in [5,500] over %u bytes",
                 bufOffFloats.size(), scanBytes);

        // For each entry, check if its bufOff is a "looks like Float" offset
        // and accumulate per-vft.
        std::map<uint64_t, std::pair<uint32_t, uint32_t>> vftFloatHits; // vft → (count, sampleIdx)
        for (uint32_t i = 0; i < cap; ++i) {
            const uintptr_t e = entries + (uint64_t)i * stride;
            uint64_t storedKey = 0;
            SafeReadU64(e + 0x08, &storedKey);
            if (storedKey == 0) continue;
            uintptr_t p10 = 0;
            SafeReadPtr(e + 0x10, &p10);
            if (!IsPlausibleUserPtr(p10)) continue;
            uint64_t vft = 0, p10_28 = 0;
            SafeReadU64(p10 + 0x00, &vft);
            SafeReadU64(p10 + 0x28, &p10_28);
            const uint32_t bufOff = (uint32_t)(p10_28 & 0xFFFFFFFFu);
            if (bufOffFloats.count(bufOff)) {
                auto& v = vftFloatHits[vft];
                v.first++;
                if (v.second == 0) v.second = i;
            }
        }

        std::vector<std::pair<uint64_t, std::pair<uint32_t, uint32_t>>> vfh(
            vftFloatHits.begin(), vftFloatHits.end());
        std::sort(vfh.begin(), vfh.end(),
                  [](const auto& a, const auto& b) { return a.second.first > b.second.first; });
        log_line("[flat-dump] === REVERSE-SCAN: vfts whose bufOffs land on gameplay-float slots ===");
        for (size_t k = 0; k < std::min<size_t>(15, vfh.size()); ++k) {
            const auto& it = clusters.find(vfh[k].first);
            const uint32_t total = (it != clusters.end()) ? it->second.freq : 0;
            const uint32_t hits  = vfh[k].second.first;
            const double pct = total ? (100.0 * hits / total) : 0.0;
            log_line("[flat-dump] FLOAT-CAND vft=0x%llx freq=%u float-bufOff-hits=%u (%.1f%%)",
                     (unsigned long long)vfh[k].first, total, hits, pct);
            DumpEntry(vfh[k].second.second,
                      entries + (uint64_t)vfh[k].second.second * stride);
        }
    }

    // Pick the dominant classification per cluster, then group by type.
    auto dominantType = [](const VftCluster& c) -> const char* {
        struct Cand { const char* name; uint32_t n; };
        Cand v[] = {
            {"Float",  c.floatRange},
            {"Int32",  c.intRange},
            {"String", c.bytesAscii},
            {"Bool",   c.boolLike},
            {"CName",  c.cnameLike},
        };
        uint32_t best = 0; const char* who = "Unknown";
        for (auto& x : v) if (x.n > best) { best = x.n; who = x.name; }
        // Require at least 25% of buffer-readable entries to commit to a type.
        const uint32_t classified = c.floatRange + c.intRange + c.bytesAscii +
                                    c.boolLike + c.cnameLike + c.bytesAllZero;
        if (best == 0 || (classified > 0 && best * 4 < classified)) return "Unknown";
        return who;
    };

    // Ranking helpers.
    std::vector<std::pair<uint64_t, VftCluster>> all(clusters.begin(), clusters.end());

    // (A) Float-typed cluster shortlist — these are the cinema targets.
    log_line("[flat-dump] === FLOAT-typed clusters (sorted by absolute floatRange count) ===");
    std::sort(all.begin(), all.end(),
              [](const std::pair<uint64_t, VftCluster>& a,
                 const std::pair<uint64_t, VftCluster>& b) {
                  return a.second.floatRange > b.second.floatRange;
              });
    uint32_t shown = 0;
    for (auto& kv : all) {
        if (kv.second.floatRange == 0) break;
        if (shown >= 10) break;
        const VftCluster& c = kv.second;
        log_line("[flat-dump] FLOAT vft=0x%llx freq=%u float=%u int=%u str=%u bool=%u "
                 "cname=%u zero=%u a4=%u",
                 (unsigned long long)kv.first, c.freq,
                 c.floatRange, c.intRange, c.bytesAscii, c.boolLike,
                 c.cnameLike, c.bytesAllZero, c.aligned4);
        for (uint32_t idx : c.typedSamples) {
            DumpEntry(idx, entries + (uint64_t)idx * stride);
        }
        ++shown;
    }

    // (B) Per-type summary across the top-N most frequent clusters.
    log_line("[flat-dump] === Per-cluster type classification (top by freq) ===");
    std::sort(all.begin(), all.end(),
              [](const std::pair<uint64_t, VftCluster>& a,
                 const std::pair<uint64_t, VftCluster>& b) {
                  return a.second.freq > b.second.freq;
              });
    for (size_t k = 0; k < std::min<size_t>(kTopClustersToDump, all.size()); ++k) {
        const VftCluster& c = all[k].second;
        log_line("[flat-dump] type[%zu]: vft=0x%llx freq=%u dominant=%s "
                 "[F=%u I=%u S=%u B=%u C=%u Z=%u]",
                 k, (unsigned long long)all[k].first, c.freq, dominantType(c),
                 c.floatRange, c.intRange, c.bytesAscii, c.boolLike,
                 c.cnameLike, c.bytesAllZero);
        for (uint32_t idx : c.sampleIdx) {
            DumpEntry(idx, entries + (uint64_t)idx * stride);
        }
    }
}

} // namespace

// ── P1.14b: name → vtable mapping ────────────────────────────────────────────
namespace {

std::once_flag g_mapNames_once;

std::string ResolveRecordNamesPath() {
    if (const char* env = std::getenv("TWEAKXL_RECORDS_FILE"); env && *env) {
        return env;
    }
    Dl_info info;
    if (dladdr(reinterpret_cast<const void*>(&MapNamesToVftables), &info) && info.dli_fname) {
        std::string p = info.dli_fname;
        const auto slash = p.find_last_of('/');
        const std::string dir = (slash == std::string::npos) ? "." : p.substr(0, slash);
        return dir + "/../../../reference/tweakxl-data/record_names.txt";
    }
    return "reference/tweakxl-data/record_names.txt";
}

// Dump 128 bytes at a named record's p10 so the field layout of one
// representative record per type can be eyeballed for a known-value Float.
void DumpNamedRecord(const TweakDB* db, const char* name) {
    const HashMap* flats = GetFlatsMapCandidate(db);
    if (!flats) return;
    const HashMode fmode = GetFlatsHashMode();
    TweakDBID id{};
    id.nameHash   = Crc32(name, std::strlen(name));
    id.nameLength = (uint8_t)std::min<size_t>(std::strlen(name), 255);
    const uint8_t* hit = Lookup(flats, id, fmode);
    if (!hit) { log_line("[record-dump] '%s' MISS", name); return; }

    const uintptr_t e = reinterpret_cast<uintptr_t>(hit);
    uintptr_t p10 = 0;
    SafeReadPtr(e + 0x10, &p10);
    if (!IsPlausibleUserPtr(p10)) { log_line("[record-dump] '%s' p10 invalid", name); return; }

    uint64_t vft = 0;
    SafeReadU64(p10 + 0x00, &vft);
    log_line("[record-dump] === '%s' (hash=0x%08x) p10=%p vft=0x%llx ===",
             name, id.nameHash, (void*)p10, (unsigned long long)vft);
    for (uint32_t off = 0; off < 128; off += 16) {
        uint64_t a = 0, b = 0;
        SafeReadU64(p10 + off,     &a);
        SafeReadU64(p10 + off + 8, &b);
        // Bit-cast LOW 32 bits of each word as float — gameplay floats jump
        // out by reading sensibly (e.g. 0.1, 1.0, 100.0).
        float fa, fb; std::memcpy(&fa, &a, 4); std::memcpy(&fb, &b, 4);
        log_line("[record-dump]   +0x%02x = %016llx %016llx | asF: %.6g %.6g | asI: %d %d",
                 off, (unsigned long long)a, (unsigned long long)b,
                 (double)fa, (double)fb,
                 (int32_t)(a & 0xffffffffu), (int32_t)(b & 0xffffffffu));
    }
}

void DoMapNamesToVftables(const TweakDB* db) {
    if (!db) { log_line("[name-vft] skipped (db=null)"); return; }
    const HashMap* flats = GetFlatsMapCandidate(db);          // +0x58
    if (!flats) { log_line("[name-vft] no +0x58 map"); return; }
    const HashMode fmode = GetFlatsHashMode();

    const std::string namesPath = ResolveRecordNamesPath();
    std::ifstream in(namesPath);
    if (!in) {
        log_line("[name-vft] ERROR: cannot open names file '%s' "
                 "(set TWEAKXL_RECORDS_FILE to override)", namesPath.c_str());
        return;
    }
    log_line("[name-vft] reading names from %s", namesPath.c_str());

    const char* outPath = std::getenv("TWEAKXL_VFT_MAP_OUT");
    FILE* outFp = outPath ? std::fopen(outPath, "w") : nullptr;
    if (outFp) log_line("[name-vft] writing tab-separated <vft>\\t<name> to %s", outPath);

    // Per-vtable bucket: count + up to N sample names.
    struct Bucket {
        uint32_t freq = 0;
        std::vector<std::string> samples;
    };
    std::map<uint64_t, Bucket> byVft;
    constexpr size_t kSamplesPerVft = 8;

    uint32_t total = 0, hits = 0, misses = 0;
    std::string line;
    while (std::getline(in, line)) {
        // Strip CR if present (cross-platform line endings).
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ' || line.back() == '\t')) {
            line.pop_back();
        }
        if (line.empty() || line[0] == '#') continue;
        ++total;

        TweakDBID id{};
        id.nameHash   = Crc32(line.data(), line.size());
        id.nameLength = (uint8_t)std::min<size_t>(line.size(), 255);
        // tdbOffset stays zero (compact-key probe in F-022).

        const uint8_t* hit = Lookup(flats, id, fmode);
        if (!hit) { ++misses; continue; }
        ++hits;

        // Read entry+0x10 → p10, then *p10 = vtable (F-023).
        const uintptr_t entryAddr = reinterpret_cast<uintptr_t>(hit);
        uintptr_t p10 = 0;
        SafeReadPtr(entryAddr + 0x10, &p10);
        uint64_t vft = 0;
        if (p10) SafeReadU64(p10 + 0x00, &vft);

        auto& b = byVft[vft];
        b.freq++;
        if (b.samples.size() < kSamplesPerVft) b.samples.push_back(line);

        if (outFp) std::fprintf(outFp, "0x%llx\t%s\n",
                                (unsigned long long)vft, line.c_str());
    }
    if (outFp) std::fclose(outFp);

    log_line("[name-vft] resolved %u/%u names (hits=%u misses=%u), %u distinct vtables",
             hits, total, hits, misses, (unsigned)byVft.size());

    // Rank vtable clusters by frequency and emit each one's sample names.
    std::vector<std::pair<uint64_t, Bucket>> ranked(byVft.begin(), byVft.end());
    std::sort(ranked.begin(), ranked.end(),
              [](const std::pair<uint64_t, Bucket>& a,
                 const std::pair<uint64_t, Bucket>& b) {
                  return a.second.freq > b.second.freq;
              });
    log_line("[name-vft] === per-vtable record clusters (sample names reveal type) ===");
    for (size_t k = 0; k < std::min<size_t>(40, ranked.size()); ++k) {
        const Bucket& b = ranked[k].second;
        std::string samples;
        for (size_t s = 0; s < b.samples.size(); ++s) {
            if (s) samples += ", ";
            samples += b.samples[s];
        }
        log_line("[name-vft] vft=0x%llx freq=%u samples=[%s]",
                 (unsigned long long)ranked[k].first, b.freq, samples.c_str());
    }

    // ── Targeted record dumps: one representative per likely-Float-bearing
    // record class (Stat, AttackModifier, Attack) and one heavy reference type
    // (Vehicle, Character) for comparison. The byte layouts side-by-side reveal
    // which offset holds the Float for the Stat type — the cinema target.
    log_line("[name-vft] === targeted record byte dumps (find the Float slot) ===");
    const char* targets[] = {
        // Stat records — these are PURE Float-bearing (BaseStats.X.value : Float).
        "BaseStats.ADSSpeedPercentBonus",
        "BaseStats.AccumulatedDoTDecayRate",
        "BaseStats.AccumulatedDoTDecayStartDelay",
        // AttackModifier — also float-bearing.
        "AttackModifier.ChopperMinTBH",
        "AttackModifier.AlliedDamage",
        // Attack — has fields like damage etc.
        "Attacks.BodySlamAttackLevel1",
        // Larger records for offset comparison.
        "Vehicle.PlayerCar",
        "Items.Preset_Lexington_Military",
    };
    for (const char* t : targets) DumpNamedRecord(db, t);
}

} // namespace

void MapNamesToVftables(const TweakDB* db) {
    std::call_once(g_mapNames_once, [db] { DoMapNamesToVftables(db); });
}

// ── P1.17: cinema mutation ───────────────────────────────────────────────────
namespace {

std::once_flag g_cinema_once;

void DoCinemaMutate(const TweakDB* db) {
    const char* name = std::getenv("TWEAKXL_CINEMA_NAME");
    const char* val  = std::getenv("TWEAKXL_CINEMA_VALUE");
    if (!name || !*name || !val || !*val) {
        log_line("[cinema] skipped (set TWEAKXL_CINEMA_NAME + TWEAKXL_CINEMA_VALUE to fire)");
        return;
    }
    if (!db) { log_line("[cinema] skipped (db=null)"); return; }

    float newValue = (float)std::strtod(val, nullptr);

    const HashMap* flats = GetFlatsMapCandidate(db);
    if (!flats) { log_line("[cinema] no +0x58 map"); return; }
    const HashMode fmode = GetFlatsHashMode();

    TweakDBID id{};
    id.nameHash   = Crc32(name, std::strlen(name));
    id.nameLength = (uint8_t)std::min<size_t>(std::strlen(name), 255);
    const uint8_t* hit = Lookup(flats, id, fmode);
    if (!hit) {
        log_line("[cinema] '%s' MISS (hash=0x%08x)", name, id.nameHash);
        return;
    }

    const uintptr_t entryAddr = reinterpret_cast<uintptr_t>(hit);
    uintptr_t p10 = 0;
    SafeReadPtr(entryAddr + 0x10, &p10);
    if (!IsPlausibleUserPtr(p10)) {
        log_line("[cinema] '%s' invalid p10=%p", name, (void*)p10);
        return;
    }

    uint64_t vft = 0;
    SafeReadU64(p10 + 0x00, &vft);

    // Read current value at p10+0x54 (F-027: gamedataStat_Record value slot).
    uint32_t beforeRaw = 0;
    SafeReadU32(p10 + 0x54, &beforeRaw);
    float beforeF; std::memcpy(&beforeF, &beforeRaw, 4);

    // Write new value via mach_vm_write — same path that worked for the
    // refcount-byte test (F-022 / F-023), now applied to the actual value slot.
    uint32_t newRaw;
    std::memcpy(&newRaw, &newValue, 4);
    const bool wrote = SafeWrite(p10 + 0x54, &newRaw, sizeof(newRaw));

    // Re-read to confirm the write committed (write-back path verification).
    uint32_t afterRaw = 0;
    SafeReadU32(p10 + 0x54, &afterRaw);
    float afterF; std::memcpy(&afterF, &afterRaw, 4);

    const bool ok = wrote && (afterRaw == newRaw);
    log_line("[cinema] '%s' vft=0x%llx p10=%p +0x54 before=%.6g after=%.6g "
             "wrote=%d verify=%s",
             name, (unsigned long long)vft, (void*)p10,
             (double)beforeF, (double)afterF,
             wrote ? 1 : 0, ok ? "OK" : "MISMATCH");
}

} // namespace

void CinemaMutateStat(const TweakDB* db) {
    std::call_once(g_cinema_once, [db] { DoCinemaMutate(db); });
}

// ── P1.17b: BULK cinema mutation across all gamedataStat_Record entries ─────
namespace {

std::once_flag g_cinemaBulk_once;

struct StatTarget {
    std::string name;
    uintptr_t   valueAddr;     // p10 + 0x54
    float       originalValue; // captured on first pass
};

// Discover the live Stat_Record vtable by looking up a known BaseStats name
// (the run's slide changes the absolute vtable each launch; we resolve it
// dynamically). Returns 0 on miss.
uint64_t DiscoverStatVft(const TweakDB* db) {
    const HashMap* flats = GetFlatsMapCandidate(db);
    if (!flats) return 0;
    const HashMode fmode = GetFlatsHashMode();
    const char* probeName = "BaseStats.AccumulatedDoTDecayRate";
    TweakDBID id{};
    id.nameHash   = Crc32(probeName, std::strlen(probeName));
    id.nameLength = (uint8_t)std::strlen(probeName);
    const uint8_t* hit = Lookup(flats, id, fmode);
    if (!hit) return 0;
    uintptr_t p10 = 0;
    SafeReadPtr(reinterpret_cast<uintptr_t>(hit) + 0x10, &p10);
    if (!IsPlausibleUserPtr(p10)) return 0;
    uint64_t vft = 0;
    SafeReadU64(p10 + 0x00, &vft);
    return vft;
}

// Build the target list: every named record whose vtable matches statVft,
// reading its current +0x54 float and recording (name, addr, original).
std::vector<StatTarget> BuildStatTargets(const TweakDB* db, uint64_t statVft) {
    std::vector<StatTarget> out;
    const HashMap* flats = GetFlatsMapCandidate(db);
    if (!flats || statVft == 0) return out;
    const HashMode fmode = GetFlatsHashMode();
    const std::string namesPath = ResolveRecordNamesPath();
    std::ifstream in(namesPath);
    if (!in) return out;
    std::string line;
    while (std::getline(in, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
        if (line.empty() || line[0] == '#') continue;
        TweakDBID id{};
        id.nameHash   = Crc32(line.data(), line.size());
        id.nameLength = (uint8_t)std::min<size_t>(line.size(), 255);
        const uint8_t* hit = Lookup(flats, id, fmode);
        if (!hit) continue;
        uintptr_t p10 = 0;
        SafeReadPtr(reinterpret_cast<uintptr_t>(hit) + 0x10, &p10);
        if (!IsPlausibleUserPtr(p10)) continue;
        uint64_t vft = 0;
        SafeReadU64(p10 + 0x00, &vft);
        if (vft != statVft) continue;
        uint32_t raw = 0;
        SafeReadU32(p10 + 0x54, &raw);
        float v; std::memcpy(&v, &raw, 4);
        // Filter: must be finite, non-zero, in a sane gameplay magnitude range.
        if (!std::isfinite(v) || v == 0.0f) continue;
        const float av = std::fabs(v);
        if (av < 1e-6f || av > 1e8f) continue;
        out.push_back({std::move(line), p10 + 0x54, v});
    }
    return out;
}

// One pass over the captured targets: write originalValue * factor.
uint32_t ApplyMutationPass(const std::vector<StatTarget>& targets, float factor) {
    uint32_t mutated = 0;
    for (const StatTarget& t : targets) {
        float newV = t.originalValue * factor;
        if (!std::isfinite(newV)) continue;
        uint32_t newRaw;
        std::memcpy(&newRaw, &newV, 4);
        if (SafeWrite(t.valueAddr, &newRaw, sizeof(newRaw))) ++mutated;
    }
    return mutated;
}

void DoCinemaBulk(const TweakDB* db) {
    if (!std::getenv("TWEAKXL_CINEMA_BULK")) {
        log_line("[cinema-bulk] skipped (set TWEAKXL_CINEMA_BULK=1 to enable)");
        return;
    }
    if (!db) { log_line("[cinema-bulk] skipped (db=null)"); return; }

    float factor = 100.0f;
    if (const char* f = std::getenv("TWEAKXL_CINEMA_BULK_FACTOR"); f && *f)
        factor = (float)std::strtod(f, nullptr);
    uint32_t repeatSec = 0;
    if (const char* r = std::getenv("TWEAKXL_CINEMA_BULK_REPEAT"); r && *r)
        repeatSec = (uint32_t)std::strtoul(r, nullptr, 10);

    const uint64_t statVft = DiscoverStatVft(db);
    if (statVft == 0) {
        log_line("[cinema-bulk] could not discover Stat_Record vftable via probe lookup");
        return;
    }
    log_line("[cinema-bulk] Stat_Record vftable resolved: 0x%llx (factor=%g repeat=%us)",
             (unsigned long long)statVft, (double)factor, repeatSec);

    auto targets = BuildStatTargets(db, statVft);
    log_line("[cinema-bulk] built target list: %zu safe Stat records to mutate",
             targets.size());

    // Sample 10 representative targets so the audit trail shows what changes.
    for (size_t k = 0; k < std::min<size_t>(10, targets.size()); ++k) {
        log_line("[cinema-bulk]   sample[%zu] %s before=%.6g new=%.6g",
                 k, targets[k].name.c_str(),
                 (double)targets[k].originalValue,
                 (double)(targets[k].originalValue * factor));
    }

    const uint32_t mutated = ApplyMutationPass(targets, factor);
    log_line("[cinema-bulk] PASS#1: mutated %u of %zu targets (factor=%g)",
             mutated, targets.size(), (double)factor);

    // Verify a few writes stuck.
    uint32_t verified = 0;
    for (size_t k = 0; k < std::min<size_t>(20, targets.size()); ++k) {
        uint32_t raw = 0;
        SafeReadU32(targets[k].valueAddr, &raw);
        float v; std::memcpy(&v, &raw, 4);
        const float expect = targets[k].originalValue * factor;
        if (v == expect) ++verified;
    }
    log_line("[cinema-bulk] verify: %u of 20 sampled writes match expected", verified);

    // Optional re-application loop: combats any per-frame "reload defaults"
    // pattern by re-writing every N seconds. Spawned detached so it never
    // blocks the loader callback chain.
    if (repeatSec > 0) {
        // Heap-copy targets for the thread's lifetime.
        auto* ownedTargets = new std::vector<StatTarget>(std::move(targets));
        const float f = factor;
        const uint32_t period = repeatSec;
        std::thread([ownedTargets, f, period]() {
            uint64_t pass = 1;
            for (;;) {
                std::this_thread::sleep_for(std::chrono::seconds(period));
                ++pass;
                const uint32_t n = ApplyMutationPass(*ownedTargets, f);
                if ((pass % 10) == 0) {
                    log_line("[cinema-bulk] re-apply pass#%llu: mutated %u/%zu",
                             (unsigned long long)pass, n, ownedTargets->size());
                }
            }
        }).detach();
        log_line("[cinema-bulk] background re-apply thread started (every %us)",
                 repeatSec);
    }
}

} // namespace

void CinemaBulkMutateStats(const TweakDB* db) {
    std::call_once(g_cinemaBulk_once, [db] { DoCinemaBulk(db); });
}

// ── P1.20: process-wide float memory scan ──────────────────────────────────
namespace {

std::once_flag g_scanMem_once;

struct ScanRegion {
    mach_vm_address_t base;
    mach_vm_size_t    size;
};

// Walk the process address space and collect every readable, writable, private
// VM region (skips __TEXT, mapped files, shared libraries — those won't hold
// per-instance gameplay state). Stops at user-space ceiling.
std::vector<ScanRegion> CollectScannableRegions() {
    std::vector<ScanRegion> out;
    mach_vm_address_t addr = 0;
    const task_t task = mach_task_self();
    for (int safety = 0; safety < 200000; ++safety) {
        mach_vm_size_t size = 0;
        natural_t depth = 1;
        vm_region_submap_info_data_64_t info;
        mach_msg_type_number_t count = VM_REGION_SUBMAP_INFO_COUNT_64;
        kern_return_t kr = mach_vm_region_recurse(
            task, &addr, &size, &depth,
            (vm_region_recurse_info_t)&info, &count);
        if (kr != KERN_SUCCESS) break;

        const bool readable  = (info.protection & VM_PROT_READ);
        const bool writable  = (info.protection & VM_PROT_WRITE);
        // Skip submaps; we already recursed for them.
        if (!info.is_submap && readable && writable && size >= 4096 &&
            size <= (1ull << 32)) {              // cap per-region 4GB
            out.push_back({addr, size});
        }
        addr += size;
        if (addr == 0) break;
    }
    return out;
}

void DoScanMemoryForFloat() {
    const char* fA = std::getenv("TWEAKXL_SCAN_FLOAT_A");
    if (!fA || !*fA) {
        log_line("[scan] skipped (set TWEAKXL_SCAN_FLOAT_A=<float> to enable)");
        return;
    }
    const char* fB = std::getenv("TWEAKXL_SCAN_FLOAT_B");
    uint32_t maxHits = 100;
    if (const char* m = std::getenv("TWEAKXL_SCAN_MAX_HITS"); m && *m)
        maxHits = (uint32_t)std::strtoul(m, nullptr, 10);
    uint32_t delaySec = 30;
    if (const char* d = std::getenv("TWEAKXL_SCAN_DELAY_SEC"); d && *d)
        delaySec = (uint32_t)std::strtoul(d, nullptr, 10);
    const char* outPath = std::getenv("TWEAKXL_SCAN_OUT");
    if (!outPath || !*outPath) outPath = "/tmp/scan_hits.tsv";

    const float tA = (float)std::strtod(fA, nullptr);
    const float tB = fB ? (float)std::strtod(fB, nullptr) : 0.0f;
    uint32_t bitsA, bitsB = 0;
    std::memcpy(&bitsA, &tA, 4);
    if (fB) std::memcpy(&bitsB, &tB, 4);

    log_line("[scan] target A=%g (bits=0x%08x) %s%s%s — delay=%us, maxHits=%u, out=%s",
             (double)tA, bitsA,
             fB ? "target B=" : "", fB ? fB : "",
             fB ? " " : "",
             delaySec, maxHits, outPath);

    // Sleep so the user can load a save before we scan — the gameplay caches
    // we're hunting only exist after the save's player entity is created.
    if (delaySec > 0) {
        log_line("[scan] sleeping %us — load your save now", delaySec);
        std::this_thread::sleep_for(std::chrono::seconds(delaySec));
    }

    const auto regions = CollectScannableRegions();
    uint64_t totalBytes = 0;
    for (const ScanRegion& r : regions) totalBytes += r.size;
    log_line("[scan] %zu writable regions, %llu MB total — scanning...",
             regions.size(), (unsigned long long)(totalBytes >> 20));

    FILE* outFp = std::fopen(outPath, "w");
    if (outFp) std::fprintf(outFp, "target\taddr\tregion_base\tregion_size\n");

    uint32_t hitsA = 0, hitsB = 0;
    std::vector<mach_vm_address_t> bAddrs;     // collect B hits for cache mutation
    constexpr size_t kBufBytes = 1u << 20; // 1MB chunks
    std::vector<uint8_t> buf(kBufBytes);
    const task_t task = mach_task_self();

    // For each hit, also dump 64 bytes of surrounding context (-16 .. +48) so
    // the data structure shape is visible. Captured to a side log so the main
    // line stream stays clean.
    auto dumpContext = [&](const char* tag, mach_vm_address_t hitAddr) {
        const int32_t before = 16, after = 48;
        const mach_vm_address_t start = hitAddr - before;
        uint8_t ctx[64];
        mach_vm_size_t cgot = 0;
        if (mach_vm_read_overwrite(task, start, sizeof(ctx),
                                   (mach_vm_address_t)ctx, &cgot) == KERN_SUCCESS &&
            cgot == sizeof(ctx)) {
            // 4 lines x 16 bytes, with floats per 4-byte group.
            for (int row = 0; row < 4; ++row) {
                char hex[64]; char asc[20]; char fl[80];
                size_t hl = 0, al = 0, fll = 0;
                for (int c = 0; c < 16; ++c) {
                    uint8_t b = ctx[row*16 + c];
                    hl += std::snprintf(hex + hl, sizeof(hex) - hl, "%02x", b);
                    if (c < 15) hl += std::snprintf(hex + hl, sizeof(hex) - hl, " ");
                    asc[al++] = (b >= 0x20 && b < 0x7f) ? (char)b : '.';
                }
                asc[al] = 0;
                for (int g = 0; g < 4; ++g) {
                    uint32_t v;
                    std::memcpy(&v, ctx + row*16 + g*4, 4);
                    float f; std::memcpy(&f, &v, 4);
                    fll += std::snprintf(fl + fll, sizeof(fl) - fll, " %.4g", (double)f);
                }
                log_line("[scan-ctx] %s 0x%llx: %s  %s | f:%s",
                         tag,
                         (unsigned long long)(start + row*16),
                         hex, asc, fl);
            }
        }
    };

    for (const ScanRegion& r : regions) {
        for (mach_vm_size_t off = 0; off < r.size; off += kBufBytes) {
            const mach_vm_size_t chunk = std::min<mach_vm_size_t>(kBufBytes, r.size - off);
            mach_vm_size_t got = 0;
            kern_return_t kr = mach_vm_read_overwrite(
                task, r.base + off, chunk,
                (mach_vm_address_t)buf.data(), &got);
            if (kr != KERN_SUCCESS || got != chunk) continue;

            // 4-byte aligned scan only — floats are float-aligned.
            for (mach_vm_size_t i = 0; i + 4 <= got; i += 4) {
                uint32_t v;
                std::memcpy(&v, buf.data() + i, 4);
                if (v == bitsA && hitsA < maxHits) {
                    const mach_vm_address_t a = r.base + off + i;
                    log_line("[scan] hit A @ 0x%llx (region 0x%llx + 0x%llx)",
                             (unsigned long long)a,
                             (unsigned long long)r.base,
                             (unsigned long long)(off + i));
                    dumpContext("A", a);
                    if (outFp) std::fprintf(outFp, "A\t0x%llx\t0x%llx\t0x%llx\n",
                                            (unsigned long long)a,
                                            (unsigned long long)r.base,
                                            (unsigned long long)r.size);
                    ++hitsA;
                }
                if (fB && v == bitsB && hitsB < maxHits) {
                    const mach_vm_address_t a = r.base + off + i;
                    log_line("[scan] hit B @ 0x%llx", (unsigned long long)a);
                    dumpContext("B", a);
                    if (outFp) std::fprintf(outFp, "B\t0x%llx\t0x%llx\t0x%llx\n",
                                            (unsigned long long)a,
                                            (unsigned long long)r.base,
                                            (unsigned long long)r.size);
                    bAddrs.push_back(a);
                    ++hitsB;
                }
                if (hitsA >= maxHits && (!fB || hitsB >= maxHits)) goto done;
            }
        }
    }
done:
    if (outFp) std::fclose(outFp);
    log_line("[scan] done: %u hits for A (target %g), %u hits for B (target %g)",
             hitsA, (double)tA, hitsB, (double)tB);

    // Cache mutation pass: write A value into every B hit address (and keep
    // re-applying every 2s in a background thread to catch the gameplay
    // re-reading from somewhere else). Gated by TWEAKXL_SCAN_MUTATE_B=1.
    if (const char* mb = std::getenv("TWEAKXL_SCAN_MUTATE_B"); mb && *mb && !bAddrs.empty()) {
        uint32_t writeRaw;
        std::memcpy(&writeRaw, &tA, 4);
        uint32_t mutated = 0;
        for (auto addr : bAddrs) {
            kern_return_t kr = mach_vm_write(task, addr, (vm_offset_t)&writeRaw, 4);
            if (kr == KERN_SUCCESS) ++mutated;
        }
        log_line("[scan] cache-mutate: wrote A=%g to %u/%zu B addrs (%u failed)",
                 (double)tA, mutated, bAddrs.size(),
                 (uint32_t)(bAddrs.size() - mutated));
        // Background re-apply so any continuous read-back from the source
        // doesn't undo our cache write.
        auto* owned = new std::vector<mach_vm_address_t>(std::move(bAddrs));
        const uint32_t raw = writeRaw;
        std::thread([owned, raw]() {
            uint64_t pass = 1;
            const task_t t = mach_task_self();
            for (;;) {
                std::this_thread::sleep_for(std::chrono::seconds(2));
                ++pass;
                uint32_t n = 0;
                for (auto a : *owned)
                    if (mach_vm_write(t, a, (vm_offset_t)&raw, 4) == KERN_SUCCESS) ++n;
                if ((pass % 15) == 0)
                    log_line("[scan] cache-reapply pass#%llu: %u/%zu B addrs rewritten",
                             (unsigned long long)pass, n, owned->size());
            }
        }).detach();
        log_line("[scan] cache re-apply thread started (every 2s)");
    }
}

} // namespace

void ScanMemoryForFloat(const TweakDB* /*db*/) {
    std::call_once(g_scanMem_once, [] {
        // Run on a detached thread so the loader callback chain isn't blocked
        // during the sleep + scan.
        std::thread(DoScanMemoryForFloat).detach();
    });
}

// ── P1.21: aggressive shotgun mutation ──────────────────────────────────────
namespace {

std::once_flag g_unleash_once;

struct UnleashTarget {
    uintptr_t addr;        // exact byte address to write
    float     newValue;    // factor × original
};

uint32_t UnleashOnePass(const std::vector<UnleashTarget>& targets) {
    uint32_t n = 0;
    const task_t task = mach_task_self();
    for (const UnleashTarget& t : targets) {
        uint32_t raw;
        std::memcpy(&raw, &t.newValue, 4);
        if (mach_vm_write(task, t.addr, (vm_offset_t)&raw, 4) == KERN_SUCCESS) ++n;
    }
    return n;
}

void DoCinemaUnleash(const TweakDB* db) {
    if (!std::getenv("TWEAKXL_UNLEASH")) {
        log_line("[unleash] skipped (set TWEAKXL_UNLEASH=1 to enable)");
        return;
    }
    if (!db) { log_line("[unleash] skipped (db=null)"); return; }

    float factor = 1000.0f, minV = 1.0f, maxV = 200.0f;
    uint32_t startOff = 0x48, endOff = 0x100, repeatSec = 2;
    std::string filterSubstr;
    if (const char* e = std::getenv("TWEAKXL_UNLEASH_FACTOR"); e && *e) factor = (float)std::strtod(e, nullptr);
    if (const char* e = std::getenv("TWEAKXL_UNLEASH_MIN"); e && *e) minV = (float)std::strtod(e, nullptr);
    if (const char* e = std::getenv("TWEAKXL_UNLEASH_MAX"); e && *e) maxV = (float)std::strtod(e, nullptr);
    if (const char* e = std::getenv("TWEAKXL_UNLEASH_OFFSET_START"); e && *e) startOff = (uint32_t)std::strtoul(e, nullptr, 0);
    if (const char* e = std::getenv("TWEAKXL_UNLEASH_OFFSET_END"); e && *e) endOff = (uint32_t)std::strtoul(e, nullptr, 0);
    if (const char* e = std::getenv("TWEAKXL_UNLEASH_REPEAT"); e && *e) repeatSec = (uint32_t)std::strtoul(e, nullptr, 10);
    if (const char* e = std::getenv("TWEAKXL_UNLEASH_NAME_SUBSTR"); e && *e) filterSubstr = e;

    log_line("[unleash] factor=%g range=[%g,%g] scan=+0x%x..+0x%x repeat=%us filter='%s'",
             (double)factor, (double)minV, (double)maxV,
             startOff, endOff, repeatSec, filterSubstr.c_str());

    // Walk only NAMED records (from InheritanceMap) so we can filter by name
    // substring AND skip auto-generated _inline records (their bodies are
    // less likely to hold gameplay-affecting floats and more likely garbage).
    const HashMap* flats = GetFlatsMapCandidate(db);
    if (!flats) { log_line("[unleash] no +0x58 map"); return; }
    const HashMode fmode = GetFlatsHashMode();
    const std::string namesPath = ResolveRecordNamesPath();
    std::ifstream in(namesPath);
    if (!in) {
        log_line("[unleash] cannot open names file %s", namesPath.c_str());
        return;
    }

    std::vector<UnleashTarget> targets;
    uint32_t recordsScanned = 0, recordsMatched = 0;
    std::string line;
    while (std::getline(in, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
        if (line.empty() || line[0] == '#') continue;
        if (!filterSubstr.empty() && line.find(filterSubstr) == std::string::npos) continue;
        ++recordsScanned;

        TweakDBID id{};
        id.nameHash   = Crc32(line.data(), line.size());
        id.nameLength = (uint8_t)std::min<size_t>(line.size(), 255);
        const uint8_t* hit = Lookup(flats, id, fmode);
        if (!hit) continue;

        uintptr_t p10 = 0;
        SafeReadPtr(reinterpret_cast<uintptr_t>(hit) + 0x10, &p10);
        if (!IsPlausibleUserPtr(p10)) continue;
        ++recordsMatched;

        for (uint32_t off = startOff; off + 4 <= endOff; off += 4) {
            uint32_t raw = 0;
            if (!SafeReadU32(p10 + off, &raw)) continue;
            float v; std::memcpy(&v, &raw, 4);
            if (!std::isfinite(v)) continue;
            // Skip negatives — modifiers and reductions get MORE negative when
            // multiplied by a positive factor, which broke damage-reduction
            // math and crashed the firing pipeline last run. Positive-only
            // is safer and still catches base-damage values.
            if (v < minV || v > maxV) continue;
            float newV = v * factor;
            if (!std::isfinite(newV)) continue;
            if (newV > 1.0e7f) newV = 1.0e7f;    // cap at 10M to avoid overflow
            targets.push_back({p10 + off, newV});
        }
    }

    log_line("[unleash] scanned %u named records (matched %u in DB), built %zu mutation targets",
             recordsScanned, recordsMatched, targets.size());

    // Sample a few so the audit trail shows what we're touching.
    for (size_t k = 0; k < std::min<size_t>(10, targets.size()); ++k) {
        uint32_t raw = 0;
        SafeReadU32(targets[k].addr, &raw);
        float before; std::memcpy(&before, &raw, 4);
        log_line("[unleash]   sample[%zu] @0x%llx before=%g new=%g",
                 k, (unsigned long long)targets[k].addr,
                 (double)before, (double)targets[k].newValue);
    }

    const uint32_t pass1 = UnleashOnePass(targets);
    log_line("[unleash] PASS#1: wrote %u of %zu targets", pass1, targets.size());

    if (repeatSec > 0 && !targets.empty()) {
        auto* owned = new std::vector<UnleashTarget>(std::move(targets));
        const uint32_t period = repeatSec;
        std::thread([owned, period]() {
            uint64_t pass = 1;
            for (;;) {
                std::this_thread::sleep_for(std::chrono::seconds(period));
                ++pass;
                const uint32_t n = UnleashOnePass(*owned);
                if ((pass % 10) == 0)
                    log_line("[unleash] re-apply pass#%llu: %u/%zu",
                             (unsigned long long)pass, n, owned->size());
            }
        }).detach();
        log_line("[unleash] re-apply thread started (every %us)", repeatSec);
    }
}

} // namespace

void CinemaUnleash(const TweakDB* db) {
    std::call_once(g_unleash_once, [db] { DoCinemaUnleash(db); });
}

void DumpFlatsSample(const TweakDB* db, uint32_t maxEntries) {
    std::call_once(g_dumpFlats_once, [db, maxEntries] {
        // Env override wins so the user can crank it up for a deeper sample.
        uint32_t cap = maxEntries;
        if (const char* env = std::getenv("TWEAKXL_DUMP_FLATS_MAX"); env && *env) {
            char* end = nullptr;
            unsigned long v = std::strtoul(env, &end, 10);
            if (end != env && v > 0 && v <= 1000000) cap = (uint32_t)v;
        }
        DoDumpFlatsSample(db, cap);
    });
}

// ═════════════════════════════════════════════════════════════════════════════
// F-031: CORRECT flat path — the REAL flats live in the SortedUniqueArray @
// db+0x40 (NOT the +0x58 map, which is recordsByID per F-029). A flat is a
// TweakDBID whose low-40-bit key = {nameHash, nameLength} and whose high-24-bit
// tdbOffset (big-endian, bytes [5][6][7]) indexes the flat-data buffer (+0x148).
// A scalar FlatValue is 0x10 bytes: [vtable @ +0x00][data @ +0x08].
// ═════════════════════════════════════════════════════════════════════════════
namespace {

// View of the +0x40 SortedUniqueArray<TweakDBID> header (F-031 field order).
struct FlatsArrayView {
    uintptr_t entries  = 0;   // +0x00
    uint32_t  capacity = 0;   // +0x08
    uint32_t  size     = 0;   // +0x0C
    uint32_t  flags    = 0;   // +0x10  (bit0 = NotSorted)
    bool      ok       = false;
};

FlatsArrayView ReadFlatsArrayHeader(const TweakDB* db) {
    FlatsArrayView v;
    if (!db) return v;
    const uintptr_t base = reinterpret_cast<uintptr_t>(db) + 0x40;
    uintptr_t entries = 0;
    if (!SafeReadPtr(base + 0x00, &entries)) return v;
    SafeReadU32(base + 0x08, &v.capacity);
    SafeReadU32(base + 0x0c, &v.size);
    SafeReadU32(base + 0x10, &v.flags);
    v.entries = entries;
    v.ok = true;
    return v;
}

bool ReadFlatEntry(uintptr_t entriesBase, uint32_t i, TweakDBID* out) {
    uint64_t raw = 0;
    if (!SafeReadU64(entriesBase + static_cast<uint64_t>(i) * 8u, &raw)) return false;
    std::memcpy(out, &raw, sizeof(*out));
    return true;
}

// tdbOffset = high 24 bits, big-endian in bytes [5][6][7] (F-031).
uint32_t TdbOffsetOf(const TweakDBID& id) {
    return (static_cast<uint32_t>(id.tdbOffset[0]) << 16)
         | (static_cast<uint32_t>(id.tdbOffset[1]) << 8)
         |  static_cast<uint32_t>(id.tdbOffset[2]);
}

} // namespace

int64_t ResolveFlatOffset(const TweakDB* db, TweakDBID key) {
    const FlatsArrayView fa = ReadFlatsArrayHeader(db);
    if (!fa.ok || !fa.entries || fa.size == 0) return -1;

    // Sort order (F-031 / FUN_102b139b8): by nameHash (u32) then nameLength (u8).
    auto keyLess = [&](const TweakDBID& e) {
        return e.nameHash < key.nameHash
            || (e.nameHash == key.nameHash && e.nameLength < key.nameLength);
    };
    auto keyEq = [&](const TweakDBID& e) {
        return e.nameHash == key.nameHash && e.nameLength == key.nameLength;
    };

    const bool notSorted = (fa.flags & 1u) != 0u;
    TweakDBID e;
    if (!notSorted) {
        uint32_t lo = 0, hi = fa.size;
        while (lo < hi) {
            const uint32_t mid = lo + (hi - lo) / 2;
            if (!ReadFlatEntry(fa.entries, mid, &e)) return -1;
            if (keyLess(e)) lo = mid + 1; else hi = mid;
        }
        if (lo < fa.size && ReadFlatEntry(fa.entries, lo, &e) && keyEq(e))
            return static_cast<int64_t>(TdbOffsetOf(e));
        return -1;
    }
    // Lazy-sorted array: fall back to a safe linear scan.
    for (uint32_t i = 0; i < fa.size; ++i) {
        if (!ReadFlatEntry(fa.entries, i, &e)) return -1;
        if (keyEq(e)) return static_cast<int64_t>(TdbOffsetOf(e));
    }
    return -1;
}

void EnsureRuntimeAccess(TweakDB* db) {
    if (!db) return;
    uint8_t zero = 0;
    SafeWrite(reinterpret_cast<uintptr_t>(db) + 0x160, &zero, 1);  // unk160 gate (F-031, inferred)
}

// ── Interning-safe flat write (F-031) ────────────────────────────────────────
// The flat-value buffer is pooled: one FlatValue per distinct value, shared by
// every flat with that value. Editing it IN PLACE corrupts every sharer. The
// safe path allocates a NEW FlatValue and repoints ONLY the target flat's
// tdbOffset. The buffer has no slack (game never upsizes), so we move it to a
// larger malloc'd buffer once, rebasing the +0x108 defaultValues absolute
// FlatValue* pointers (flats store OFFSETS, so they survive the move).
namespace {

// Return the address of a flat's entry in the +0x40 SortedUniqueArray (so its
// tdbOffset can be rewritten), or 0 if absent.
uintptr_t ResolveFlatEntryAddr(const TweakDB* db, TweakDBID key) {
    const FlatsArrayView fa = ReadFlatsArrayHeader(db);
    if (!fa.ok || !fa.entries || fa.size == 0) return 0;
    const bool notSorted = (fa.flags & 1u) != 0u;
    auto less = [&](const TweakDBID& e){ return e.nameHash < key.nameHash || (e.nameHash==key.nameHash && e.nameLength<key.nameLength); };
    auto eq   = [&](const TweakDBID& e){ return e.nameHash==key.nameHash && e.nameLength==key.nameLength; };
    TweakDBID e;
    if (!notSorted) {
        uint32_t lo = 0, hi = fa.size;
        while (lo < hi) { uint32_t mid = lo+(hi-lo)/2; if (!ReadFlatEntry(fa.entries, mid, &e)) return 0; if (less(e)) lo=mid+1; else hi=mid; }
        if (lo < fa.size && ReadFlatEntry(fa.entries, lo, &e) && eq(e)) return fa.entries + (uint64_t)lo*8;
        return 0;
    }
    for (uint32_t i=0;i<fa.size;++i){ if(!ReadFlatEntry(fa.entries,i,&e))return 0; if(eq(e))return fa.entries+(uint64_t)i*8; }
    return 0;
}

// Move the flat buffer to a larger malloc'd block (one-time), rebasing +0x108
// defaultValues pointers + the 4 buffer fields. Returns false on failure.
bool GrowFlatBuffer(TweakDB* db, uint32_t extra) {
    const uintptr_t dbp = reinterpret_cast<uintptr_t>(db);
    uintptr_t base = 0, end = 0; uint32_t cap = 0;
    SafeReadPtr(dbp + 0x148, &base); SafeReadU32(dbp + 0x150, &cap); SafeReadPtr(dbp + 0x158, &end);
    if (!base || !end || end < base) return false;
    const uint32_t used = (uint32_t)(end - base);
    uint32_t newCap = cap + extra;
    if (newCap > 0x00FFFFFFu) newCap = 0x00FFFFFFu;
    if (newCap <= used) return false;             // can't grow further
    void* nb = nullptr;
    if (posix_memalign(&nb, 16, newCap) != 0 || !nb) return false;
    std::memcpy(nb, reinterpret_cast<void*>(base), used);
    const uintptr_t newBase = reinterpret_cast<uintptr_t>(nb);

    // Rebase defaultValues (+0x108) absolute FlatValue* pointers.
    const uintptr_t dv = dbp + 0x108;
    uintptr_t dvEntries = 0; uint32_t dvCount = 0, dvStride = 0;
    SafeReadU32(dv + 0x08, &dvCount); SafeReadPtr(dv + 0x10, &dvEntries); SafeReadU32(dv + 0x1c, &dvStride);
    if (dvEntries && dvStride && dvCount < 100000u) {
        for (uint32_t i = 0; i < dvCount; ++i) {
            const uintptr_t e = dvEntries + (uint64_t)i * dvStride;
            uintptr_t fv = 0; SafeReadPtr(e + 0x10, &fv);
            if (fv >= base && fv < end) { uintptr_t nfv = newBase + (fv - base); SafeWrite(e + 0x10, &nfv, 8); }
        }
    }
    const uintptr_t newEnd = newBase + used;
    SafeWrite(dbp + 0x00, &newBase, 8);   // staticFlatDataBuffer
    SafeWrite(dbp + 0x148, &newBase, 8);  // flatDataBuffer
    SafeWrite(dbp + 0x150, &newCap, 4);   // capacity
    SafeWrite(dbp + 0x158, &newEnd, 8);   // end
    log_line("[flat-grow] moved buffer 0x%llx(used=%u cap=%u) -> 0x%llx(cap=%u) defaultValues=%u",
             (unsigned long long)base, used, cap, (unsigned long long)newBase, newCap, dvCount);
    return true;
}

// Append a scalar FlatValue ([vtable][data@+0x08]); grow the buffer if needed.
// Returns the new tdbOffset (<= 0xFFFFFF) or -1.
int64_t AppendFlatValue(TweakDB* db, uint64_t vtable, const uint8_t* data, uint32_t width) {
    const uintptr_t dbp = reinterpret_cast<uintptr_t>(db);
    uintptr_t base = 0, end = 0; uint32_t cap = 0;
    SafeReadPtr(dbp + 0x148, &base); SafeReadU32(dbp + 0x150, &cap); SafeReadPtr(dbp + 0x158, &end);
    uintptr_t slot = (end + 7) & ~(uintptr_t)7;
    if (slot + 0x10 > base + cap) {
        if (!GrowFlatBuffer(db, 1u << 20)) return -1;   // +1MB
        SafeReadPtr(dbp + 0x148, &base); SafeReadU32(dbp + 0x150, &cap); SafeReadPtr(dbp + 0x158, &end);
        slot = (end + 7) & ~(uintptr_t)7;
        if (slot + 0x10 > base + cap) return -1;
    }
    const int64_t off = (int64_t)(slot - base);
    if (off > 0xFFFFFF) return -1;                       // tdbOffset is 24-bit
    SafeWrite(slot + 0x00, &vtable, 8);
    uint8_t buf[8] = {0}; std::memcpy(buf, data, width);
    SafeWrite(slot + 0x08, buf, width);
    uintptr_t newEnd = slot + 0x10;
    SafeWrite(dbp + 0x158, &newEnd, 8);
    return off;
}

} // namespace

bool EditScalarFlatSafe(TweakDB* db, TweakDBID flatId, FlatValue v) {
    if (!db || !IsScalarFlatType(v.type)) return false;
    const uintptr_t entry = ResolveFlatEntryAddr(db, flatId);
    if (!entry) { log_line("[flat-safe] MISS id=<0x%08x>", flatId.nameHash); return false; }
    // Current FlatValue → clone its vtable (preserves exact type ABI).
    TweakDBID cur{}; { uint64_t raw=0; SafeReadU64(entry,&raw); std::memcpy(&cur,&raw,8); }
    const uint32_t curOff = (uint32_t)(((uint32_t)cur.tdbOffset[0]<<16)|((uint32_t)cur.tdbOffset[1]<<8)|cur.tdbOffset[2]);
    uintptr_t base=0; SafeReadPtr(reinterpret_cast<uintptr_t>(db)+0x148,&base);
    uint64_t vtable=0; SafeReadU64(base + curOff, &vtable);
    if (!vtable) return false;

    uint8_t bytes[4]={0}; const uint32_t width = ScalarFlatTypeSize(v.type);
    switch (v.type) {
        case FlatType::Int32: { int32_t x = std::holds_alternative<int32_t>(v.value)?std::get<int32_t>(v.value):std::holds_alternative<uint32_t>(v.value)?(int32_t)std::get<uint32_t>(v.value):0; std::memcpy(bytes,&x,4); break; }
        case FlatType::Float: { if(!std::holds_alternative<float>(v.value))return false; float x=std::get<float>(v.value); std::memcpy(bytes,&x,4); break; }
        case FlatType::Bool:  bytes[0]=(std::holds_alternative<bool>(v.value)&&std::get<bool>(v.value))?1:0; break;
        default: return false;
    }

    EnsureRuntimeAccess(db);
    const int64_t newOff = AppendFlatValue(db, vtable, bytes, width);
    if (newOff < 0) { log_line("[flat-safe] alloc failed id=<0x%08x>", flatId.nameHash); return false; }
    // Repoint ONLY this flat: rewrite its entry's tdbOffset (BE bytes [5][6][7]).
    uint8_t be[3] = { (uint8_t)((newOff>>16)&0xFF), (uint8_t)((newOff>>8)&0xFF), (uint8_t)(newOff&0xFF) };
    SafeWrite(entry + 5, be, 3);
    log_line("[flat-safe] id=<0x%08x len=%u> %u -> new FlatValue @0x%06llx (own copy, no interning)",
             flatId.nameHash, flatId.nameLength, curOff, (unsigned long long)newOff);
    return true;
}

// Read a scalar flat's typed value via the +0x40 path (F-031): resolve → read
// FlatValue [vtable@+0x00][data@+0x08]; the vtable identifies the scalar type.
// Returns nullopt if absent or non-scalar. Used by the applicator for snapshots.
std::optional<FlatValue> ReadScalarFlat(const TweakDB* db, TweakDBID id) {
    if (!db || !db->flatDataBuffer) return std::nullopt;
    const int64_t off = ResolveFlatOffset(db, id);
    if (off < 0) return std::nullopt;
    const uintptr_t fv = reinterpret_cast<uintptr_t>(db->flatDataBuffer) + static_cast<uint64_t>(off);
    uint64_t vt = 0;
    if (!SafeReadU64(fv, &vt)) return std::nullopt;
    uint32_t raw = 0;
    if (!SafeReadU32(fv + 0x08, &raw)) return std::nullopt;

    FlatValue out;
    if (vt == StaticToRuntime(0x106e925f0)) {        // Float
        out.type = FlatType::Float; float f; std::memcpy(&f, &raw, 4); out.value = f;
    } else if (vt == StaticToRuntime(0x106e926f8)) { // Int32
        out.type = FlatType::Int32; int32_t i; std::memcpy(&i, &raw, 4); out.value = i;
    } else if (vt == StaticToRuntime(0x106e92d78)) { // Bool
        out.type = FlatType::Bool; out.value = (raw & 0xFFu) != 0u;
    } else {
        return std::nullopt;  // non-scalar (String/CName/array/...) — not this layer
    }
    return out;
}

bool EditScalarFlatInPlace(TweakDB* db, TweakDBID flatId, FlatValue v) {
    if (!db) return false;
    if (!IsScalarFlatType(v.type)) {
        log_line("[flat-edit] REFUSED id=<0x%08x>: non-scalar type", flatId.nameHash);
        return false;
    }
    const int64_t off = ResolveFlatOffset(db, flatId);
    if (off < 0) {
        log_line("[flat-edit] MISS id=<0x%08x len=%u>: absent from +0x40 flats array",
                 flatId.nameHash, flatId.nameLength);
        return false;
    }
    const uint8_t* buf = db->flatDataBuffer;   // +0x148
    if (!buf) { log_line("[flat-edit] no flat buffer"); return false; }

    uint8_t bytes[4] = {0};
    const uint32_t width = ScalarFlatTypeSize(v.type);
    switch (v.type) {
        case FlatType::Int32: {
            int32_t x = std::holds_alternative<int32_t>(v.value)  ? std::get<int32_t>(v.value)
                      : std::holds_alternative<uint32_t>(v.value) ? static_cast<int32_t>(std::get<uint32_t>(v.value))
                      : 0;
            std::memcpy(bytes, &x, 4);
            break;
        }
        case FlatType::Float: {
            if (!std::holds_alternative<float>(v.value)) {
                log_line("[flat-edit] REFUSED id=<0x%08x>: Float variant mismatch", flatId.nameHash);
                return false;
            }
            float x = std::get<float>(v.value);
            std::memcpy(bytes, &x, 4);
            break;
        }
        case FlatType::Bool:
            bytes[0] = (std::holds_alternative<bool>(v.value) && std::get<bool>(v.value)) ? 1 : 0;
            break;
        default:
            return false;
    }

    EnsureRuntimeAccess(db);
    const uintptr_t dataAddr = reinterpret_cast<uintptr_t>(buf) + static_cast<uint64_t>(off) + 0x08;
    const bool ok = SafeWrite(dataAddr, bytes, width);
    log_line("[flat-edit] id=<0x%08x len=%u> tdbOff=0x%06llx data@%p width=%u wrote=%d",
             flatId.nameHash, flatId.nameLength, (unsigned long long)off, (void*)dataAddr, width, ok ? 1 : 0);
    return ok;
}

// GROUND TRUTH: after editing a flat, ask the GAME's own GetFlat accessor
// (FUN_102b76708) what value it returns — does the game see our edit? Tests both
// editors (in-place + interning-safe) and compares to our own re-read. This is
// the honest verification: if the game's GetFlat doesn't return our value, our
// edit is NOT reaching the game, regardless of what our own reader says.
// Env-gated TWEAKXL_VERIFY_GAME=1. Edits are restored after.
void VerifyGameSeesEdit(TweakDB* db) {
    if (!db || !std::getenv("TWEAKXL_VERIFY_GAME")) {
        if (db) log_line("[verify-game] skipped (set TWEAKXL_VERIFY_GAME=1)");
        return;
    }
    using GetFlatFn = void* (*)(void*, uint64_t);
    GetFlatFn gGetFlat = reinterpret_cast<GetFlatFn>(StaticToRuntime(0x102b76708));
    auto idRaw = [](TweakDBID id){ uint64_t r=0; std::memcpy(&r,&id,8); return r; };

    auto gameValue = [&](TweakDBID id, uint64_t* vtOut) -> double {
        void* fv = gGetFlat(db, idRaw(id));
        if (!fv) { if (vtOut)*vtOut=0; return -1e30; }
        uint64_t vt=0; SafeReadU64(reinterpret_cast<uintptr_t>(fv), &vt); if (vtOut)*vtOut=vt;
        uint32_t raw=0; SafeReadU32(reinterpret_cast<uintptr_t>(fv)+0x08, &raw);
        float f; std::memcpy(&f,&raw,4); return f;
    };
    auto ourValue = [&](TweakDBID id) -> double {
        int64_t off = ResolveFlatOffset(db, id);
        if (off < 0 || !db->flatDataBuffer) return -1e30;
        uint32_t raw=0; SafeReadU32(reinterpret_cast<uintptr_t>(db->flatDataBuffer)+off+0x08, &raw);
        float f; std::memcpy(&f,&raw,4); return f;
    };

    struct Case { const char* name; bool useSafe; };
    Case cases[] = { {"BaseStats.Health.max", false}, {"BaseStats.Health.min", true} };
    for (auto& c : cases) {
        TweakDBID id = MakeCompactId(c.name);
        if (ResolveFlatOffset(db, id) < 0) { log_line("[verify-game] %s does NOT resolve", c.name); continue; }
        auto snap = ReadScalarFlat(db, id);
        uint64_t gvt=0;
        double gBefore = gameValue(id,&gvt), oBefore = ourValue(id);
        FlatValue nv; nv.type = FlatType::Float; nv.value = 4242.0f;
        bool ok = c.useSafe ? EditScalarFlatSafe(db, id, nv) : EditScalarFlatInPlace(db, id, nv);
        double gAfter = gameValue(id,nullptr), oAfter = ourValue(id);
        log_line("[verify-game] %s (%s) editOk=%d gameVtbl=0x%llx | OUR %g->%g | GAME-GetFlat %g->%g | GAME-SEES-EDIT=%s",
                 c.name, c.useSafe?"safe":"inplace", ok?1:0, (unsigned long long)gvt,
                 oBefore, oAfter, gBefore, gAfter, (gAfter==4242.0)?"YES":"NO");
        if (snap.has_value()) { c.useSafe ? EditScalarFlatSafe(db,id,*snap) : EditScalarFlatInPlace(db,id,*snap); }
    }
}

// FOOL-PROOF stat-flat identifier: for each candidate flat, read its LIVE value
// via the GAME's own GetFlat (FUN_102b76708) and report which one's value is
// physically the stat (base HP ~150-500, base RAM ~4-16). Selection is by VALUE
// through the game's accessor, not by name shape — so the BaseStats.Health.max
// = -1e7 sentinel trap is culled by range. No save needed. Env TWEAKXL_FIND_STAT_FLAT=1.
void FindStatFlatByValue(TweakDB* db) {
    if (!db || !std::getenv("TWEAKXL_FIND_STAT_FLAT")) {
        if (db) log_line("[find-stat] skipped (set TWEAKXL_FIND_STAT_FLAT=1)");
        return;
    }
    // Candidate file: TWEAKXL_STAT_CANDIDATES_FILE wins, else dylib-relative.
    std::string path;
    if (const char* env = std::getenv("TWEAKXL_STAT_CANDIDATES_FILE"); env && *env) {
        path = env;
    } else {
        bool fe = false;
        std::string base = ResolveCandidatesPath(&fe);          // .../tools/probes/candidate_flats.txt
        const auto slash = base.find_last_of('/');
        path = (slash == std::string::npos ? std::string("tools/probes")
                                           : base.substr(0, slash)) + "/stat_candidate_flats.txt";
    }
    std::ifstream in(path);
    if (!in) { log_line("[find-stat] cannot open candidates '%s'", path.c_str()); return; }
    log_line("[find-stat] reading %s", path.c_str());

    using GetFlatFn = void* (*)(void*, uint64_t);
    GetFlatFn gGetFlat = reinterpret_cast<GetFlatFn>(StaticToRuntime(0x102b76708));
    const uintptr_t floatVft = StaticToRuntime(0x106e925f0);
    const uintptr_t int32Vft = StaticToRuntime(0x106e926f8);
    const uintptr_t boolVft  = StaticToRuntime(0x106e92d78);

    std::string bestHp, bestRam; double bestHpV = 0, bestRamV = 0;
    auto consider = [](const std::string& name, double v, bool inRange,
                       std::string& best, double& bestV, const char* hint) {
        if (!inRange) return;
        const bool nameMatch = name.find(hint) != std::string::npos;
        const bool bestHasHint = !best.empty() && best.find(hint) != std::string::npos;
        if (best.empty() || (nameMatch && !bestHasHint)) { best = name; bestV = v; }
    };

    std::string line;
    while (std::getline(in, line)) {
        const std::string name = StripLine(line);
        if (name.empty()) continue;
        TweakDBID id = MakeCompactId(name);
        if (ResolveFlatOffset(db, id) < 0) { log_line("[find-stat] name=%s resolves=no", name.c_str()); continue; }
        uint64_t idr = 0; std::memcpy(&idr, &id, 8);
        void* fv = gGetFlat(db, idr);
        if (!fv) { log_line("[find-stat] name=%s GetFlat=null", name.c_str()); continue; }
        uint64_t vt = 0; SafeReadU64(reinterpret_cast<uintptr_t>(fv), &vt);
        uint32_t raw = 0; SafeReadU32(reinterpret_cast<uintptr_t>(fv) + 0x08, &raw);
        const char* tn = (vt == floatVft) ? "Float" : (vt == int32Vft) ? "Int32"
                       : (vt == boolVft) ? "Bool" : "?";
        float asF; std::memcpy(&asF, &raw, 4);
        int32_t asI; std::memcpy(&asI, &raw, 4);
        const double v = (vt == floatVft) ? (double)asF : (double)asI;
        const bool hp  = (v >= 150.0 && v <= 500.0);
        const bool ram = (v >= 4.0   && v <= 16.0);
        log_line("[find-stat] name=%s resolves=yes type=%s asF=%g asI=%d HP_RANGE=%s RAM_RANGE=%s",
                 name.c_str(), tn, asF, asI, hp ? "yes" : "no", ram ? "yes" : "no");
        consider(name, v, hp,  bestHp,  bestHpV,  "Health");
        consider(name, v, ram, bestRam, bestRamV, "Memory");
    }
    log_line("[find-stat] VERDICT hp-flat=%s(val=%g) ram-flat=%s(val=%g)",
             bestHp.empty()  ? "<none>" : bestHp.c_str(),  bestHpV,
             bestRam.empty() ? "<none>" : bestRam.c_str(), bestRamV);
}

// Identify which item/cyberware "Memory Boost" modifier records carry a stored
// ConstantStatModifier whose statType is the Memory (cyberdeck RAM) stat, so we
// can double its .value to visibly raise RAM. Unlike base HP/RAM (computed at
// entity-init, F-039), these are STORED scalar flats with KNOWN names
// (`Items.*_inlineN`), so we read/edit them by name. Reads <record>.statType (a
// TweakDBID flat; value 8 bytes @ fv+0x08), .value (Float), .modifierType (CName)
// via the game's own GetFlat. With TWEAKXL_DOUBLE_STATS=1, doubles each matching
// .value (interning-safe) + UpdateRecord. Env TWEAKXL_ID_MEM_MODS=1. No save.
void IdentifyStatModifiers(TweakDB* db) {
    if (!db || !std::getenv("TWEAKXL_ID_MEM_MODS")) {
        if (db) log_line("[mem-mod] skipped (set TWEAKXL_ID_MEM_MODS=1)");
        return;
    }
    if (!db->flatDataBuffer) { log_line("[mem-mod] no flat buffer"); return; }
    std::string path;
    if (const char* env = std::getenv("TWEAKXL_MEMMOD_FILE"); env && *env) {
        path = env;
    } else {
        bool fe = false;
        std::string base = ResolveCandidatesPath(&fe);
        const auto slash = base.find_last_of('/');
        path = (slash == std::string::npos ? std::string("tools/probes")
                                           : base.substr(0, slash)) + "/memory_modifier_records.txt";
    }
    std::ifstream in(path);
    if (!in) { log_line("[mem-mod] cannot open %s", path.c_str()); return; }
    log_line("[mem-mod] reading %s", path.c_str());

    const bool doDouble = std::getenv("TWEAKXL_DOUBLE_STATS") != nullptr;
    const uint32_t MEMORY_STAT = 0x4c58e91c, HEALTH_STAT = 0x68effe3a;
    const uint32_t MEMORY_POOL = 0x41af9eeb, HEALTH_POOL = 0xf319225a;

    using GetFlatFn = void* (*)(void*, uint64_t);
    GetFlatFn gGetFlat = reinterpret_cast<GetFlatFn>(StaticToRuntime(0x102b76708));
    const uintptr_t floatVft = StaticToRuntime(0x106e925f0);
    auto idRaw = [](TweakDBID id){ uint64_t r = 0; std::memcpy(&r, &id, 8); return r; };

    int scanned = 0, matches = 0;
    std::string line;
    while (std::getline(in, line)) {
        const std::string rec = StripLine(line);
        if (rec.empty()) continue;
        ++scanned;
        // statType is a TweakDBID flat: the 8-byte target id lives at fv+0x08.
        TweakDBID stId = MakeCompactId(rec + ".statType");
        if (ResolveFlatOffset(db, stId) < 0) continue;           // not a stat modifier
        void* sfv = gGetFlat(db, idRaw(stId));
        if (!sfv) continue;
        uint64_t stVal = 0; SafeReadU64(reinterpret_cast<uintptr_t>(sfv) + 0x08, &stVal);
        const uint32_t stHash = static_cast<uint32_t>(stVal & 0xffffffffu);
        const char* which = stHash == MEMORY_STAT ? "Memory" : stHash == HEALTH_STAT ? "Health"
                          : stHash == MEMORY_POOL ? "MemoryPool" : stHash == HEALTH_POOL ? "HealthPool"
                          : nullptr;
        if (!which) continue;                                    // targets some other stat
        // .value (Float)
        TweakDBID vId = MakeCompactId(rec + ".value");
        void* vfv = gGetFlat(db, idRaw(vId));
        uint64_t vvt = 0; uint32_t vraw = 0; float val = 0; bool isFloat = false;
        if (vfv) {
            SafeReadU64(reinterpret_cast<uintptr_t>(vfv), &vvt);
            SafeReadU32(reinterpret_cast<uintptr_t>(vfv) + 0x08, &vraw);
            if (vvt == floatVft) { std::memcpy(&val, &vraw, 4); isFloat = true; }
        }
        // .modifierType (CName) — raw hash for context (Additive vs Multiplier).
        TweakDBID mId = MakeCompactId(rec + ".modifierType");
        void* mfv = gGetFlat(db, idRaw(mId));
        uint64_t mhash = 0; if (mfv) SafeReadU64(reinterpret_cast<uintptr_t>(mfv) + 0x08, &mhash);
        ++matches;
        log_line("[mem-mod] MATCH %s statType=%s value=%s%g modType=0x%llx valueFlat{hash=0x%08x len=%u}%s",
                 rec.c_str(), which, isFloat ? "" : "(non-float)", val,
                 (unsigned long long)mhash, vId.nameHash, vId.nameLength,
                 (doDouble && isFloat) ? " [DOUBLING]" : "");
        if (doDouble && isFloat) {
            FlatValue nv; nv.type = FlatType::Float; nv.value = val * 2.0f;
            const bool ok = EditScalarFlatSafe(db, vId, nv);
            UpdateRecord(db, MakeCompactId(rec));
            log_line("[mem-mod]   -> EditScalarFlatSafe(%g->%g) ok=%d + UpdateRecord",
                     val, val * 2.0f, ok ? 1 : 0);
        }
    }
    log_line("[mem-mod] done scanned=%d matches=%d (double=%d)", scanned, matches, doDouble ? 1 : 0);
}

// ═════════════════════════════════════════════════════════════════════════════
// Phase 3: self-verifying runtime stat oracle (F-037). Reaches the player's live
// StatsContainer and reads RAM/Memory through the game's own data, so the
// framework can REPORT "game shows RAM=X" (ground truth) and optionally write it.
// Needs a save loaded (player exists) — runs on a poll thread. Env TWEAKXL_STAT_ORACLE=1.
// ═════════════════════════════════════════════════════════════════════════════
namespace {

// F-038: there is NO flat engine/gameInstance global on macOS (exhaustive scan).
// So this chain can't be bootstrapped from a flat global by pure reads — it needs
// ONE system-instance pointer from a flat global (none pinned) or a code hook
// (blocked, FA-001). kEngineGlobalStatic stays 0 → oracle no-ops. The GetSystem
// mechanics (vtable+0x10) + PlayerSystem type (*0x1090d9090) below ARE correct;
// only the ROOT is missing (FA-012). Kept wired for when a root is pinned.
constexpr uintptr_t kEngineGlobalStatic     = 0;           // <-- UNPINNED (FA-012)
constexpr uintptr_t kPlayerSystemTypeGlobal = 0x1090d9090; // CClass* for PlayerSystem (F-038)

// engine global → +0x308 framework → +0x10 gameInstance → GetSystem(PlayerSystem)
//   → +0x604c0 playerEntity → +0x18 StatsContainer. Heavily guarded; the only
//   non-SafeRead step is the GetSystem vtable call into game code.
uintptr_t GetPlayerStatsContainer() {
    if (!kEngineGlobalStatic) return 0;
    uintptr_t eng = 0; if (!SafeReadPtr(StaticToRuntime(kEngineGlobalStatic), &eng) || !eng) return 0;
    uintptr_t fw = 0;  if (!SafeReadPtr(eng + 0x308, &fw) || !fw) return 0;
    uintptr_t gi = 0;  if (!SafeReadPtr(fw + 0x10, &gi) || !gi) return 0;
    uintptr_t gvt = 0; if (!SafeReadPtr(gi, &gvt) || !gvt) return 0;
    uintptr_t getSysFn = 0; if (!SafeReadPtr(gvt + 0x10, &getSysFn) || !getSysFn) return 0;  // GetSystem = vtable+0x10 (F-038)
    uintptr_t psType = 0; if (!SafeReadPtr(StaticToRuntime(kPlayerSystemTypeGlobal), &psType) || !psType) return 0;
    log_line("[oracle] eng=0x%llx fw=0x%llx gi=0x%llx getSys=0x%llx psType=0x%llx",
             (unsigned long long)eng, (unsigned long long)fw, (unsigned long long)gi,
             (unsigned long long)getSysFn, (unsigned long long)psType);
    using GetSysFn = void* (*)(void*, void*);
    void* ps = reinterpret_cast<GetSysFn>(getSysFn)(reinterpret_cast<void*>(gi), reinterpret_cast<void*>(psType));
    if (!ps) { log_line("[oracle] GetSystem(PlayerSystem) returned null"); return 0; }
    uintptr_t ent = 0; if (!SafeReadPtr(reinterpret_cast<uintptr_t>(ps) + 0x604c0, &ent) || !ent) {
        log_line("[oracle] no player entity @ playerSystem+0x604c0 (no save loaded yet?)"); return 0;
    }
    uintptr_t cont = 0; SafeReadPtr(ent + 0x18, &cont);
    log_line("[oracle] playerSystem=%p playerEntity=0x%llx statsContainer=0x%llx",
             ps, (unsigned long long)ent, (unsigned long long)cont);
    return cont;
}

// Dump the StatsContainer (F-030 layout) + identify RAM/Memory by value (~4-16).
// Reads raw header first so the inline-vs-pointer layout can be confirmed live.
void OracleDumpContainer(uintptr_t container) {
    if (!container) return;
    for (int o = 0x40; o < 0x70; o += 8) {
        uint64_t w = 0; SafeReadU64(container + o, &w);
        log_line("[oracle] container+0x%02x = 0x%016llx", o, (unsigned long long)w);
    }
    uint32_t count = 0; SafeReadU32(container + 0x5c, &count);
    uintptr_t idArr = 0, entArr = 0;
    SafeReadPtr(container + 0x50, &idArr);
    SafeReadPtr(container + 0x60, &entArr);
    log_line("[oracle] count=%u idArrPtr=0x%llx entPtr=0x%llx",
             count, (unsigned long long)idArr, (unsigned long long)entArr);
    if (count == 0 || count > 4096 || !idArr || !entArr) { log_line("[oracle] implausible container header"); return; }
    for (uint32_t i = 0; i < count && i < 128; ++i) {
        uint32_t id = 0; SafeReadU32(idArr + (uint64_t)i * 4, &id);
        uint32_t raw = 0; SafeReadU32(entArr + (uint64_t)i * 0x10 + 0x0c, &raw);
        float v; std::memcpy(&v, &raw, 4);
        const bool ram = (v >= 4.0f && v <= 16.0f);
        log_line("[oracle] stat[%u] id=0x%08x value=%g%s", i, id, v, ram ? "  <-- RAM-RANGE" : "");
    }
}

std::once_flag g_oracle_once;

} // namespace

void StatOracle(TweakDB* /*db*/) {
    if (!std::getenv("TWEAKXL_STAT_ORACLE")) { log_line("[oracle] skipped (set TWEAKXL_STAT_ORACLE=1)"); return; }
    if (!kEngineGlobalStatic) { log_line("[oracle] engine global not yet pinned (no-op)"); return; }
    std::call_once(g_oracle_once, [] {
        std::thread([] {
            log_line("[oracle] poll thread started — waiting for a loaded save (player entity)...");
            for (int i = 0; i < 600; ++i) {            // up to ~20 min
                uintptr_t cont = GetPlayerStatsContainer();
                if (cont) { OracleDumpContainer(cont); break; }
                std::this_thread::sleep_for(std::chrono::seconds(2));
            }
        }).detach();
    });
}

void VerifyFlatArrayAccess(const TweakDB* db) {
    const FlatsArrayView fa = ReadFlatsArrayHeader(db);
    if (!fa.ok) { log_line("[flat-array] header read FAILED"); return; }
    const bool sorted = (fa.flags & 1u) == 0u;
    log_line("[flat-array] +0x40 SortedUniqueArray: entries=0x%llx capacity=%u size=%u flags=0x%x sorted=%s",
             (unsigned long long)fa.entries, fa.capacity, fa.size, fa.flags, sorted ? "yes" : "no");
    if (!fa.entries || fa.size == 0) { log_line("[flat-array] empty/unpopulated"); return; }

    const uintptr_t floatVft = StaticToRuntime(0x106e925f0);
    const uintptr_t int32Vft = StaticToRuntime(0x106e926f8);
    const uintptr_t boolVft  = StaticToRuntime(0x106e92d78);
    log_line("[flat-array] expected vfts (slid): Float=0x%llx Int32=0x%llx Bool=0x%llx",
             (unsigned long long)floatVft, (unsigned long long)int32Vft, (unsigned long long)boolVft);

    const uint32_t n = fa.size < 6 ? fa.size : 6;
    for (uint32_t i = 0; i < n; ++i) {
        TweakDBID e;
        if (!ReadFlatEntry(fa.entries, i, &e)) { log_line("[flat-array] entry[%u] read fail", i); continue; }
        const uint32_t off = TdbOffsetOf(e);
        uint64_t vt = 0; uint32_t data = 0;
        const char* tname = "?";
        if (db->flatDataBuffer) {
            const uintptr_t fv = reinterpret_cast<uintptr_t>(db->flatDataBuffer) + off;
            SafeReadU64(fv, &vt);
            SafeReadU32(fv + 0x08, &data);
            if      (vt == floatVft) tname = "Float";
            else if (vt == int32Vft) tname = "Int32";
            else if (vt == boolVft)  tname = "Bool";
        }
        float asF; std::memcpy(&asF, &data, 4);
        log_line("[flat-array] entry[%u] hash=0x%08x len=%u tdbOff=0x%06x vft=0x%llx type=%s data=0x%08x asF=%g",
                 i, e.nameHash, e.nameLength, off, (unsigned long long)vt, tname, data, asF);
    }

    // Round-trip: resolve entry[0] by its own 40-bit key (machinery self-test).
    TweakDBID e0;
    if (ReadFlatEntry(fa.entries, 0, &e0)) {
        TweakDBID key = e0;
        key.tdbOffset[0] = key.tdbOffset[1] = key.tdbOffset[2] = 0;
        const int64_t off = ResolveFlatOffset(db, key);
        const uint32_t expect = TdbOffsetOf(e0);
        log_line("[flat-array] self-resolve entry0 hash=0x%08x len=%u -> off=0x%06llx expect=0x%06x %s",
                 e0.nameHash, e0.nameLength, (unsigned long long)off, expect,
                 (off == static_cast<int64_t>(expect)) ? "HIT" : "MISS");
    }
}

// ═════════════════════════════════════════════════════════════════════════════
// H-011 step 2a (no-write validation): build a record from the (possibly edited)
// flats via the game's own factory and compare to the live record. Calls into
// game code (baseHash accessor + factory) but writes NO game state — the factory
// allocates a fresh record we read and leak. Proves the UpdateRecord mechanism
// before the risky copy-back. Env-gated: TWEAKXL_TEST_UPDATEREC=1.
// ═════════════════════════════════════════════════════════════════════════════
namespace {

// CreateTDBRecord's 32 per-class factories (static addrs, base 0x100000000),
// indexed by baseHash & 0x1f (F-031 / F-033). Each: void* fn(uint32 baseHash, uint64 recordId).
const uintptr_t kFactoryTable[32] = {
    0x1027052ac, 0x102726504, 0x10274f578, 0x102775fc0, 0x102798288, 0x1027bb6f8,
    0x1027dc118, 0x1027f6068, 0x10280d970, 0x10282a38c, 0x102852278, 0x1028681f8,
    0x10287d218, 0x1028a84b8, 0x1028bbe1c, 0x1028d2c10, 0x1028f89d4, 0x102913214,
    0x1029245e0, 0x102946060, 0x10296d3b8, 0x1029a0100, 0x1029c7e90, 0x1029eb910,
    0x102a01e44, 0x102a1827c, 0x102a681f0, 0x102a800a4, 0x102a9a66c, 0x102ad3a20,
    0x102aec110, 0x102afe918,
};

// Look up a record instance by id in the +0x58 recordsByID map (F-029): the old
// "flats" accessor name; records resolve here via the flats hash mode (F-026).
uintptr_t LookupRecordInstance(const TweakDB* db, TweakDBID id) {
    const uint8_t* hit = Lookup(GetFlatsMapCandidate(db), id, GetFlatsHashMode());
    if (!hit) return 0;
    uintptr_t p10 = 0;
    SafeReadPtr(reinterpret_cast<uintptr_t>(hit) + 0x10, &p10);   // entry+0x10 = instance
    return p10;
}

// Call the GetTweakBaseHash virtual at vtable+0x118 (F-033).
uint32_t CallGetBaseHash(uintptr_t rec) {
    uintptr_t vtbl = 0;
    if (!SafeReadPtr(rec + 0x00, &vtbl) || !vtbl) return 0;
    uintptr_t fn = 0;
    if (!SafeReadPtr(vtbl + 0x118, &fn) || !fn) return 0;
    using BaseHashFn = uint32_t (*)(void*);
    return reinterpret_cast<BaseHashFn>(fn)(reinterpret_cast<void*>(rec));
}

} // namespace

// H-011 / F-034: re-materialize a record from current flats and copy the result
// over the live instance via RTTI Assign — so a flat edit reaches the record's
// reflected (flat-backed) properties and the StatsContainer seed (F-030).
//   realRec = recordsByID[recordId];  baseHash = realRec->vtable+0x118();
//   newRec  = factory[baseHash&0x1f](baseHash, recordId);   // built from live flats
//   realRec->GetNativeType()->Assign(realRec, newRec);      // copy reflected props
// newRec is leaked (one record per call; proper release is a follow-up).
bool UpdateRecord(TweakDB* db, TweakDBID recordId) {
    if (!db) return false;
    const uintptr_t realRec = LookupRecordInstance(db, recordId);
    if (!realRec) return false;
    uint64_t recIdFull = 0;
    SafeReadU64(realRec + 0x40, &recIdFull);                 // canonical recordID
    const uint32_t baseHash = CallGetBaseHash(realRec);      // vtable+0x118 (F-033)
    if (!baseHash) return false;

    using FactoryFn = void* (*)(uint32_t, uint64_t);
    void* newRec = reinterpret_cast<FactoryFn>(StaticToRuntime(kFactoryTable[baseHash & 0x1fu]))(baseHash, recIdFull);
    if (!newRec) return false;

    uint64_t rvt = 0; SafeReadU64(realRec + 0x00, &rvt);
    uintptr_t getTypeFn = 0; SafeReadPtr(static_cast<uintptr_t>(rvt) + 0x08, &getTypeFn);   // GetNativeType (slot 1)
    if (!getTypeFn) return false;
    void* nativeType = reinterpret_cast<void* (*)(void*)>(getTypeFn)(reinterpret_cast<void*>(realRec));
    if (!nativeType) return false;
    uint64_t tvt = 0; SafeReadU64(reinterpret_cast<uintptr_t>(nativeType) + 0x00, &tvt);
    uintptr_t assignFn = 0; SafeReadPtr(static_cast<uintptr_t>(tvt) + 0x50, &assignFn);     // CBaseRTTIType::Assign (F-034)
    if (!assignFn || !PlausiblePtr(assignFn)) return false;

    reinterpret_cast<void (*)(void*, void*, void*)>(assignFn)(nativeType, reinterpret_cast<void*>(realRec), newRec);
    return true;
}

// ── Stat modifier-graph dumper (Phase A2, F-030 seeder content) ──────────────
// The player's max HP / cyberdeck RAM magnitude is NOT a single base flat: the
// seeder (FUN_103a99a60) gathers a stat record's modifier list and folds it
// (base + additives, × multipliers) into the StatsContainer at entity-init.
// Ghidra trace (re-seeder-1..3): a Stat record holds modifier-list FLAT-ID
// fields at object offsets +0x48 / +0x54 / +0x60; each resolves (exactly like
// FUN_100a261ac: flatDataBuffer + TdbOffsetOf) to a DynArray {data@0, count@0xc}
// of modifier TweakDBIDs. Each modifier (ConstantStatModifier) carries a Float
// `.value` flat. This probe walks that graph for Health/Memory and prints every
// modifier's value + the RAW flat-id of its `.value` — so we can target the
// dominant base modifier by VALUE, and (TWEAKXL_DOUBLE_STATS=1) double it via
// EditScalarFlatSafe + UpdateRecord using the raw id (no name needed — inline
// modifier names are synthetic). Env-gated TWEAKXL_DUMP_STAT_MODIFIERS=1. No
// save needed: records exist at TweakDB-init, which is our apply point (F-018).
void DumpStatModifiers(TweakDB* db) {
    if (!db || !std::getenv("TWEAKXL_DUMP_STAT_MODIFIERS")) {
        if (db) log_line("[stat-mod] skipped (set TWEAKXL_DUMP_STAT_MODIFIERS=1)");
        return;
    }
    const uint8_t* buf = db->flatDataBuffer;
    if (!buf) { log_line("[stat-mod] no flat buffer"); return; }
    const bool doDouble = std::getenv("TWEAKXL_DOUBLE_STATS") != nullptr;
    const uintptr_t floatVft = StaticToRuntime(0x106e925f0);
    log_line("[stat-mod] begin (buf=%p floatVft=0x%llx double=%d)",
             (const void*)buf, (unsigned long long)floatVft, doDouble ? 1 : 0);

    auto readId = [&](uintptr_t addr, TweakDBID* out) -> bool {
        uint64_t raw = 0; if (!SafeReadU64(addr, &raw)) return false;
        std::memcpy(out, &raw, 8); return true;
    };
    // Raw hex of a memory region (diagnostic: lets us decode offline if a field
    // heuristic misses, and see lazy-resolution state of the modifier-list fields).
    auto hexDump = [&](const char* tag, uintptr_t base, uint64_t lo, uint64_t hi) {
        for (uint64_t o = lo; o < hi; o += 0x10) {
            uint64_t a = 0, b = 0;
            const bool oka = SafeReadU64(base + o, &a);
            const bool okb = SafeReadU64(base + o + 8, &b);
            log_line("[stat-mod]   %s +0x%02llx: %016llx %016llx", tag,
                     (unsigned long long)o,
                     (unsigned long long)(oka ? a : 0), (unsigned long long)(okb ? b : 0));
        }
    };
    // Resolve a record's flat-id FIELD to its DynArray (data ptr, element count).
    auto resolveArray = [&](const TweakDBID& fid, uintptr_t* dataOut, uint32_t* cntOut) -> bool {
        const uint32_t off = TdbOffsetOf(fid);
        if (off == 0) return false;
        const uintptr_t arr = reinterpret_cast<uintptr_t>(buf) + off;
        uintptr_t dataPtr = 0; uint32_t cnt = 0;
        if (!SafeReadPtr(arr + 0x00, &dataPtr)) return false;
        if (!SafeReadU32(arr + 0x0c, &cnt)) return false;
        *dataOut = dataPtr; *cntOut = cnt; return true;
    };
    // Decode a record FIELD as a Float scalar flat (vtable==Float). Returns the
    // live value and the raw flat-id (so it can be edited by id, no name).
    auto tryFloatField = [&](uintptr_t addr, float* vOut, TweakDBID* idOut) -> bool {
        TweakDBID fid; if (!readId(addr, &fid)) return false;
        const uint32_t off = TdbOffsetOf(fid);
        if (off == 0) return false;
        const uintptr_t fv = reinterpret_cast<uintptr_t>(buf) + off;
        uint64_t vt = 0; if (!SafeReadU64(fv, &vt)) return false;
        if (vt != floatVft) return false;
        uint32_t raw = 0; if (!SafeReadU32(fv + 0x08, &raw)) return false;
        float f; std::memcpy(&f, &raw, 4);
        if (!std::isfinite(f)) return false;
        *vOut = f; *idOut = fid; return true;
    };

    // Inline float scan: read 4-byte floats DIRECTLY from a record's materialized
    // fields. The seeder reads some modifier values inline (modRec+0x6c..) rather
    // than via a flat-id field, so a flat-only scan would miss the magnitude.
    // Tag values that fall in plausible base-HP / base-RAM ranges (greppable).
    auto rangeTag = [](float f) -> const char* {
        const float a = f < 0 ? -f : f;
        if (a >= 80.0f && a <= 700.0f) return " <<HP?";
        if (a >= 3.0f  && a <= 24.0f)  return " <<RAM?";
        return "";
    };
    auto inlineFloatScan = [&](const char* tag, uintptr_t rec, uint64_t lo, uint64_t hi) {
        for (uint64_t o = lo; o < hi; o += 4) {
            uint32_t raw = 0; if (!SafeReadU32(rec + o, &raw)) continue;
            float f; std::memcpy(&f, &raw, 4);
            if (!std::isfinite(f)) continue;
            const float a = f < 0 ? -f : f;
            if (a < 0.01f || a > 1.0e8f) continue;             // skip 0 / pointers / junk
            log_line("[stat-mod]   %s inlineF@+0x%03llx=%g%s", tag, (unsigned long long)o, f, rangeTag(f));
        }
    };
    // Resolve a record FIELD (8 bytes @ rec+off) as a flat-id -> DynArray, REQUIRING
    // the first element to itself be a resolvable record id (so it's a genuine
    // record-reference array, not a coincidental layout).
    auto recordArrayAt = [&](uintptr_t rec, uint64_t off, uintptr_t* dataOut, uint32_t* cntOut) -> bool {
        TweakDBID fid; if (!readId(rec + off, &fid)) return false;
        uintptr_t data = 0; uint32_t cnt = 0;
        if (!resolveArray(fid, &data, &cnt)) return false;
        if (cnt < 1 || cnt > 64 || !PlausiblePtr(data)) return false;
        TweakDBID e0; if (!readId(data, &e0)) return false;
        if (!LookupRecordInstance(db, e0)) return false;       // 1st element must be a record
        *dataOut = data; *cntOut = cnt; return true;
    };
    // Dump a record's value-bearing fields: flat-backed floats (with raw id, so it
    // can be edited by id) AND inline floats.
    auto dumpLeaf = [&](const char* tag, uintptr_t M) {
        for (uint64_t mo = 0x40; mo <= 0x90; mo += 8) {
            float f; TweakDBID vid;
            if (tryFloatField(M + mo, &f, &vid))
                log_line("[stat-mod]   %s flatF@+0x%02llx=%g valueFlat{hash=0x%08x len=%u}%s",
                         tag, (unsigned long long)mo, f, vid.nameHash, vid.nameLength, rangeTag(f));
        }
        inlineFloatScan(tag, M, 0x40, 0x90);
        hexDump(tag, M, 0x40, 0x80);   // raw: statType (TweakDBID) + modifierType (CName)
    };

    const char* names[] = {
        "Character.Player_Puppet_Base",            // player char -> base stat modifier groups
        "Modifiers.Health", "Modifiers.HealthBoost",
        "BaseStats.Health", "BaseStats.Memory",
        "BaseStatPools.Player_Health_Base", "BaseStatPools.Player_Memory_Base",
        "BaseStatPools.Memory",
    };

    for (const char* nm : names) {
        TweakDBID rid = MakeCompactId(nm);
        const uintptr_t rec = LookupRecordInstance(db, rid);
        log_line("[stat-mod] === %s rec=%p (hash=0x%08x len=%u) ===",
                 nm, (void*)rec, rid.nameHash, rid.nameLength);
        if (!rec) continue;
        hexDump("rawrec", rec, 0x00, 0x90);
        inlineFloatScan("top", rec, 0x40, 0x90);

        // Broad scan for record-reference array fields (the modifier-source list is
        // at +0x288 per FUN_102ac0c70/FUN_102ac0a90; scan widely to be robust).
        for (uint64_t off = 0x40; off <= 0x300; off += 8) {
            uintptr_t data = 0; uint32_t cnt = 0;
            if (!recordArrayAt(rec, off, &data, &cnt)) continue;
            log_line("[stat-mod]   recArray@+0x%03llx count=%u", (unsigned long long)off, cnt);
            const uint32_t lim = cnt < 24 ? cnt : 24;
            for (uint32_t i = 0; i < lim; ++i) {
                TweakDBID gid; if (!readId(data + static_cast<uint64_t>(i) * 8, &gid)) break;
                const uintptr_t G = LookupRecordInstance(db, gid);
                log_line("[stat-mod]     grp[%u]{hash=0x%08x len=%u} G=%p",
                         i, gid.nameHash, gid.nameLength, (void*)G);
                if (!G) continue;
                dumpLeaf("    grp", G);
                // one level deeper: this group's own record-array fields = modifiers
                for (uint64_t go = 0x40; go <= 0x90; go += 8) {
                    uintptr_t mdata = 0; uint32_t mcnt = 0;
                    if (!recordArrayAt(G, go, &mdata, &mcnt)) continue;
                    log_line("[stat-mod]       modArray@+0x%02llx count=%u", (unsigned long long)go, mcnt);
                    const uint32_t mlim = mcnt < 24 ? mcnt : 24;
                    for (uint32_t j = 0; j < mlim; ++j) {
                        TweakDBID mid; if (!readId(mdata + static_cast<uint64_t>(j) * 8, &mid)) break;
                        const uintptr_t M = LookupRecordInstance(db, mid);
                        log_line("[stat-mod]         mod[%u]{hash=0x%08x len=%u} M=%p",
                                 j, mid.nameHash, mid.nameLength, (void*)M);
                        if (M) dumpLeaf("        mod", M);
                    }
                }
            }
        }

        // Chase single-TweakDBID reference fields (pool.stat, pool.+0x60) one level.
        for (uint64_t off = 0x48; off <= 0x70; off += 8) {
            TweakDBID fid; if (!readId(rec + off, &fid)) continue;
            const uint32_t o = TdbOffsetOf(fid);
            if (o == 0) continue;
            uint64_t val = 0;
            if (!SafeReadU64(reinterpret_cast<uintptr_t>(buf) + o, &val)) continue;
            TweakDBID ref; std::memcpy(&ref, &val, 8);
            const uintptr_t R = LookupRecordInstance(db, ref);
            if (!R) continue;
            log_line("[stat-mod]   ref@+0x%02llx -> {hash=0x%08x len=%u} R=%p",
                     (unsigned long long)off, ref.nameHash, ref.nameLength, (void*)R);
            dumpLeaf("   ref", R);
        }
    }
    log_line("[stat-mod] done (double=%d, dump-only this build)", doDouble ? 1 : 0);
}

// Find which candidate flats actually back an RTTI-reflected record field (i.e.
// editing them + rebuilding changes the record → UpdateRecord will propagate to
// gameplay). Edits each candidate to a sentinel, rebuilds via the factory, diffs
// the rebuilt record vs an unedited rebuild (ignoring the +0x28 alloc counter),
// restores. Env-gated TWEAKXL_PROBE_REFLECTED=1. No save / no persistent writes.
void ProbeReflectedFlats(TweakDB* db) {
    if (!db || !std::getenv("TWEAKXL_PROBE_REFLECTED")) {
        if (db) log_line("[reflected] skipped (set TWEAKXL_PROBE_REFLECTED=1)");
        return;
    }
    bool fromEnv = false;
    const std::string path = ResolveCandidatesPath(&fromEnv);
    std::ifstream in(path);
    if (!in) { log_line("[reflected] cannot open candidates %s", path.c_str()); return; }
    log_line("[reflected] scanning candidates from %s", path.c_str());

    using FactoryFn = void* (*)(uint32_t, uint64_t);
    uint32_t reflected = 0, tested = 0;
    std::string line;
    while (std::getline(in, line)) {
        const std::string name = StripLine(line);
        if (name.empty()) continue;
        const auto dot = name.rfind('.');
        if (dot == std::string::npos || dot == 0) continue;
        const std::string recName = name.substr(0, dot);

        // flat must resolve, record must exist
        const TweakDBID fid = MakeCompactId(name);
        if (ResolveFlatOffset(db, fid) < 0) continue;
        auto snap = ReadScalarFlat(db, fid);
        if (!snap.has_value()) continue;   // non-scalar — skip
        const uintptr_t realRec = LookupRecordInstance(db, MakeCompactId(recName));
        if (!realRec) continue;
        uint64_t recIdFull = 0; SafeReadU64(realRec + 0x40, &recIdFull);
        const uint32_t baseHash = CallGetBaseHash(realRec);
        if (!baseHash) continue;
        FactoryFn factory = reinterpret_cast<FactoryFn>(StaticToRuntime(kFactoryTable[baseHash & 0x1fu]));
        ++tested;

        uintptr_t before = reinterpret_cast<uintptr_t>(factory(baseHash, recIdFull));
        if (!before) continue;
        FlatValue sent; sent.type = FlatType::Float; sent.value = 9999.0f;
        EditScalarFlatInPlace(db, fid, sent);
        uintptr_t after = reinterpret_cast<uintptr_t>(factory(baseHash, recIdFull));
        EditScalarFlatInPlace(db, fid, *snap);   // restore immediately

        if (!after) continue;
        // Diff [0x18,0x150), ignore +0x28 (alloc counter).
        int firstOff = -1; uint32_t va = 0, vb = 0;
        for (uint32_t off = 0x18; off < 0x150; off += 4) {
            if (off == 0x28) continue;
            uint32_t x = 0, y = 0;
            SafeReadU32(before + off, &x); SafeReadU32(after + off, &y);
            if (x != y) { firstOff = (int)off; va = x; vb = y; break; }
        }
        if (firstOff >= 0) {
            float fa, fb; std::memcpy(&fa,&va,4); std::memcpy(&fb,&vb,4);
            log_line("[reflected] YES %s -> field @+0x%02x %g => %g (edit propagates)",
                     name.c_str(), firstOff, fa, fb);
            ++reflected;
        }
    }
    log_line("[reflected] done: %u/%u tested candidates back a reflected record field",
             reflected, tested);
}

void TestUpdateRecordBuild(TweakDB* db) {
    if (!db) return;
    if (!std::getenv("TWEAKXL_TEST_UPDATEREC")) {
        log_line("[updaterec] skipped (set TWEAKXL_TEST_UPDATEREC=1)");
        return;
    }
    // Target a known Stat record (F-027/F-033: gamedataStat_Record, value@+0x54).
    const char* recName = "BaseStats.AccumulatedDoTDecayRate";
    if (const char* e = std::getenv("TWEAKXL_TEST_RECORD"); e && *e) recName = e;

    TweakDBID rid = MakeCompactId(recName);
    const uintptr_t realRec = LookupRecordInstance(db, rid);
    if (!realRec) { log_line("[updaterec] record '%s' not found in +0x58", recName); return; }

    // Canonical recordID stored on the record (+0x40) — pass this to the factory.
    uint64_t recIdFull = 0;
    SafeReadU64(realRec + 0x40, &recIdFull);

    const uint32_t baseHash = CallGetBaseHash(realRec);
    const uint32_t cat = baseHash & 0x1fu;
    const uintptr_t facStatic = kFactoryTable[cat];
    const uintptr_t facRuntime = StaticToRuntime(facStatic);
    uint64_t realVtbl = 0; SafeReadU64(realRec + 0x00, &realVtbl);
    log_line("[updaterec] rec='%s' realRec=%p vtbl=0x%llx recID@+0x40=0x%016llx baseHash=0x%08x cat=%u factory=0x%llx",
             recName, (void*)realRec, (unsigned long long)realVtbl,
             (unsigned long long)recIdFull, baseHash, cat, (unsigned long long)facStatic);

    using FactoryFn = void* (*)(uint32_t, uint64_t);
    FactoryFn factory = reinterpret_cast<FactoryFn>(facRuntime);

    // ── Build #1 (no edit): should faithfully reproduce the live record ──────
    log_line("[updaterec] calling factory (build #1, no edit)...");
    uintptr_t newRec1 = reinterpret_cast<uintptr_t>(factory(baseHash, recIdFull));
    if (!newRec1) { log_line("[updaterec] factory returned null"); return; }
    uint64_t n1vtbl = 0; SafeReadU64(newRec1 + 0x00, &n1vtbl);
    log_line("[updaterec] build #1 ok: newRec1=%p vtbl=0x%llx", (void*)newRec1, (unsigned long long)n1vtbl);

    // Compare value-region (skip the 0x18-byte header: vtable/self/refcount).
    const uint32_t kCmpStart = 0x18, kCmpEnd = 0x60;
    auto cmpRegion = [&](uintptr_t a, uintptr_t b, const char* tag) {
        uint32_t diffs = 0;
        for (uint32_t off = kCmpStart; off < kCmpEnd; off += 4) {
            uint32_t va = 0, vb = 0;
            SafeReadU32(a + off, &va); SafeReadU32(b + off, &vb);
            if (va != vb) {
                float fa, fb; std::memcpy(&fa,&va,4); std::memcpy(&fb,&vb,4);
                log_line("[updaterec]   %s diff @+0x%02x: 0x%08x(%g) vs 0x%08x(%g)", tag, off, va, fa, vb, fb);
                ++diffs;
            }
        }
        log_line("[updaterec]   %s: %u differing words in [0x%02x,0x%02x)", tag, diffs, kCmpStart, kCmpEnd);
    };
    cmpRegion(newRec1, realRec, "build1-vs-real");

    // ── Probe propagation: edit a backing flat, rebuild, see what changes ────
    const char* props[] = { ".value", ".max", ".min", ".statType" };
    for (const char* prop : props) {
        std::string fname = std::string(recName) + prop;
        TweakDBID fid = MakeCompactId(fname);
        auto snap = ReadScalarFlat(db, fid);
        if (!snap.has_value()) { log_line("[updaterec] flat %s absent/non-scalar — skip", fname.c_str()); continue; }
        FlatValue bump; bump.type = FlatType::Float; bump.value = 4242.0f;
        EditScalarFlatInPlace(db, fid, bump);
        uintptr_t newRec2 = reinterpret_cast<uintptr_t>(factory(baseHash, recIdFull));
        log_line("[updaterec] edited %s=4242 -> rebuild newRec2=%p; diff vs build#1:", fname.c_str(), (void*)newRec2);
        if (newRec2) cmpRegion(newRec2, newRec1, fname.c_str());
        EditScalarFlatInPlace(db, fid, *snap);  // restore
    }

    // ── Step 2b: prove the RTTI Assign copy-back (TWEAKXL_TEST_ASSIGN=1) ──────
    // GetNativeType = realRec->vtable[1] (+0x08); CBaseRTTIType::Assign = type
    // vtable +0x50 (RED4ext.SDK). Poke a sentinel into our TEMP record's +0x54,
    // Assign temp->realRec, and verify realRec+0x54 changed; then restore.
    if (std::getenv("TWEAKXL_TEST_ASSIGN")) {
        // GetNativeType(realRec)
        uint64_t rvt = 0; SafeReadU64(realRec + 0x00, &rvt);
        uintptr_t getTypeFn = 0; SafeReadPtr(static_cast<uintptr_t>(rvt) + 0x08, &getTypeFn);
        log_line("[updaterec-assign] realRec.vtbl=0x%llx GetNativeType@+0x08=0x%llx (static~0x%llx)",
                 (unsigned long long)rvt, (unsigned long long)getTypeFn,
                 (unsigned long long)(getTypeFn - GetSlide()));
        if (!getTypeFn) { log_line("[updaterec-assign] no GetNativeType ptr"); }
        else {
            using GetTypeFn = void* (*)(void*);
            void* nativeType = reinterpret_cast<GetTypeFn>(getTypeFn)(reinterpret_cast<void*>(realRec));
            uint64_t tvt = 0; SafeReadU64(reinterpret_cast<uintptr_t>(nativeType) + 0x00, &tvt);
            uintptr_t assignFn = 0; SafeReadPtr(static_cast<uintptr_t>(tvt) + 0x50, &assignFn);
            log_line("[updaterec-assign] nativeType=%p typeVtbl=0x%llx Assign@+0x50=0x%llx (static~0x%llx)",
                     nativeType, (unsigned long long)tvt, (unsigned long long)assignFn,
                     (unsigned long long)(assignFn - GetSlide()));

            // Build a fresh temp record, poke a sentinel into its +0x54.
            uintptr_t tmp = reinterpret_cast<uintptr_t>(factory(baseHash, recIdFull));
            if (tmp && assignFn && PlausiblePtr(assignFn)) {
                const float kSentinel = 7777.0f;
                uint32_t sraw; std::memcpy(&sraw, &kSentinel, 4);
                SafeWrite(tmp + 0x54, &sraw, 4);
                uint32_t before = 0; SafeReadU32(realRec + 0x54, &before);
                float bf; std::memcpy(&bf,&before,4);
                log_line("[updaterec-assign] calling Assign(nativeType, realRec, tmp) — realRec+0x54 before=%g ...", bf);

                using AssignFn = void (*)(void*, void*, void*);
                reinterpret_cast<AssignFn>(assignFn)(nativeType, reinterpret_cast<void*>(realRec), reinterpret_cast<void*>(tmp));

                uint32_t after = 0; SafeReadU32(realRec + 0x54, &after);
                float af; std::memcpy(&af,&after,4);
                log_line("[updaterec-assign] Assign returned (no crash). realRec+0x54 after=%g %s",
                         af, (af == kSentinel) ? "== SENTINEL (Assign copies fields!)" : "(unchanged?)");
                // restore realRec+0x54 to its original value
                SafeWrite(realRec + 0x54, &before, 4);
                log_line("[updaterec-assign] restored realRec+0x54");
            } else {
                log_line("[updaterec-assign] skipped call (tmp=%p assignFn=0x%llx plausible=%d)",
                         (void*)tmp, (unsigned long long)assignFn, assignFn ? PlausiblePtr(assignFn) : 0);
            }
        }
    }

    log_line("[updaterec] done (temp records leaked, no game state written)");
}

// F-031: prove the correct flat path end-to-end at the title screen (no save):
//  (1) resolve REAL NAMED flats via +0x40 — the same map-less retest of names
//      that got 0/N against the wrong +0x58 map; includes high-confidence
//      BaseStats.*.value/.enumName flats (psiberx StatService reads these);
//  (2) round-trip EditScalarFlatInPlace on a real Float flat (edit → verify →
//      restore). All memory-level; no gameplay observation required.
void TestFlatWritePath(TweakDB* db) {
    if (!db) return;
    const uintptr_t floatVft = StaticToRuntime(0x106e925f0);
    const uintptr_t int32Vft = StaticToRuntime(0x106e926f8);
    const uintptr_t boolVft  = StaticToRuntime(0x106e92d78);

    auto probeName = [&](const std::string& name) -> bool {
        const TweakDBID id = MakeCompactId(name);
        const int64_t off = ResolveFlatOffset(db, id);
        if (off < 0) { log_line("[flat-name] MISS %s (hash=0x%08x len=%zu)", name.c_str(), id.nameHash, name.size()); return false; }
        uint64_t vt = 0; uint32_t data = 0; const char* tn = "?";
        if (db->flatDataBuffer) {
            const uintptr_t fv = reinterpret_cast<uintptr_t>(db->flatDataBuffer) + static_cast<uint64_t>(off);
            SafeReadU64(fv, &vt); SafeReadU32(fv + 0x08, &data);
            if (vt==floatVft) tn="Float"; else if (vt==int32Vft) tn="Int32"; else if (vt==boolVft) tn="Bool";
        }
        float asF; std::memcpy(&asF, &data, 4);
        log_line("[flat-name] HIT  %s -> off=0x%06llx type=%s data=0x%08x asF=%g asI=%d",
                 name.c_str(), (unsigned long long)off, tn, data, asF, (int)data);
        return true;
    };

    // (1a) High-confidence named flats: BaseStats records' .value/.enumName/.min/.max.
    log_line("[flat-name] === high-confidence BaseStats flats (psiberx StatService) ===");
    const char* kStatRecords[] = {
        "BaseStats.AccumulatedDoTDecayRate",  // F-027: record value 0.0961 @ +0x54
        "BaseStats.Health", "BaseStats.CarryCapacity", "BaseStats.ADSSpeedPercentBonus",
    };
    const char* kProps[] = { ".value", ".enumName", ".min", ".max", ".flags" };
    uint32_t hcHits = 0, hcTotal = 0;
    for (const char* rec : kStatRecords)
        for (const char* p : kProps) { ++hcTotal; if (probeName(std::string(rec) + p)) ++hcHits; }
    log_line("[flat-name] high-confidence: %u/%u HIT", hcHits, hcTotal);

    // (1b) Re-sweep the candidate file against +0x40 (got 0/N against +0x58).
    bool fromEnv = false;
    const std::string path = ResolveCandidatesPath(&fromEnv);
    std::ifstream in(path);
    if (in) {
        uint32_t hits = 0, total = 0; std::string line;
        while (std::getline(in, line)) {
            const std::string name = StripLine(line);
            if (name.empty()) continue;
            ++total;
            if (ResolveFlatOffset(db, MakeCompactId(name)) >= 0) ++hits;
        }
        log_line("[flat-name] candidate-file +0x40 sweep: %u/%u HIT (file=%s)", hits, total, path.c_str());
    }

    // (2) Round-trip edit on the first real Float flat we can find.
    const FlatsArrayView fa = ReadFlatsArrayHeader(db);
    if (!fa.ok || !fa.entries || fa.size == 0 || !db->flatDataBuffer) {
        log_line("[flat-edit-test] skipped (no flats/buffer)"); return;
    }
    const uint32_t scanCap = fa.size < 300000u ? fa.size : 300000u;
    int64_t editOff = -1; TweakDBID editId{};
    for (uint32_t i = 0; i < scanCap; ++i) {
        TweakDBID e;
        if (!ReadFlatEntry(fa.entries, i, &e)) break;
        const uint32_t off = TdbOffsetOf(e);
        uint64_t vt = 0;
        SafeReadU64(reinterpret_cast<uintptr_t>(db->flatDataBuffer) + off, &vt);
        if (vt == floatVft) { editOff = off; editId = e; break; }
    }
    if (editOff < 0) { log_line("[flat-edit-test] no Float flat in first %u entries", scanCap); return; }

    const uintptr_t dataAddr = reinterpret_cast<uintptr_t>(db->flatDataBuffer) + static_cast<uint64_t>(editOff) + 0x08;
    uint32_t origRaw = 0; SafeReadU32(dataAddr, &origRaw); float origF; std::memcpy(&origF, &origRaw, 4);

    FlatValue sentinel; sentinel.type = FlatType::Float; sentinel.value = 1337.0f;
    const bool wrote = EditScalarFlatInPlace(db, editId, sentinel);
    uint32_t aRaw = 0; SafeReadU32(dataAddr, &aRaw); float aF; std::memcpy(&aF, &aRaw, 4);

    FlatValue restore; restore.type = FlatType::Float; restore.value = origF;
    EditScalarFlatInPlace(db, editId, restore);
    uint32_t rRaw = 0; SafeReadU32(dataAddr, &rRaw); float rF; std::memcpy(&rF, &rRaw, 4);

    const bool pass = wrote && aF == 1337.0f && rF == origF;
    log_line("[flat-edit-test] Float flat hash=0x%08x len=%u off=0x%06llx orig=%g wrote=%d after=%g restored=%g %s",
             editId.nameHash, editId.nameLength, (unsigned long long)editOff, origF, wrote ? 1 : 0, aF, rF,
             pass ? "PASS" : "FAIL");
}

} // namespace red4ext_mac
