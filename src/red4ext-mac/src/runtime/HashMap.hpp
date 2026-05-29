#pragma once
//
// HashMap.hpp — P1.5 hash-map walker.
//
// Lookup() resolves a TweakDBID to its entry inside any of the three
// structurally-identical TweakDB hash maps (flats +0x58, records +0x88,
// queries +0x108 — H-008 resolved-fact: flats=+0x58). The HashMap struct and
// the per-map field offsets live in TweakDB.hpp (P1.4); this file is the
// algorithm that walks them.
//
// Bucket hashing is 32-bit FNV-1a (F-012: basis 0x811c9dc5, prime 0x01000193,
// found in GetRecords' body). F-012 did not pin down WHICH bytes of the key are
// hashed; VerifyHashFunction() settles that empirically at runtime against a
// real stored entry, then HashKey() uses the confirmed mode.
//
// READ-ONLY. Every game-managed pointer is dereferenced via
// mach_vm_read_overwrite (SingletonAccess model) — a bad pointer yields nullptr
// / 0, never a crash. No mutation (that's P1.6/P1.10).
//
#include <cstddef>
#include <cstdint>

namespace red4ext_mac {

struct HashMap;   // defined in TweakDB.hpp (+0x00 bucketArray .. +0x28 allocator)
struct TweakDB;   // defined in TweakDB.hpp

// The packed 8-byte TweakDBID (binary-format spec §6).
struct TweakDBID {
    uint32_t nameHash;       // CRC32(full_name) — zlib/ISO-3309 poly (spec §6)
    uint8_t  nameLength;     // strlen(name)
    uint8_t  tdbOffset[3];   // 24-bit LE offset (Windows: into flatDataBuffer; macOS meaning per P1.6)
};
static_assert(sizeof(TweakDBID) == 8, "TweakDBID must be 8 bytes");

// Which hash the live maps actually use for bucketing / the +0x04 stored hash.
// Decided at runtime by VerifyHashFunction(); defaults to the F-012-implied
// 32-bit FNV-1a over the full 8 key bytes until the probe confirms otherwise.
enum class HashMode : uint8_t {
    Unknown,        // probe ran but matched nothing — lookups non-functional
    Fnv1a8B,        // FNV-1a 32-bit over all 8 TweakDBID bytes  (default prior)
    Fnv1a5B,        // FNV-1a 32-bit over nameHash(4) + nameLength(1)
    Crc32_8B,       // zlib CRC32 over all 8 bytes
    NameHashDirect, // stored hash IS key.nameHash (no rehash) — also safe fallback
};

// ── Hash primitives (exposed so the unit test can verify them) ───────────────
// Standard 32-bit FNV-1a: basis 0x811c9dc5, prime 0x01000193 (F-012).
uint32_t Fnv1a32(const void* data, size_t len);
// zlib / ISO-3309 CRC32 (reflected poly 0xEDB88320), seed 0 (spec §6).
uint32_t Crc32(const void* data, size_t len);

// Compute the bucket/stored hash for a key using the runtime-confirmed mode.
uint32_t HashKey(TweakDBID key);

// Walk `map` for `key`. Returns a pointer to the matching entry's BYTE-0 start
// (next-index @+0x00, stored hash @+0x04, key @+0x08, payload after), or
// nullptr if not found / map invalid. Never crashes on bad game pointers.
const uint8_t* Lookup(const HashMap* map, TweakDBID key);

// One-shot (std::call_once) diagnostic: against the first non-empty records-map
// entry, try every candidate hash function, identify which reproduces the
// stored +0x04 hash, set the global HashMode, then round-trip-lookup that same
// key. Emits to /tmp/red4ext-mac.log:
//   [hashmap] hash-function: <mode> stored=0x.. computed=0x.. key=<TweakDBID(..)>
//   [hashmap] lookup-test: <pass|fail> ...
// Internal use (called from Loader after VerifyH008).
void VerifyHashFunction(const TweakDB* db);

} // namespace red4ext_mac
