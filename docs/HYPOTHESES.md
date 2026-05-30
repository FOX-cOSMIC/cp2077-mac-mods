# HYPOTHESES.md — Theories Being Tested

**Owner:** any agent may add; `doc-keeper` prunes stale entries.

Hypotheses go here while they're being tested. Each entry must resolve to either `FACTS.md` (validated) or `FAILED_APPROACHES.md` (disproved) within **14 days**. Stale hypotheses are demoted by `doc-keeper`.

---

## How to Add an Entry

```markdown
### H-NNN: <one-line claim>

- **Proposed:** YYYY-MM-DD by <agent>
- **Why we think this:** <reasoning, prior session evidence, RE clues>
- **How to test:** <experiment design>
- **Status:** open | in-progress | resolved-fact | resolved-failed
- **Owner:** <agent assigned to test>
```

---

## How to Resolve

When you have evidence:

- **Confirmed:** create the matching `F-NNN` in `FACTS.md`, mark this hypothesis `resolved-fact`, link both ways.
- **Disproved:** create the matching `FA-NNN` in `FAILED_APPROACHES.md`, mark this hypothesis `resolved-failed`, link both ways.

Resolved hypotheses can be deleted after 30 days (the source of truth is now in FACTS or FAILED).

---

## Open Hypotheses

*Seed hypotheses are below — these come from the old work and need re-validation on the current game build before they can be promoted to FACTS.*

### H-001: Game's TweakDB storage uses a function-pointer dispatch table, not raw data

- **Proposed:** 2026-05-28 (carried from old session 62e, dated 2025-12-14)
- **Why we think this:** Old dump showed `staticFlatDataBuffer` contained code pointers (e.g., `0x102f53b04`) rather than data values. Hook on entry[16] reportedly fired at startup.
- **How to test:** Re-dump the structure at the same offset on the current game build. Disassemble entries[0..20] and check if they're code (function prologues) or data (TweakDBID-shaped values).
- **Status:** **resolved-failed (2026-05-29)** — DISPROVED. The TweakDB constructor `FUN_102b73db8` writes no function pointers; `this+0x00` is zeroed (no vtable, F-016); flats/records/queries are plain hash tables (+0x58/+0x88/+0x108) backed by a flat-value buffer at +0x148, not a code-pointer dispatch table. See **F-015/F-016/F-017** and **FA-011**.
- **Owner:** researcher
- **Resolved by:** F-017 (disproved); FA-011

### H-002: macOS TweakDB struct is 0x198 bytes, not Windows's 0x168 bytes

- **Proposed:** 2026-05-28 (carried from old session 20)
- **Why we think this:** Old finding that Windows offsets (`0x148` for flatDataBuffer) didn't match macOS layout.
- **How to test:** Locate TweakDB constructor in current Ghidra output, measure struct size from allocation site.
- **Status:** **resolved-failed (2026-05-29)** — DISPROVED. The singleton initializer `FUN_102b73b50` allocates `0x168` bytes (`mov w0,#0x168; bl Allocate`), so the macOS struct is `0x168` — the SAME as Windows, not `0x198`. See **F-013** and **FA-010**. The old session-20 figure was wrong on build 2.3.1.
- **Owner:** researcher

### H-003: VTABLE[5] of TweakDB is a reliable startup-fire hook

- **Proposed:** 2026-05-28 (carried from old session 14)
- **Why we think this:** Old session showed VTABLE[5] firing hundreds of times from startup, useful as init trigger.
- **How to test:** Hook VTABLE[5] on current build, log first-fire timestamp relative to game launch.
- **Status:** open
- **Owner:** researcher

### H-004: Inline `__TEXT` hooking remains blocked under current macOS / current game build

- **Proposed:** 2026-05-28 (carried from old session 5 — well-established but should be smoke-tested)
- **Why we think this:** macOS code signing enforced; old `mprotect()` attempts returned EPERM.
- **How to test:** Attempt `mprotect(addr & ~PAGE_MASK, 4096, PROT_READ|PROT_WRITE|PROT_EXEC)` on a `__TEXT` page; expect EPERM.
- **Status:** open
- **Owner:** researcher

