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
