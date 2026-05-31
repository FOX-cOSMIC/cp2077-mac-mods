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

---

### FA-010: Assuming the macOS TweakDB struct is 0x198 bytes (differs from Windows's 0x168)

- **Date:** 2026-05-29
- **Tried by:** Scope (researcher), T-002c
- **Source:** Disproves hypothesis H-002 (carried from old session 20); resolved by F-013.
- **Approach:** Old session-20 work concluded the macOS TweakDB struct was `0x198` bytes — larger than the Windows `0x168` — because Windows field offsets (e.g. `0x148` for flatDataBuffer) appeared not to match the macOS layout.
- **Why it failed (it's false):** The TweakDB singleton initializer `FUN_102b73b50` (@ static `0x102b73b50`) allocates the instance with `mov w0,#0x168` immediately before `bl red::memory::Pool…Allocate`, then `bzero`s the `0x168`-byte block, constructs it (`FUN_102b73db8`), and stores it into the global `0x1080c92d0`. The struct is **`0x168` bytes — identical to the Windows size.** The records hash-map fields (`+0x88…+0xa4`, F-012) all fit within `0x168`. The old "0x198" was wrong (likely a misread allocation site or a different object on an older build).
- **What to do instead:** Use struct size `0x168` (F-013). The Windows-vs-macOS divergence is NOT in the struct size — it is in the *internal field layout / storage semantics* (macOS records storage is a hash table per FA-003/F-012). Do not re-derive struct size; cite F-013. Re-validate only if the binary SHA256 (F-001) changes.

---

### FA-011: Theory that TweakDB storage is a function-pointer dispatch table (H-001)

- **Date:** 2026-05-29
- **Tried by:** Scope (researcher), T-002d
- **Source:** Disproves hypothesis H-001 (carried from old session 62e, dated 2025-12-14); resolved by F-017.
- **Approach (the disproved theory):** Old session 62 dumped a region it called `staticFlatDataBuffer` and saw values that looked like code pointers (e.g. `0x102f53b04`), concluding TweakDB flats are accessed via a function-pointer dispatch table — and that hooking those table entries was the way to intercept flat reads/writes.
- **Why it failed (it's false):** The TweakDB constructor `FUN_102b73db8` (@ static `0x102b73db8`) writes **no function pointers** into the `0x168` struct. `this+0x00` is zeroed (no vtable — F-016). Flats, records, and queries are stored in three structurally-identical **plain hash tables** (bucket-index array of u32 with `0xffffffff` empty sentinel + entries array; +0x58/+0x88/+0x108, F-015), keyed by an FNV-1a hash of the TweakDBID and looked up with inline arithmetic (F-012) — not via calls through a pointer table. Actual flat *values* live in a data buffer at +0x148 (F-017). The old "code pointers in staticFlatDataBuffer" reading was a misattribution — most likely embedded allocator/sub-object pointers, or a slide-conversion error.
- **What to do instead:** Treat TweakDB as plain data. To read/modify flats, walk the hash table to resolve a TweakDBID to its entry, then read/write the flat-value buffer (+0x148 region). No code hooking, no dispatch-table interception. See F-015/F-017 and hook approach F-014(a). The remaining step is to byte-confirm the flats block (+0x58 vs +0x108) via a `GetFlat` xref (T-002e).

### FA-012: Reaching the player at runtime via a flat engine/gameInstance global (Windows RED4ext chain)

- **Tried (2026-05-31):** replicate `CGameEngine::Get() → +0x308 framework → +0x10 gameInstance → GetSystem(PlayerSystem)` to reach the player's StatsContainer for the self-verifying stat oracle.
- **Why it failed:** on macOS the engine/gameInstance is NOT stored in any flat global (exhaustive Ghidra scan of all 345,127 functions → 0 hits). The compiler carries gameInstance only in context fields. So there is no flat-global root to start the chain from pure memory reads. (F-038.)
- **What to do instead:** to bootstrap gameInstance you need ONE game-system/object instance pointer from a flat global (e.g. pin a system-instance global, or navigate a system pool to its instance) then `system->vtable[+0x80]()`=GetGameInstance and `gameInstance->vtable[+0x10]`=GetSystem (F-038). OR a heuristic heap scan for the StatsContainer signature. `__TEXT` code hooking to capture gameInstance is blocked (FA-001). The GetSystem mechanics + PlayerSystem type (F-038) are correct; only the ROOT is missing.
