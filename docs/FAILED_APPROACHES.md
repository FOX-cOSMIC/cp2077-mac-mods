# FAILED_APPROACHES.md — Don't Repeat These

**Owner:** any agent appends after disproving a hypothesis or hitting a dead end. `doc-keeper` may merge duplicates but **never removes entries**. This file is append-only forever.

If you are tempted to try something, search this file first.

---

## How to Add an Entry

```markdown
### FA-NNN: <one-line summary>

- **Date:** YYYY-MM-DD
- **Tried by:** <agent or session>
- **Source:** <where this lesson came from — old session #, current failed PR, etc.>
- **Approach:** <what was tried>
- **Why it failed:** <root cause, with evidence>
- **What to do instead:** <correct approach, or "see F-NNN">
```

---

## Entries (Seeded from Prior Work, Nov-Dec 2025)

The following entries carry forward hard-won lessons from ~17 prior Claude sessions. Each is tagged with a source session for traceability. Re-validation is **not** required to trust these — they're documented dead ends.

---

### FA-001: Inline `__TEXT` hooking via `mprotect` / `vm_protect`

- **Date:** 2025-11-XX (Session 5)
- **Tried by:** prior Claude sessions
- **Source:** `Programming/new-claude/CLAUDE.md` ("CRITICAL DISCOVERY (Session 5)")
- **Approach:** Patch game code in-place by changing page protection of `__TEXT` pages from R-X to RWX, write hook instructions, change back.
- **Why it failed:** macOS code signing enforces `__TEXT` immutability. `mprotect()` returned errno 13 (EPERM). `vm_protect()` returned KERN_PROTECTION_FAILURE (kr 2). Hardened Runtime + page protection cannot be bypassed in this way.
- **What to do instead:** Use `__DATA`-only hook strategies — GOT entries, VTable slots, function-pointer tables. All of these are writable.

---

### FA-002: Offline binary patching of `tweakdb.bin`

- **Date:** 2025-10-XX
- **Tried by:** prior Claude sessions (project `cp2077-tweakxl-mac` at `/Users/lucas_1/cp2077-tweakxl-mac/`)
- **Source:** REVISED_PLAN.md, mac-port-plan.md in that repo
- **Approach:** Build a CLI tool that reads `tweakdb.bin`, applies mod operations on disk, writes a modified `tweakdb.bin` before launch.
- **Why it failed:** Three problems:
  1. Binary format incompatibilities between Windows-derived spec (from WolvenKit) and the macOS binary.
  2. Offline patching cannot support any RED4ext plugin (only TweakXL — and even then only a subset).
  3. Mod load order and dependency handling become inflexible compared to runtime.
- **What to do instead:** Runtime hooking via the red4ext-mac framework. Apply mods at TweakDB init time, in memory.

---

### FA-003: Direct memory write to TweakDB flats using Windows layout (binary search + 24-bit offset)

- **Date:** 2025-11-27 (Session 20)
- **Tried by:** prior Claude sessions
- **Source:** `Programming/new-claude/TWEAKDB_ACCESS_STRATEGY.md` ("CRITICAL UPDATE (Session 20)")
- **Approach:** Capture TweakDB singleton pointer, binary-search the `flats` sorted array at offset `0x40`, extract 24-bit `tdbOffset` from the TweakDBID, write directly to `flatDataBuffer + offset`.
- **Why it failed:** macOS TweakDB layout is structurally different from Windows. At offset `0x40` there is **no sorted array** — the field is NULL. Offset `0x48` (expected count) contained `193048544` (garbage). Memory at the singleton pointer showed repeated sentinel pointers consistent with a hash table bucket structure, not the Windows sorted-array layout.
- **What to do instead:** Re-validate the actual macOS TweakDB struct via researcher work. Do not assume Windows offsets transfer. The macOS struct appears to be 0x198 bytes (see H-002), and storage is likely either a hash table or function-pointer dispatch table (see H-001).

---

### FA-004: Searching for `GetFlat` / `SetFlat` as virtual functions in TweakDB vtable

- **Date:** 2025-11-XX (Sessions 14-16)
- **Tried by:** prior Claude sessions
- **Source:** `Programming/new-claude/VTABLE_ANALYSIS.md`, `VTABLE16_FINDINGS.md`
- **Approach:** Hook each vtable entry from VTABLE[5] through VTABLE[29], check which one fires when reading/writing a TweakDB flat.
- **Why it failed:** Only 12% of vtable hits corresponded to TweakDB operations. `GetFlat` / `SetFlat` are very likely **non-virtual** on macOS (or inlined). Vtable scanning is a dead end for this access pattern.
- **What to do instead:** Hook the function-pointer table that `staticFlatDataBuffer` actually points to (see H-001), or find the non-virtual `GetFlat`/`SetFlat` symbols directly via Mach-O `.symtab` and hook them with GOT entries.

---