### H-005: The TweakDB singleton getter / flat accessor exists as an unnamed internal function, locatable by xref (not by symbol or dlsym)

- **Proposed:** 2026-05-29 by Scope (researcher)
- **Why we think this:** F-006 established the binary is not export-stripped (67,928 exports) but exposes no TweakDB function symbol. F-008 confirmed it more precisely: `game::data::TweakDB` exists (with `GetRecords<T>()` const members and full RTTI), but **every** TweakDB symbol is DATA-only (guard variables + function-local statics) — there are zero `T`/`t` function-entry symbols, and they are NOT in the unscanned symbol range (they have no name at all; inlined / internal linkage). The functions clearly exist (the game reads/writes flats at runtime). The leverage is therefore xref **from the named DATA statics** (each function-local static is referenced by exactly one function — its owner).
- **How to test (refined with F-008 anchors):**
  1. **Primary entry point:** In Ghidra, go to static `0x1073bbea8` (the `emptyRecords` static local of `TweakDB::GetRecords<Vehicle_Record>`). It has exactly one referencing function — that function IS `game::data::TweakDB::GetRecords<Vehicle_Record>() const`. Disassemble it: as a `const` member it dereferences `this` (the TweakDB instance), exposing the struct offset of the records collection and the internal access path. Repeat for 1–2 other `GetRecords<T>` statics (e.g. `0x1073d9af0` District, `0x1073e5980` LifePath) to triangulate the struct layout (feeds H-002).
  2. **Singleton hunt:** find the callers of those `GetRecords` functions — they must obtain the `TweakDB*` (the `this`) from somewhere. That source is the singleton getter / global the game uses. 
  3. **Secondary hub:** static `0x1073af788` (the `game::data::TweakDBID` RTTI type-object pointer) is read from many call sites; its writer is `GetTypeObject<TweakDBID>()` and `RegisterGameTweakDBRTTI` (`0x1073eeab0`) — useful for mapping the TweakDB record/flat type registration.
  4. Record the static address of the most stable candidate (singleton getter or init/load). Convert to runtime with `static + slide` (F-004), using the slide from the launch's probe log (F-007). Confirm by reading the bytes (see redefined T-002b).
- **Status:** **resolved-fact (2026-05-29)** — RESOLVED via T-002c Ghidra xref. The singleton getter `FUN_102b73c7c` (@ static `0x102b73c7c`), the global instance pointer (`0x1080c92d0`, `__bss`), the initializer (`FUN_102b73b50` @ `0x102b73b50`), and a concrete member function `GetRecords<Vehicle_Record>` (`FUN_10121f264` @ `0x10121f264`) were all identified by xref from the F-008 anchor `0x1073bbea8`. See **F-011** (singleton path), **F-012** (GetRecords + layout), **F-013** (struct size + hash-table storage). The accessor is reachable; the original premise (find via xref, not symbol/dlsym) was correct.
- **Owner:** researcher
- **Resolved by:** F-011 / F-012 / F-013

### H-006: TweakDB flats are stored in a second hash map within the 0x168 struct, parallel to the records map

