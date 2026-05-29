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
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <mutex>
#include <string>

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

} // namespace red4ext_mac
