#pragma once
//
// TweakDB.hpp — P1.4 typed TweakDB struct + read-only accessor.
//
// Models the macOS-runtime `game::data::TweakDB` instance whose live pointer is
// read from the global singleton (F-011, via SingletonAccess.hpp). The layout
// here is transcribed *exactly* from F-015 (constructor FUN_102b73db8), with
// every offset accounted for and each field annotated with the F-### row it
// came from. Where F-015 describes an embedded sub-object/container we don't
// need typed access to in v1.0, an opaque byte array preserves the offset
// without guessing internal structure (per the P1.4 "prefer opaque over guess"
// constraint).
//
// READ-ONLY at this layer. No mutation helpers — value-buffer writes land in
// P1.6, real mutation in P1.10. (AGENTS.md: "No partial-state mutations.")
//
// All offsets are in-struct byte offsets. Static (Ghidra) addresses cited in
// comments are base 0x100000000; runtime = static + slide (F-004/F-007).
//
#include <cstddef>
#include <cstdint>
#include <optional>

#include "Values.hpp"     // FlatType, FlatValue
#include "HashMap.hpp"     // TweakDBID, Lookup

namespace red4ext_mac {

// ───────────────────────────────────────────────────────────────────────────
// HashMap — one of the three structurally-identical TweakDBID-keyed hash maps
// (F-017: "three structurally-identical hash maps"). Sub-layout confirmed two
// ways: F-012 disassembled the records map (+0x88) and F-019 disassembled the
// +0x58 map's destructor (FUN_100ad1df0). Both agree:
//
//   block+0x00  bucketArray  uint32* — bucket-index array, empty bucket = 0xffffffff
//   block+0x08  count        uint32  — populated entry count   (F-012 +0x90; F-019 +0x08)
//   block+0x0c  bucketCount  uint32  — modulus for the hash    (F-012 +0x94; F-019 +0x0c)
//   block+0x10  entries      uint8*  — entries array (typed by 'stride')
//   block+0x18  field18      uint32  — unannotated in F-015 (capacity/used?); kept explicit
//   block+0x1c  stride       uint32  — bytes per entry         (F-012 +0xa4; F-019 +0x1c)
//   block+0x20  sentinel     uint32  — empty-bucket template = 0xffffffff (F-015 +0xa8)
//   block+0x24  field24      uint32  — alignment/unannotated padding
//   block+0x28  allocator    void*   — backing allocator       (F-015 +0xb0)
//
// Map stride in the parent struct is 0x30 (0x88 records − 0x58 mapA), so the
// whole HashMap is exactly 0x30 bytes.
//
// Entry shape (F-012 @ 0x10121f328), for downstream P1.5: +0x00 next-index,
// +0x04 stored hash (uint32), +0x08 key/TweakDBID (8 bytes), payload follows.
struct HashMap {
    uint32_t* bucketArray;   // +0x00 — bucket-index array (F-012/F-019)
    uint32_t  count;         // +0x08 — populated entry count (F-012/F-019)
    uint32_t  bucketCount;   // +0x0c — modulus (F-012/F-019)
    uint8_t*  entries;       // +0x10 — entries array (F-012/F-019)
    uint32_t  field18;       // +0x18 — unannotated in F-015; held explicit
    uint32_t  stride;        // +0x1c — entry size in bytes (F-012/F-019)
    uint32_t  sentinel;      // +0x20 — 0xffffffff empty-bucket template (F-015)
    uint32_t  field24;       // +0x24 — alignment/unannotated
    void*     allocator;     // +0x28 — backing allocator (F-015)
};
static_assert(sizeof(HashMap) == 0x30, "HashMap must be 0x30 (map stride 0x88-0x58)");
static_assert(offsetof(HashMap, count)       == 0x08, "F-012/F-019: count @ +0x08");
static_assert(offsetof(HashMap, bucketCount) == 0x0c, "F-012/F-019: bucketCount @ +0x0c");
static_assert(offsetof(HashMap, entries)     == 0x10, "F-012/F-019: entries @ +0x10");
static_assert(offsetof(HashMap, stride)      == 0x1c, "F-012/F-019: stride @ +0x1c");
static_assert(offsetof(HashMap, allocator)   == 0x28, "F-015: allocator @ block+0x28");

// ───────────────────────────────────────────────────────────────────────────
// TweakDB — the 0x168-byte heap instance (F-013).
//
// Per F-016: NO vtable. this+0x00 is plain zeroed data (constructor's first op
// is `stp q0,q0,[x0]` with q0=0). So there is nothing polymorphic here.
//
// Three structurally-identical hash maps (F-017):
//   +0x58  — hashMapA          flats candidate  (H-008 pending runtime verify)
//   +0x88  — hashMapB_records  records          (F-012 CONFIRMED)
//   +0x108 — hashMapC          queries candidate (H-008 inference)
// The flats-vs-queries label for A/C is INFERENCE only (F-019); do not treat
// it as fact. VerifyH008() resolves it at runtime from the live entry counts.
struct TweakDB {
    // +0x00..0x1F — zeroed 32 bytes; vtable slot is 0 (F-015 / F-016).
    uint8_t   pad00[0x20];
    // +0x20 — u16 atomic/refcount (F-015: dtor touches +0x20/+0x21).
    uint16_t  refcount;
    uint8_t   pad22[0x28 - 0x22];          // +0x22..0x27
    // +0x28 — ptr → 0x158-byte heap sub-object (F-015: flat-value pool/manager candidate).
    void*     subObjA;
    // +0x30 — ptr → 0xf8-byte heap sub-object (F-015: secondary manager).
    void*     subObjB;
    // +0x38 — u8 = 1: owns/initialized flag (F-015).
    uint8_t   flagInit;
    uint8_t   pad39[0x40 - 0x39];          // +0x39..0x3F
    // +0x40..0x4B — embedded lock/small container (F-015); opaque.
    uint8_t   embedded40[0x4c - 0x40];
    // +0x4c..0x57 — F-015 "+0x4c 8 bytes = 0" (`stur d0,[x19,#0x4c]`) plus the
    // 4-byte gap to +0x58. Kept as opaque bytes: a uint64_t here would land at
    // the unaligned offset 0x4c and force the compiler to pad-shift the whole
    // tail of the struct (caught by the offsetof asserts during bring-up).
    uint8_t   pad4c[0x58 - 0x4c];
    // +0x58 — HASH MAP A: flats candidate (F-015 / F-019 / H-008).
    HashMap   hashMapA;
    // +0x88 — RECORDS map (F-012 CONFIRMED).
    HashMap   hashMapB_records;
    // +0xB8..0xC3 — embedded sub-object (F-015); opaque.
    uint8_t   embedded_b8[0xc4 - 0xb8];
    // +0xC4 — u32 = 0 (F-015).
    uint32_t  pad_c4;
    // +0xC8..0xDF — embedded small hash map (F-015; count @ +0xd4); opaque.
    uint8_t   smallMap_c8[0xe0 - 0xc8];
    // +0xE0..0xEB — embedded sub-object (F-015); opaque.
    uint8_t   embedded_e0[0xec - 0xe0];
    // +0xEC — u32 = 0 (F-015).
    uint32_t  pad_ec;
    // +0xF0..0x107 — embedded container (F-015; +0xfc = 0); opaque.
    uint8_t   container_f0[0x108 - 0xf0];
    // +0x108 — HASH MAP C: queries candidate (F-015 / H-008).
    HashMap   hashMapC;
    // +0x138..0x147 — embedded container (F-015; count @ +0x144); opaque.
    uint8_t   container_138[0x148 - 0x138];
    // +0x148 — flat-data-buffer pointer (F-015: mirrors Windows flatDataBuffer@0x148;
    //          zeroed in ctor, populated at load time per F-018).
    const uint8_t* flatDataBuffer;
    // +0x150 — u32 buffer size/end (F-015).
    uint32_t  flatDataBufferSize;
    // +0x154..0x157 — 4-byte gap before the 8-byte capacity field (align).
    uint32_t  pad154;
    // +0x158 — 8 bytes = 0 in ctor: buffer capacity/ptr, populated at load (F-015).
    uint64_t  flatDataBufferCapacity;
    // +0x160 — u8 = 0 (F-015).
    uint8_t   field160;
    uint8_t   pad161[0x164 - 0x161];       // +0x161..0x163
    // +0x164 — u32, last field; struct ends at 0x168 (F-015).
    uint32_t  field164;
};

// ── Layout assertions: catch any drift from F-015 at compile time ────────────
static_assert(sizeof(TweakDB) == 0x168, "F-013: TweakDB must be exactly 0x168 bytes");
static_assert(offsetof(TweakDB, refcount)               == 0x20,  "F-015: refcount @ +0x20");
static_assert(offsetof(TweakDB, subObjA)                == 0x28,  "F-015: subObjA @ +0x28");
static_assert(offsetof(TweakDB, subObjB)                == 0x30,  "F-015: subObjB @ +0x30");
static_assert(offsetof(TweakDB, flagInit)               == 0x38,  "F-015: flagInit @ +0x38");
static_assert(offsetof(TweakDB, hashMapA)               == 0x58,  "F-015: hashMapA @ +0x58");
static_assert(offsetof(TweakDB, hashMapB_records)       == 0x88,  "F-012: records map @ +0x88");
// Spot-check that the records map's count lands at the F-012 absolute offset +0x90.
static_assert(offsetof(TweakDB, hashMapB_records) + offsetof(HashMap, count) == 0x90,
              "F-012: records count @ +0x90");
static_assert(offsetof(TweakDB, hashMapC)               == 0x108, "F-015: hashMapC @ +0x108");
static_assert(offsetof(TweakDB, flatDataBuffer)         == 0x148, "F-015: flat buffer ptr @ +0x148");
static_assert(offsetof(TweakDB, flatDataBufferSize)     == 0x150, "F-015: flat buffer size @ +0x150");
static_assert(offsetof(TweakDB, flatDataBufferCapacity) == 0x158, "F-015: flat buffer cap @ +0x158");

// ───────────────────────────────────────────────────────────────────────────
// Read-only accessor helpers (consumed by P1.5/P1.6/P1.10).
//
// These exist so downstream callers never hardcode offsets — they ask the
// accessor. The flats/queries *labels* on A/C are still candidates until
// VerifyH008() resolves H-008 at runtime; the helper names say "Candidate" to
// keep that honest.
inline const HashMap* GetRecordsMap(const TweakDB* db) {
    return db ? &db->hashMapB_records : nullptr;
}
inline const HashMap* GetFlatsMapCandidate(const TweakDB* db) {
    return db ? &db->hashMapA : nullptr;          // +0x58 (H-008: likely flats)
}
inline const HashMap* GetQueriesMapCandidate(const TweakDB* db) {
    return db ? &db->hashMapC : nullptr;          // +0x108 (H-008: likely queries)
}
inline const uint8_t* GetValueBuffer(const TweakDB* db) {
    return db ? db->flatDataBuffer : nullptr;     // +0x148
}
inline uint32_t GetValueBufferSize(const TweakDB* db) {
    return db ? db->flatDataBufferSize : 0u;      // +0x150
}
inline uint64_t GetValueBufferCapacity(const TweakDB* db) {
    return db ? db->flatDataBufferCapacity : 0ull; // +0x158
}

// ───────────────────────────────────────────────────────────────────────────
// H-008 verification.
//
// Logs the live entry counts of map A (+0x58) and map C (+0x108) exactly once
// per process to /tmp/red4ext-mac.log. Whichever map holds vastly more entries
// is flats; the other is queries (flats outnumber queries by orders of
// magnitude — F-019 / H-008).
//
// Call on first successful access — e.g. the P1.2 deferred sample or the P1.3
// polling loop, once the singleton is non-null and populated. Idempotent: the
// function records that it ran and is a no-op on subsequent calls.
//
// Emits (machine-parseable):
//   [tweakdb] H-008 verification: mapA(+0x58).count=N1 mapC(+0x108).count=N2 verdict=<v>
//     verdict ∈ { flats-is-A | flats-is-C | indeterminate }
// plus a raw diagnostic line cross-checking the F-015 map-C count-offset
// discrepancy (see TweakDB.cpp).
//
// Reads are done through mach_vm_read_overwrite so a partially-constructed or
// unmapped struct can never crash the game (same safety model as P1.2).
void VerifyH008(const TweakDB* db);

// ───────────────────────────────────────────────────────────────────────────
// P1.6 — flat value buffer R/W.
//
// How a flat's value is reached from its hash-map entry is NOT yet mapped on
// macOS. Two evidence-backed candidates (resolved in-game by VerifyFlatEntry):
//
//   BufferViaTdbOffset  — the stored TweakDBID's 24-bit tdbOffset (key +0x05)
//                         is a byte offset into the +0x148 value buffer
//                         (windows-tweakxl-api §"compact ID": tdbOffset is
//                         filled from the file at load = the buffer offset).
//   BufferViaEntryOffset— an int32 offset stored in the entry payload (+0x10),
//                         Windows' HashMap<TweakDBID,int32_t> model.
//   FlatValuePtrAtEntry — a ref-counted FlatValue* in the entry payload
//                         (F-019: +0x58 dtor releases a payload at entry+0x18).
//
// At the resolved value object, the layout mirrors Windows' TweakDBFlatValue
// (Buffer.cpp ResolveOffset): [vft ptr @ +0x00][value @ +valueDataOff]. For
// 4-byte scalars valueDataOff is +0x08 (after the 8-byte vft).
enum class FlatValueSource : uint8_t {
    Unknown,
    BufferViaTdbOffset,
    BufferViaEntryOffset,
    FlatValuePtrAtEntry,
    InlineEntry,            // value lives directly in the entry (no indirection)
};

struct FlatLayout {
    FlatValueSource source       = FlatValueSource::Unknown;
    uint16_t        entryFieldOff = 0x10;  // where the offset/ptr lives in an entry
    uint16_t        valueDataOff  = 0x08;  // value bytes within the value object
    bool            confirmed     = false; // VerifyFlatEntry validated it in-game
};

// The layout VerifyFlatEntry settled on (defaults to Unknown/unconfirmed).
// Read/WriteFlat refuse to operate until this is confirmed.
FlatLayout GetFlatLayout();

// Read a flat by TweakDBID. nullopt if not found, layout unconfirmed, or the
// type is not yet decodable. For confirmed scalar layouts this returns the raw
// 4-byte value as FlatType::Unknown carrying a uint32_t (the vft→FlatType map
// is a pending follow-up); callers reinterpret per the type they already know.
// Safe-read everywhere — never crashes.
std::optional<FlatValue> ReadFlat(const TweakDB* db, TweakDBID id);

// Write a flat by TweakDBID. Returns true on success. Scalar types only
// (Int32/Float/Bool); String/CName/arrays are rejected (logged) at this layer.
// No type coercion (P1.10's job): the caller-declared FlatType decides the
// write width. WRITE is the first mutation in the stack — every call is logged
// (target id, old bytes, new bytes). Atomic: validates the target slot is
// writable and in-bounds before committing; refuses otherwise.
bool WriteFlat(TweakDB* db, TweakDBID id, FlatValue newValue);

// ───────────────────────────────────────────────────────────────────────────
// F-031 CORRECT flat path. The old ReadFlat/WriteFlat resolve via the +0x58
// map — which F-029 proved is recordsByID, NOT flats — so they cannot target a
// real flat. These resolve the REAL flats SortedUniqueArray @ db+0x40 and the
// macOS FlatValue layout ([vtable @ +0x00][data @ +0x08], 0x10 bytes).
//
// ResolveFlatOffset: binary-search (linear if NotSorted) the +0x40 array by the
// 40-bit key {nameHash, nameLength}; returns the flat's tdbOffset into the
// flat-data buffer (+0x148), or -1 if the flat is absent.
int64_t ResolveFlatOffset(const TweakDB* db, TweakDBID key);

// EditScalarFlatInPlace: overwrite an EXISTING scalar flat's value at
// flatDataBuffer + tdbOffset + 0x08 (clears the runtime gate first). Interning
// caveat (F-031): pooled values are shared — editing one in place affects every
// flat pointing at the same FlatValue. New-flat allocation (interning-safe
// SetFlat) + UpdateRecord land next. Returns false if absent or non-scalar.
bool EditScalarFlatInPlace(TweakDB* db, TweakDBID flatId, FlatValue v);

// Interning-SAFE scalar flat write (F-031): allocates a NEW FlatValue (cloning
// the type's vtable) and repoints ONLY this flat's tdbOffset, so it never
// corrupts other flats sharing the original pooled value. Grows the flat buffer
// once if needed (rebasing defaultValues). This is what the applicator uses.
bool EditScalarFlatSafe(TweakDB* db, TweakDBID flatId, FlatValue v);

// Clear the runtime-write gate (db+0x160 = 0) — macOS analog of TweakXL's
// EnsureRuntimeAccess (F-031, inferred). Harmless if already 0.
void EnsureRuntimeAccess(TweakDB* db);

// Read a scalar flat's typed value via the +0x40 path (F-031). Returns nullopt
// if the flat is absent or non-scalar. Snapshot source for the applicator.
std::optional<FlatValue> ReadScalarFlat(const TweakDB* db, TweakDBID id);

// Self-test (once-only): read the +0x40 flats array header, log the first
// entries with their FlatValue vtables/values, and round-trip-resolve entry[0]
// by its own key. Proves the correct flat path works. Logs to red4ext-mac.log.
void VerifyGameSeesEdit(TweakDB* db);  // ground-truth: does the GAME's GetFlat see our edit?
void FindStatFlatByValue(TweakDB* db);  // identify HP/RAM flats by live value via game GetFlat
void StatOracle(TweakDB* db);  // F-037: read player's live RAM via the game (poll thread)
void VerifyFlatArrayAccess(const TweakDB* db);

// Title-screen (no-save) proof of the correct flat path: resolve real NAMED
// flats via +0x40 (incl. high-confidence BaseStats.*.value flats), and a
// round-trip EditScalarFlatInPlace on a real Float flat (edit→verify→restore).
void TestFlatWritePath(TweakDB* db);

// F-034: re-materialize a record from current flats (factory build + RTTI Assign
// @ vtable+0x50) so a flat edit reaches the record's reflected properties / the
// StatsContainer seed (F-030). Call after EditScalarFlatInPlace on the record's
// flats. Returns false if the record/accessors are unresolvable.
bool UpdateRecord(TweakDB* db, TweakDBID recordId);

// Probe (env TWEAKXL_PROBE_REFLECTED): report which candidate flats back an
// RTTI-reflected record field (edit+rebuild changes the record). No save.
void ProbeReflectedFlats(TweakDB* db);

// H-011 step 2a (env-gated TWEAKXL_TEST_UPDATEREC): build a record from edited
// flats via the game's factory and byte-compare to the live record — validates
// the baseHash accessor + factory call (game code) WITHOUT writing game state.
void TestUpdateRecordBuild(TweakDB* db);

// ───────────────────────────────────────────────────────────────────────────
// P1.6 Phase A — flat entry/value layout discovery (once-only).
//
// Dumps sample flats-map entries + candidate value objects, tests each value
// source hypothesis across several entries, sets the global FlatLayout, and
// re-confirms the flats-map hash function (tdbOffset is non-zero on flats, so
// fnv1a-8B vs fnv1a-5B is distinguishable — sharper than P1.5's records test).
// Emits to /tmp/red4ext-mac.log:
//   [flat-layout] verdict: <source> type-tag-offset:+0xNN buffer-offset-source:<..> ...
//   [flat-layout] sample[i]: hash=.. key=.. valueObj=.. vft=.. raw32=.. asI32=.. asF32=..
//   [hashmap] flats-map hash-function: <fnv1a-8B|fnv1a-5B|...>
void VerifyFlatEntry(const TweakDB* db);

// ───────────────────────────────────────────────────────────────────────────
// P1.12 — runtime candidate-flat verifier (once-only).
//
// Reads a newline-delimited candidate-name file (TWEAKXL_CANDIDATES_FILE if set,
// else derived from this dylib's path: <dylib dir>/../../../tools/probes/
// candidate_flats.txt), strips '#' comments + whitespace, and for each name
// computes TweakDBID{CRC32(name), strlen(name), {0,0,0}} and looks it up in the
// flats map with GetFlatsHashMode(). HIT → reads the FlatValue ptr at entry+0x18
// and dumps vft (+0x00) + raw value (+0x08). Pre-flight: first confirms a known
// RECORD id resolves in the records map, so an all-MISS run distinguishes a
// systemic lookup bug from genuine flat absence. Emits to /tmp/red4ext-mac.log.
void VerifyCandidateFlats(const TweakDB* db);

// ───────────────────────────────────────────────────────────────────────────
// P1.13 — flats map walker / cinema target prospecting (once-only).
//
// Bypasses the offline-name dead end (F-020 + the 297-cand sweep proving even
// canonical-looking <record>.<property> guesses miss): walks the +0x58 map's
// `entries` array linearly i = 0..min(count, maxN), dumps each non-empty entry's
// raw bytes (key, next-index, stored-hash, payload @ +0x18 + dereferenced
// FlatValue {vft, val64}), and accumulates a vft-frequency histogram. Names are
// NOT required — the dump exposes the live (id, vft, value) tuples directly so
// a Float cluster with meaningful defaults can be hand-picked for blind
// mutation. Sampling cap defaults to 500; env var TWEAKXL_DUMP_FLATS_MAX
// overrides. Emits to /tmp/red4ext-mac.log:
//   [flat-dump] map=+0x58 count=N stride=S bucketCount=B entries=PTR
//   [flat-dump] e[i] key=.. hash=.. len=.. tdb=.. flat=.. vft=.. val64=.. i32=.. f32=..
//   [flat-dump] vft-cluster[k]: vft=.. count=..
//   [flat-dump] sampled N entries (emitted M with plausible flatPtr+vft)
void DumpFlatsSample(const TweakDB* db, uint32_t maxEntries = 500);

// ───────────────────────────────────────────────────────────────────────────
// P1.14b — name → vtable mapping for type identification (once-only).
//
// Reads a newline-delimited record-names file (TWEAKXL_RECORDS_FILE if set,
// else <dylib dir>/../../../reference/tweakxl-data/record_names.txt — 63,711
// canonical names from psiberx TweakXL's InheritanceMap.yaml). For each name,
// computes its TweakDBID, looks it up in +0x58, and on HIT dumps the entry's
// p10 vtable. Aggregates per-vtable: (a) total HIT count, (b) up to N sample
// record names. The dominant prefix in each cluster's sample names (e.g.
// "Items.*", "Attacks.*", "Character.*") reveals which record class that
// vtable represents — closing F-025's "vtable → type name" mapping gap.
//
// Emits to /tmp/red4ext-mac.log and (if TWEAKXL_VFT_MAP_OUT set) a parseable
// "<vtable_hex>\t<record_name>" line per HIT to that path for post-processing.
void MapNamesToVftables(const TweakDB* db);

// ───────────────────────────────────────────────────────────────────────────
// P1.17 — cinema mutation (once-only).
//
// First end-to-end proof that the macOS framework can MUTATE live game state:
//   - Looks up a named gamedataStat_Record (e.g. "BaseStats.AccumulatedDoTDecayRate")
//   - Reads p10 via entry+0x10 (F-023)
//   - Reads the current float at p10+0x54 (F-027 — Stat_Record value slot)
//   - Writes a new float via mach_vm_write
//   - Re-reads to confirm
// Emits a machine-parseable summary line to /tmp/red4ext-mac.log:
//   [cinema] '<name>' before=<f> after=<f> kr=<int> ok=<bool>
// Driven by env vars (so this never fires unless explicitly opted in):
//   TWEAKXL_CINEMA_NAME — record name to mutate
//   TWEAKXL_CINEMA_VALUE — new float value (decimal)
// Skipped if either env var is missing.
void CinemaMutateStat(const TweakDB* db);

// ───────────────────────────────────────────────────────────────────────────
// P1.17b — BULK cinema mutation (once-only).
//
// Locates every named record whose vtable matches the live gamedataStat_Record
// vtable (detected from a reference Stat record), then multiplies every safe
// non-zero finite Float at p10+0x54 by TWEAKXL_CINEMA_BULK_FACTOR. Skips
// garbage (NaN/inf/0/wildly-out-of-range) to keep the game stable.
// Logs: count attempted, count mutated, count skipped, sample {name, before, after}.
// Env-gated:
//   TWEAKXL_CINEMA_BULK         — set to any value to enable
//   TWEAKXL_CINEMA_BULK_FACTOR  — float multiplier (default 100.0)
//   TWEAKXL_CINEMA_BULK_REPEAT  — seconds between re-application
//                                 (default 0 = one-shot)
// Skipped silently if TWEAKXL_CINEMA_BULK is unset.
void CinemaBulkMutateStats(const TweakDB* db);

// ───────────────────────────────────────────────────────────────────────────
// P1.20 — process-wide float memory scan.
//
// Walks all writable VM regions of THIS process (the game) via
// mach_vm_region_recurse and searches each for the IEEE-754 bits of one or
// two target floats (TWEAKXL_SCAN_FLOAT_A and optionally TWEAKXL_SCAN_FLOAT_B).
// Reports every address whose 4-byte aligned read equals the target. Limits
// results per target so a popular value doesn't blow the log.
//
// Cinema-debug use: mutate a known TweakDB Float to a distinctive value
// (e.g. 13.371337) via CinemaMutateStat, then scan for it after the game has
// loaded a save — any address ≠ the mutation source is a downstream READER.
//
// Env-gated (does nothing unless TWEAKXL_SCAN_FLOAT_A is set):
//   TWEAKXL_SCAN_FLOAT_A         — primary target float (decimal, e.g. "13.371337")
//   TWEAKXL_SCAN_FLOAT_B         — optional second target
//   TWEAKXL_SCAN_DELAY_SEC       — sleep N seconds before scanning (default 30,
//                                  to give the user time to load a save)
//   TWEAKXL_SCAN_MAX_HITS        — cap per-target results (default 100)
//   TWEAKXL_SCAN_OUT             — optional path for tab-separated address
//                                  output (default /tmp/scan_hits.tsv)
//
// Safety: read-only via mach_vm_read_overwrite. Skips non-writable, non-
// private, and __TEXT regions. Will NOT mutate anything.
void ScanMemoryForFloat(const TweakDB* db);

// ───────────────────────────────────────────────────────────────────────────
// P1.21 — UNLEASH: aggressive shotgun mutation across all 193K records.
//
// Walks every entry in the +0x58 map. For each record's p10 (the typed record
// instance), scans bytes from +TWEAKXL_UNLEASH_OFFSET_START to +TWEAKXL_UNLEASH_OFFSET_END
// every 4 bytes. Any 4-byte aligned float in [MIN, MAX] (current value) gets
// multiplied by FACTOR (clamped to avoid inf). Re-applies every REPEAT seconds.
//
// This is the "set all damage to infinity" sledgehammer — it doesn't know which
// fields are damage vs range vs multiplier, it just nukes every plausible
// gameplay-magnitude float in every record's body. Something will visibly
// change unless the gameplay system never reads any value from p10 directly.
//
// Env-gated:
//   TWEAKXL_UNLEASH=1                  — enable
//   TWEAKXL_UNLEASH_FACTOR (1000.0)    — multiply by this
//   TWEAKXL_UNLEASH_MIN (1.0)          — lower bound of "plausible value"
//   TWEAKXL_UNLEASH_MAX (200.0)        — upper bound
//   TWEAKXL_UNLEASH_OFFSET_START (0x48)
//   TWEAKXL_UNLEASH_OFFSET_END (0x100)
//   TWEAKXL_UNLEASH_REPEAT (2)         — seconds between re-applications
//   TWEAKXL_UNLEASH_NAME_SUBSTR        — only records whose name contains this
//                                        substring (uses InheritanceMap names);
//                                        empty = ALL records
void CinemaUnleash(const TweakDB* db);

} // namespace red4ext_mac
