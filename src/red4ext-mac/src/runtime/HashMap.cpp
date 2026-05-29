// HashMap.cpp — P1.5 hash-map walker implementation.
//
// Walks the bucket-index array + entry chain of any TweakDB hash map (layout
// from TweakDB.hpp / F-012). Every dereference of a game-managed pointer goes
// through mach_vm_read_overwrite, so corrupt/unmapped pointers yield nullptr
// instead of crashing (SOUL.md: "never crash on bad pointers").
//
// Entry shape (F-012 @ 0x10121f328):
//   +0x00 next-index (u32, chain link; 0xFFFFFFFF terminates)
//   +0x04 stored hash (u32)
//   +0x08 key / TweakDBID (8 bytes)
//   +0x10 payload...
//
// No mutation (P1.6/P1.10 territory).

#include "HashMap.hpp"
#include "TweakDB.hpp"   // HashMap struct + GetRecordsMap()

#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <mutex>

#include <mach/mach.h>
#include <mach/mach_vm.h>

namespace red4ext_mac {
namespace {

constexpr const char* kLogPath = "/tmp/red4ext-mac.log";

constexpr uint32_t kEmptyBucket = 0xFFFFFFFFu; // F-015 sentinel
constexpr size_t   kEntryHashOff = 0x04;
constexpr size_t   kEntryKeyOff  = 0x08;
constexpr size_t   kEntryNextOff = 0x00;

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

// Safe read of `n` bytes from an arbitrary process address. false on fault/short.
bool SafeRead(uintptr_t addr, void* out, size_t n) {
    if (addr == 0) return false;
    mach_vm_size_t got = 0;
    kern_return_t kr = mach_vm_read_overwrite(
        mach_task_self(),
        (mach_vm_address_t)addr,
        (mach_vm_size_t)n,
        (mach_vm_address_t)out,
        &got);
    return kr == KERN_SUCCESS && got == n;
}
bool SafeU32(uintptr_t a, uint32_t* v) { return SafeRead(a, v, sizeof(*v)); }
bool SafeU64(uintptr_t a, uint64_t* v) { return SafeRead(a, v, sizeof(*v)); }
bool SafePtr(uintptr_t a, uintptr_t* v) { return SafeRead(a, v, sizeof(*v)); }

// Per-map runtime-confirmed hash modes (P1.6b). Records uses FNV-1a (F-012/P1.5);
// flats uses nameHash-direct (P1.6). Default to the structural prior (Fnv1a8B)
// until each verifier overwrites it with the in-game truth.
std::atomic<HashMode> g_recordsMode{HashMode::Fnv1a8B};
std::atomic<HashMode> g_flatsMode{HashMode::Fnv1a8B};

// Read a hash map's four load-bearing fields safely. Returns false if any read
// faults or the map is structurally empty (zero buckets/stride/null pointers).
struct MapView {
    uint32_t  bucketCount;
    uint32_t  stride;
    uint32_t  count;
    uintptr_t bucketArray;
    uintptr_t entries;
};
bool ReadMapView(const HashMap* map, MapView* mv) {
    if (!map) return false;
    const uintptr_t mb = reinterpret_cast<uintptr_t>(map);
    if (!SafeU32(mb + offsetof(HashMap, bucketCount), &mv->bucketCount)) return false;
    if (!SafeU32(mb + offsetof(HashMap, stride),      &mv->stride))      return false;
    if (!SafeU32(mb + offsetof(HashMap, count),       &mv->count))       return false;
    if (!SafePtr(mb + offsetof(HashMap, bucketArray), &mv->bucketArray)) return false;
    if (!SafePtr(mb + offsetof(HashMap, entries),     &mv->entries))     return false;
    if (mv->bucketCount == 0 || mv->stride == 0 ||
        mv->bucketArray == 0 || mv->entries == 0) return false;
    return true;
}

std::once_flag g_hashfn_once;

void DoVerifyHashFunction(const TweakDB* db) {
    if (db == nullptr) {
        log_line("[hashmap] hash-function: skipped (db=null)");
        return;
    }
    const HashMap* rec = GetRecordsMap(db);   // +0x88, records (843 entries live)
    MapView mv;
    if (!ReadMapView(rec, &mv)) {
        log_line("[hashmap] hash-function: skipped (records map unreadable/empty)");
        return;
    }

    // First non-empty bucket → head entry of its chain.
    uint32_t headIdx = kEmptyBucket;
    uint32_t bucket  = 0;
    for (uint32_t i = 0; i < mv.bucketCount; ++i) {
        uint32_t v = kEmptyBucket;
        if (SafeU32(mv.bucketArray + (uint64_t)i * 4, &v) && v != kEmptyBucket) {
            headIdx = v;
            bucket  = i;
            break;
        }
    }
    if (headIdx == kEmptyBucket) {
        log_line("[hashmap] hash-function: skipped (no non-empty bucket in records map)");
        return;
    }

    const uintptr_t entry = mv.entries + (uint64_t)headIdx * mv.stride;
    uint32_t storedHash = 0;
    uint8_t  keyBytes[8] = {0};
    if (!SafeU32(entry + kEntryHashOff, &storedHash) ||
        !SafeRead(entry + kEntryKeyOff, keyBytes, sizeof(keyBytes))) {
        log_line("[hashmap] hash-function: skipped (entry read faulted)");
        return;
    }

    TweakDBID key;
    std::memcpy(&key, keyBytes, sizeof(key));

    // Candidate hash functions over the live key bytes.
    const uint32_t cFnv8 = Fnv1a32(keyBytes, 8);
    const uint32_t cFnv5 = Fnv1a32(keyBytes, 5);   // nameHash(4) + nameLength(1)
    const uint32_t cCrc8 = Crc32(keyBytes, 8);
    const uint32_t cName = key.nameHash;

    HashMode    mode     = HashMode::Unknown;
    const char* modeName = "unknown";
    uint32_t    computed = 0;
    if (storedHash == cFnv8)      { mode = HashMode::Fnv1a8B;        modeName = "fnv1a-8B";       computed = cFnv8; }
    else if (storedHash == cFnv5) { mode = HashMode::Fnv1a5B;        modeName = "fnv1a-5B";       computed = cFnv5; }
    else if (storedHash == cCrc8) { mode = HashMode::Crc32_8B;       modeName = "crc32-8B";       computed = cCrc8; }
    else if (storedHash == cName) { mode = HashMode::NameHashDirect; modeName = "nameHash-direct";computed = cName; }

    // Per spec: if nothing matched, fall back to nameHash-direct (safest) and
    // dump every candidate so the researcher can investigate.
    const HashMode effective = (mode == HashMode::Unknown) ? HashMode::NameHashDirect : mode;
    SetRecordsHashMode(effective);

    const uint32_t off = (uint32_t)key.tdbOffset[0]
                       | ((uint32_t)key.tdbOffset[1] << 8)
                       | ((uint32_t)key.tdbOffset[2] << 16);

    log_line("[hashmap] hash-function: %s stored=0x%08x computed=0x%08x "
             "key=<TweakDBID(0x%08x,len=%u,off=0x%06x)> bucket=%u/%u",
             modeName, storedHash, computed,
             key.nameHash, key.nameLength, off, bucket, mv.bucketCount);

    if (mode == HashMode::Unknown) {
        log_line("[hashmap] candidates: fnv1a-8B=0x%08x fnv1a-5B=0x%08x "
                 "crc32-8B=0x%08x nameHash=0x%08x (NONE matched stored=0x%08x — "
                 "HashKey defaulted to nameHash-direct; lookups likely non-functional)",
                 cFnv8, cFnv5, cCrc8, cName, storedHash);
    }

    // Round-trip: look the same key back up (with the records mode just found);
    // it must resolve to the same entry.
    const uint8_t* hit = Lookup(rec, key, effective);
    const bool pass = (hit == reinterpret_cast<const uint8_t*>(entry));
    log_line("[hashmap] lookup-test: %s key=<TweakDBID(0x%08x,len=%u)> entry=%p expected=%p",
             pass ? "pass" : "fail", key.nameHash, key.nameLength,
             (void*)hit, (void*)entry);
}

} // namespace

// ── Public API ───────────────────────────────────────────────────────────────

uint32_t Fnv1a32(const void* data, size_t len) {
    const uint8_t* p = static_cast<const uint8_t*>(data);
    uint32_t h = 0x811c9dc5u;            // FNV offset basis (F-012)
    for (size_t i = 0; i < len; ++i) {
        h ^= p[i];
        h *= 0x01000193u;                // FNV prime (F-012)
    }
    return h;
}

uint32_t Crc32(const void* data, size_t len) {
    const uint8_t* p = static_cast<const uint8_t*>(data);
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; ++i) {
        crc ^= p[i];
        for (int k = 0; k < 8; ++k) {
            const uint32_t mask = -(crc & 1u);          // 0 or 0xFFFFFFFF
            crc = (crc >> 1) ^ (0xEDB88320u & mask);    // reflected ISO-3309 poly
        }
    }
    return ~crc;
}