- **Proposed:** 2026-05-29 by Scope (researcher)
- **Why we think this:** F-012 showed the *records* map is a hash table at `this+0x88…+0xa4` inside the `0x168` struct (F-013). The struct has room before `+0x88` and after `+0xa4` for a second, structurally-similar map. TweakXL on Windows treats flats and records as distinct collections; the macOS struct almost certainly mirrors this with a separate flats hash map (the thing mod-loader must write to apply flat Assign/Append/Remove).
- **How to test:** Disassemble the TweakDB constructor `FUN_102b73db8` (@ static `0x102b73db8`) to enumerate every field it initializes across the `0x168` block; identify a second bucket-array/entries/stride triple (the flats map) and its base offset. Cross-check by finding a flat-read function (xref from a flat-typed RTTI or a `GetFlat`-shaped access) and confirming it indexes the same offsets.
- **Status:** **resolved-fact (2026-05-29)** — CONFIRMED in substance. The constructor (F-015) shows **three** structurally-identical hash maps parallel to the records map (+0x58 / +0x88=records / +0x108), plus a flat-data-buffer region at +0x148. Flats are stored as a hash table + value buffer, same family as records (F-017). **Open detail (narrowed, not blocking H-006):** which non-records block (+0x58 vs +0x108) is flats vs queries is inferred from declaration order, pending a `GetFlat` accessor xref — tracked as the T-002e follow-up below, NOT as part of H-006.
- **Owner:** researcher
- **Resolved by:** F-015 / F-017
- **Note:** This is the direct prerequisite for hook-target approach F-014(a) (direct data manipulation) — mod-loader cannot write flats without the byte-exact block assignment, so the T-002e `GetFlat`-xref step must precede any flat-writing code.

### H-007: The DB-populate/load path (not the FUN_102b73b50 constructor) is the correct mods-apply hook point