### FA-005: VTABLE[8] of TweakDB as the `Load()` trigger

- **Date:** 2025-11-25 (Session 14)
- **Tried by:** prior Claude sessions
- **Source:** `Programming/new-claude/SESSION_14_SUMMARY.md` ("Critical Discovery: VTABLE[8] vs VTABLE[5]")
- **Approach:** Hook VTABLE[8] expecting it to be `TweakDB::Load()` based on a Nov 24 test that showed it firing 40 times.
- **Why it failed:** Subsequent builds showed VTABLE[8] **never fires** during gameplay, save loading, or mission completion. The game build changed and the vtable layout shifted, OR the Nov 24 finding was misinterpreted.
- **What to do instead:** Use VTABLE[5] as the startup-fire trigger (see H-003). Generally: **all vtable-slot findings are dated** and must be re-validated after any game update. Don't hardcode slot numbers without a refresh test.

---

### FA-006: Trusting old findings across game updates

- **Date:** 2025-11-25 (Session 14, lesson learned)
- **Tried by:** prior Claude sessions
- **Source:** `Programming/new-claude/SESSION_14_SUMMARY.md` ("Lessons Learned" section)
- **Approach:** Build on results from a prior session without re-validating that the game version is unchanged.
- **Why it failed:** Game patches can move function addresses, change struct sizes, alter vtable layouts. A finding that was true on game v2.3 may be false on v2.4. Multiple sessions wasted time building on stale assumptions.
- **What to do instead:** Always log game version from `Info.plist` at the start of any test/research session. If the version changed since a fact was recorded, treat the fact as a hypothesis until re-verified.

---

### FA-007: Direct memory write without hooking the access path

- **Date:** 2025-12-XX (Session 62 series)
- **Tried by:** prior Claude sessions
- **Source:** `Programming/new-claude/PROJECT_STATUS.md` ("Direct Memory Attempts (Abandoned)")
- **Approach:** Locate the data buffer for a flat value in memory, write the new value directly, expect the game to read the new value.
- **Why it failed:** The discovered "data buffer" turned out to be a **function-pointer table**, not raw data (see H-001). Direct memory writes did not affect gameplay because values are accessed via function dispatch, not direct reads. Memory writes succeeded (no crash) but had no effect.
- **What to do instead:** Hook the function-pointer table entries so that intercepted calls return modified values. The dispatch path, not the storage path, is what controls observed behavior.

---

### FA-008: Mixing investigations within a single session

- **Date:** ongoing (meta-lesson)
- **Tried by:** prior Claude sessions
- **Source:** general observation across session summaries
- **Approach:** "While I'm here let me also check X" during what was supposed to be a focused investigation.
- **Why it failed:** Cross-contaminated findings. Hard to isolate cause and effect. Sessions ended with a mixed bag of half-validated claims.
- **What to do instead:** **One question per investigation.** Plan it, run it, log the result, then start a new investigation if needed. Append findings to FACTS / HYPOTHESES / FAILED_APPROACHES individually.

---

### FA-009: dlsym for `RED4ext::TweakDB::Get*` mangled names against the stock binary

- **Date:** 2026-05-29
- **Tried by:** Scope (researcher), via the H-001 probe
- **Source:** `/tmp/h001-probe.log` (earlier probe build) + independent `nm` verification; T-002
- **Approach:** `dlsym(RTLD_DEFAULT, ...)` for mangled-name candidates of a TweakDB singleton getter: `_ZN6RED4ext7TweakDB3GetEv` (RED4ext::TweakDB::Get), `_ZN7TweakDB3GetEv` (TweakDB::Get), `_ZN6RED4ext7TweakDB11GetInstanceEv`, `_ZN6RED4ext7TweakDB14GetDefaultInstEv`, `_ZN8RED4ext7TweakDB3GetEv`. All returned `not found`.
- **Why it failed:** Two independent reasons, both confirmed:
  1. **Wrong namespace.** `RED4ext::*` is the *Windows mod SDK's* wrapper namespace — it does not exist in CDPR's shipped binary. The engine's actual types live under `game::data::TweakDB*` / `red::*`. So every `RED4ext::`-prefixed candidate is guaranteed to miss.
  2. **The real accessor is not an exported dynamic symbol anyway.** The binary is NOT export-stripped (67,928 exports — see F-006), yet none of its 136 exported `Tweak*` functions is a TweakDB singleton getter / `GetFlat` / `SetFlat`. Those are internal/inlined and have no name reachable by `dlsym`. (Consistent with FA-004: `GetFlat`/`SetFlat` are likely non-virtual/inlined.)
- **What to do instead:** Do not use `dlsym` to locate TweakDB internals. Locate the function statically (Ghidra static address) and convert to runtime with the slide formula in F-004 (`runtime = static + slide`). Since Ghidra's auto-analysis did not name the accessor either (F-006), it must first be located by xref analysis — see H-005.