HashMode GetRecordsHashMode() { return g_recordsMode.load(std::memory_order_acquire); }
HashMode GetFlatsHashMode()   { return g_flatsMode.load(std::memory_order_acquire); }
void SetRecordsHashMode(HashMode mode) { g_recordsMode.store(mode, std::memory_order_release); }
void SetFlatsHashMode(HashMode mode)   { g_flatsMode.store(mode, std::memory_order_release); }

uint32_t HashKey(TweakDBID key, HashMode mode) {
    uint8_t b[8];
    std::memcpy(b, &key, sizeof(b));
    switch (mode) {
        case HashMode::Fnv1a8B:        return Fnv1a32(b, 8);
        case HashMode::Fnv1a5B:        return Fnv1a32(b, 5);
        case HashMode::Crc32_8B:       return Crc32(b, 8);
        case HashMode::NameHashDirect: return key.nameHash;
        case HashMode::Unknown:
        default:                       return key.nameHash; // safest fallback
    }
}

const uint8_t* Lookup(const HashMap* map, TweakDBID key, HashMode mode) {
    MapView mv;
    if (!ReadMapView(map, &mv)) return nullptr;

    const uint32_t h = HashKey(key, mode);
    uint64_t key64 = 0;
    std::memcpy(&key64, &key, sizeof(key64));

    const uint32_t bucket = h % mv.bucketCount;
    uint32_t idx = kEmptyBucket;
    if (!SafeU32(mv.bucketArray + (uint64_t)bucket * 4, &idx)) return nullptr;

    // Bound the walk so a corrupt/cyclic chain can never hang us.
    uint64_t guard = (uint64_t)mv.count + 8;
    while (idx != kEmptyBucket && guard-- != 0) {
        const uintptr_t entry = mv.entries + (uint64_t)idx * mv.stride;

        uint32_t storedHash = 0;
        if (!SafeU32(entry + kEntryHashOff, &storedHash)) return nullptr;

        if (storedHash == h) {
            uint64_t storedKey = 0;
            if (SafeU64(entry + kEntryKeyOff, &storedKey) && storedKey == key64)
                return reinterpret_cast<const uint8_t*>(entry);
        }

        uint32_t next = kEmptyBucket;
        if (!SafeU32(entry + kEntryNextOff, &next)) return nullptr;
        idx = next;
    }
    return nullptr;
}

void VerifyHashFunction(const TweakDB* db) {
    std::call_once(g_hashfn_once, [db] { DoVerifyHashFunction(db); });
}

} // namespace red4ext_mac