- **Proposed:** 2026-05-29 by Scope (researcher)
- **Why we think this:** F-011 showed `FUN_102b73b50` constructs an EMPTY TweakDB (allocate + bzero + ctor). The records/flats maps are populated later, from the on-disk TweakDB data (the engine has `TweakDBReloader`/`TweakDBResource` machinery — seen in exported symbols, F-006/F-008). Mods must be applied AFTER that load completes, or they'll be overwritten.
- **How to test:** Locate the function that fills the records/flats maps (xref the maps' writer sites, or trace `TweakDBResource` text-format resource loader / `TweakDBReloader` symbols). Find the post-load completion point. That is the timing signal for mod application.
- **Status:** **resolved-fact (2026-05-29)** — CONFIRMED. The populate path is the initial-load orchestrator `FUN_102b75744` (@ static `0x102b75744`): it builds `{root}/tweakdb.bin` (via `FUN_102b75b48` @ `0x102b75b48`, string-confirmed), reads the file, and parses/inserts via `FUN_102253964`. Completion of `FUN_102b75744` = DB populated = the mods-apply point; the constructor (F-011/F-015) is confirmed NOT the trigger (builds an empty DB). See **F-018**.
- **Owner:** researcher
- **Resolved by:** F-018

### H-008: TweakDB flats hash map is the +0x58 block (queries = +0x108)

- **Proposed:** 2026-05-29 by Scope (researcher)
- **Why we think this:** F-019 confirmed +0x58 is a peer hash map (identical layout to records, holds ref-counted payloads via dedicated destructor `FUN_100ad1df0`). Records = +0x88 (F-012, confirmed). RED declaration order is flats, records, queries → flats=+0x58 (before records), queries=+0x108 (after). The flat-value buffer at +0x148 sits after all three maps, mirroring the Windows layout. **Inference only — not yet confirmed by a macOS accessor (FA-006 caution against trusting Windows-order analogy).**
- **How to test (two cheap options):**
  1. **Static:** xref a `GetFlat`/flat-read function that walks one of the maps AND indexes the +0x148 buffer; whichever block it walks (+0x58 vs +0x108) is flats.
  2. **Runtime (cheapest, decisive):** from the injected dylib, read the live singleton (`*(0x1080c92d0+slide)`) after load (F-018) and compare the entry counts of the +0x58 vs +0x108 maps — flats vastly outnumber queries (tens of thousands vs hundreds), so the larger map is flats.
- **Status:** **resolved-fact (2026-05-29)** — CONFIRMED via Option 2 (runtime). P1.4's smoke test (`tools/test-tweakdb-access.sh`) injected `libred4ext.dylib` into a fresh stock 2.3.1 game launch, waited 15 s for full DB populate, and read the live struct via the singleton: `mapA(+0x58).count=193354`, `mapC(+0x108).count=12`, `records.count=843`, `valueBufferSize=4,291,664 B`. The 16,000:1 ratio between mapA and mapC is decisive — **flats are at +0x58, queries at +0x108**, exactly matching the Windows declaration-order analogy. Evidence captured in `docs/probes/logs/red4ext-mac-2026-05-29-p1-4.log`. **One side-finding:** the F-015 annotation of `hashMapC.count @ +0x114` (block-relative +0x0c, the *bucketCount* slot) was the row that prompted Schema to log BOTH +0x08 and +0x0c — runtime shows `mapC{@08=12,@0c=13}`, so count is at +0x08 like the other maps (F-019/F-012), not +0x0c. Recommend Scope formalize the flats=+0x58 confirmation and the count-offset uniformity as F-020.
- **Owner:** researcher
- **Resolved by:** P1.4 smoke test in-game evidence (live game build 2.3.1)

### H-009: Individual records live in the +0x58 map (combined flat+record id map), not a separate by-id map

- **Proposed:** 2026-05-29 by Scope (researcher)
- **Why we think this:** F-021 established +0x88 (843) is records-BY-TYPE, not by-id, and the +0x28 sub-object is a type/reflection registry — so the ~12,951 individual records are NOT in any structure found so far. The +0x58 map holds **193,354** entries ≈ (~180K flats + ~13K records): a strong hint that +0x58 is the *universal TweakDBID → entry* map holding flats AND records together (Schema's H-008 only distinguished it from +0x108 by size and assumed flats-only). If true, record-targeting mods (clone/edit a record) resolve through the same +0x58 path as flats.
- **How to test (decisive, cheap — Schema/runtime):** in the P1.12-style probe, look up a known **record** id against the **+0x58** map: `Items.Preset_Nova_Default` → CRC32 `0xb1e27e8e`, build compact key {0xb1e27e8e, len=25, off=0}, `Lookup(mapA, id, flatsHashMode)`.
  - **HIT** → +0x58 is the combined flat+record id map; records live there. (Then inspect the entry payload to see how record vs flat entries differ.)
  - **MISS** → records are in a yet-unfound structure (next: disassemble the by-id `GetRecord(TweakDBID)` function and the +0x30 0xf8-byte sub-object `FUN_102b76e78`).
- **Status:** **resolved-fact (2026-05-29)** — CONFIRMED. P1.14b's `MapNamesToVftables` probe looked up all **63,711** canonical record names (from psiberx's `InheritanceMap.yaml`) against the **+0x58** map: **63,699 HIT (99.98%)**, the 12 misses all stale/removed records. This proves individual records live in +0x58 by name (the universal TweakDBID→entry map), exactly as hypothesised — records and flats coexist there (193,354 entries). The hits further cluster into **269 distinct record-class vtables**, read via `entry+0x10 → p10 → *p10 = vtable`. See **F-026** (vtable→class map), built on **F-023** (entry+0x10 indirection) and **F-025** (entries are typed record instances). Record-targeting mods therefore resolve through the same +0x58 path as flats.
- **Owner:** researcher (static) / Schema (runtime probe)
- **Resolved by:** F-026 (+ F-023, F-025)

### H-010: The macOS TweakDB struct matches the Windows RED4ext.SDK layout exactly — we mislabeled the maps; real flats are at +0x40, +0x58 is recordsByID

- **Proposed:** 2026-05-30 by Conductor (from psiberx Windows TweakXL + RED4ext.SDK `TweakDB.hpp`)
- **Why we think this:** The authoritative Windows struct (`reference/windows-tweakxl/vendor/RED4ext.SDK/include/RED4ext/TweakDB.hpp`, `RED4EXT_ASSERT_SIZE 0x168`) has: `flats` SortedUniqueArray<TweakDBID> @ **0x40**; `recordsByID` HashMap @ **0x58**; `recordsByType` HashMap @ **0x88**; `queries` Map @ 0xB8; `groups` Map @ 0xE0; `defaultValues` HashMap<CName,FlatValue*> (lazy, tiny) @ **0x108**; `flatDataBuffer` @ **0x148**; `unk160` @ **0x160**. Our macOS runtime probes found HashMaps at exactly +0x58/+0x88/+0x108 (matching the three Windows HashMaps' offsets), and +0x108's tiny ~10-entry count matches "lazily-created defaultValues" — NOT queries. So macOS layout == Windows.
- **The consequences (corrections):**
  - **+0x58 is `recordsByID`, NOT flats.** That's why 63,699 *record* names HIT there (F-026/H-009 conclusion was right; the F-020/F-022 *label* "flats" was wrong). The 193,354 count = all records incl. auto-`_inline` ones.
  - **The actual flats (`SortedUniqueArray<TweakDBID>` @ +0x40) were NEVER located** — it's a sorted array, not a hashmap, so our hashmap-centric probes missed it. This is the real mod-write target.
  - **+0x108 is `defaultValues`, not queries** (F-022 mislabel). Queries are the Map @ 0xB8.
  - Flat VALUES are pooled `FlatValue*` objects in `flatDataBuffer` (+0x148): `[vtable @0x00][T data @0x10]`, size 0x20 for scalars. A flat = a TweakDBID whose embedded tdbOffset points to its FlatValue. Values are INTERNED (RED4ext.SDK comment L34: int '1' exists once; editing it affects all records sharing it).
  - **Why the cinema poke failed:** we wrote the RECORD object's inline bytes (`p10+0x54`, from recordsByID @ +0x58), never the flat's FlatValue in the buffer. The game reads stat = record → flat field (offset) → FlatValue in buffer. Also, after changing a flat you must call **`UpdateRecord`** (TweakDB.hpp L206-209: "Updates all the value offsets inside the record") so the record's cached offsets re-point; and clear **`unk160`** (+0x160) to enable runtime writes (TweakXL `EnsureRuntimeAccess`).
- **How to test (macOS, decisive):** (1) read the singleton +0x40 as a SortedUniqueArray<TweakDBID> {ptr,size,cap}; confirm size ≈ number of flats (hundreds of thousands) and that `Items.*.<prop>` flat IDs binary-search-hit there. (2) For a known flat, decode its tdbOffset → index into +0x148 buffer → FlatValue → read `data@+0x10`; confirm it equals the stat's value. (3) In Ghidra, find the macOS `UpdateRecord` + the flats-array accessor + `unk160` writer. The running Ghidra consumer-trace agent should confirm the read path lands on +0x40/+0x148, not +0x58 inline.
- **Status:** **resolved-fact (2026-05-30)** — CONFIRMED on macOS by the Ghidra consumer-trace: `GetFlat` (`FUN_102b76708`) binary-searches a SORTED key array at **db+0x40** (= the `flats` SortedUniqueArray we'd never located) and returns a pointer into the +0x148 buffer; `GetRecord` (`FUN_102b745d0`) reads the **+0x58** by-id map. Exactly the Windows layout. See **F-029** (accessors + layout confirmation) and **F-030** (the consumer reads a cache, not live TweakDB). The +0x58="flats"/+0x108="queries" labels in F-020/F-022 are corrected to recordsByID / defaultValues.
- **Owner:** researcher
- **Resolved by:** F-029 / F-030

### H-011: UpdateRecord (propagate flat edits to record cache) — RE blueprint + the baseHash blocker

- **Proposed:** 2026-05-30 by Conductor (Ghidra decompile of CreateTDBRecord + factories + ctor)
- **Goal:** after `EditScalarFlatInPlace`, re-materialize the affected record so its cached fields (and the StatsContainer seed, F-030) reflect the new flat value.
- **Mechanism (decompile-confirmed):**
  - `CreateTDBRecord = FUN_1026b8db8(TweakDB* db, uint32 baseHash, TweakDBID recordId)`: `switch(baseHash & 0x1f)` → per-class **factory(baseHash, recordId)** (takes NO db — reads flats from the GLOBAL buffer, so our live edits are seen) → returns a freshly-built record `rec`. Then `nativeType = rec->vtable[1](rec)` (GetNativeType, slot +0x08); wrap in handle `FUN_102104788(&h, rec)`; insert `FUN_102b74408(db, recordId, nativeType, &h)` into recordsByID(+0x58, FUN_102b46f14) + recordsByType(+0x88, FUN_102b7751c, append).
  - **Factory table (baseHash&0x1f → addr):** 0=`FUN_1027052ac` 1=`FUN_102726504` 2=`FUN_10274f578` 3=`FUN_102775fc0` 4=`FUN_102798288` 5=`FUN_1027bb6f8` 6=`FUN_1027dc118` 7=`FUN_1027f6068` 8=`FUN_10280d970` 9=`FUN_10282a38c` 10=`FUN_102852278` b=`FUN_1028681f8` c=`FUN_10287d218` d=`FUN_1028a84b8` e=`FUN_1028bbe1c` f=`FUN_1028d2c10` 10=`FUN_1028f89d4` 11=`FUN_102913214` 12=`FUN_1029245e0` 13=`FUN_102946060` 14=`FUN_10296d3b8` 15=`FUN_1029a0100` 16=`FUN_1029c7e90` 17=`FUN_1029eb910` 18=`FUN_102a01e44` 19=`FUN_102a1827c` 1a=`FUN_102a681f0` 1b=`FUN_102a800a4` 1c=`FUN_102a9a66c` 1d=`FUN_102ad3a20` 1e=`FUN_102aec110` 1f=(get from CreateTDBRecord tail).
  - Each factory is a nested-if on the FULL baseHash (per-type constant, e.g. `-0x7ec94680`); allocates a `PoolGMPL_TDB_Records` record of the type's size (0x58..0x5f8), constructs, sets vtable, wires `.property`→flat-id via `FUN_103453b14`, returns. **baseHash is NOT written into the record.**
  - Empty-map init (ctor `FUN_102b73db8`): recordsByID @ +0x58 = zero +0x58..+0x77, `+0x78 = 0xffffffff`, `+0x80 = allocator` (from `FUN_102b045d0`); recordsByType @ +0x88 similar (decompile the ctor tail for exact fields).
- **THE BLOCKER (open):** runtime `baseHash` for an existing record is not directly readable. Options to resolve first: (a) build a **vtable→baseHash** table by parsing each factory's `if (baseHash==CONST) ...*rec = &VTABLE` branches (deterministic, ~tedious); (b) reproduce murmur3 of the type name (seed `0x5eedba5e`, builder `FUN_1026b8ce8`) — confirm the exact string + variant; (c) instrument the build loop `FUN_102b16a48` to capture (vtable,baseHash) pairs live.
- **Two impl paths (after baseHash resolved):** (1) **fake-DB**: 0x168 struct, empty +0x58/+0x88 maps (replicate ctor init + copy real allocator), call `CreateTDBRecord(fakeDB, baseHash, recordId)`, extract the single record from fakeDB.recordsByID (`*(entry+0x10)`); (2) **factory-direct**: call factory[cat](baseHash, recordId) directly (skips insert), get rec. Either way then copy `rec`→realRec via `nativeType->Assign` (find CClass Assign vtable slot) or a scalar-field copy (header-preserving).
- **Safest first test (no game-state write):** build `rec` from edited flats, byte-compare `rec[0..0x90]` vs realRec — proves the factory reads our edit — before any copy to realRec.
- **Status:** open — blueprint complete; needs the baseHash resolution then careful incremental in-game (no-save) testing. Crash-prone; implement as a focused unit.
- **Owner:** researcher (baseHash table) → engineer (impl)
