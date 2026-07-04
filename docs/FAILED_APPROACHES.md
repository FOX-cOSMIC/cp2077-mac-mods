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
- **SCOPE (2026-06-10):** this immutability holds **only for the stock, non-re-signed binary** under Hardened Runtime's executable-page protection. It is *not* an absolute macOS limit: re-signing the game ad-hoc with the Hardened Runtime entitlement **`com.apple.security.cs.disable-executable-page-protection`** lifts it and makes inline `__TEXT` patching viable. That entitlement is the most security-reducing one Apple ships (discouraged) and breaks the original signature, so the `__DATA`/GOT/`MAP_JIT` routes remain preferred; inline `__TEXT` patching is a re-sign-gated **Phase 5 fallback** (see `docs/ROADMAP.md` Phase 5 + `docs/reference/macos-codesigning.md`). Does not change v1.0–v1.x, which never patch `__TEXT`.
- **⚠ CORRECTED 2026-07-04 by F-053:** the 2026-06-10 scope note above was never empirically tested and is now known to be **wrong**. `h004_probe.cpp` tested `disable-executable-page-protection` (plus `allow-unsigned-executable-memory`/`allow-jit`, plus `get-task-allow`) via ad-hoc re-signing against 4 real copies of the game binary: `mprotect`/`mach_vm_protect` on `__TEXT` failed identically to the stock binary in every configuration (KERN_PROTECTION_FAILURE / EPERM). **Ad-hoc re-signing with any tested entitlement combination does NOT lift `__TEXT` immutability on this build.** The `__DATA`/GOT/`MAP_JIT` routes are therefore not merely "preferred" but currently the **only known working path** — inline `__TEXT` patching is not simply "re-sign-gated," it is unresolved pending either a real (non-ad-hoc) Apple Developer ID signature (untested, uncertain per `memaxo/RED4ext-macos`'s own analysis) or a different technique entirely. See **F-053** for full methodology (6 tested configurations) and the incidental discovery that preserving a publisher's *restricted* provisioned entitlements under ad-hoc re-signing causes an instant SIGKILL, independent of the `__TEXT` question.

---

### FA-002: Offline binary patching of `tweakdb.bin`

- **Date:** 2025-10-XX
- **Tried by:** prior Claude sessions (project `cp2077-tweakxl-mac` at `$HOME/cp2077-tweakxl-mac/`)
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

---

### FA-013: Interning-safe flat edit (`EditScalarFlatSafe`) to produce a VISIBLE gameplay change

- **Date:** 2026-05-31 … 2026-06-03
- **Tried by:** this session (HP/RAM, then prices)
- **Source:** F-036, F-043; session-log 2026-06-03.
- **Approach:** the interning-safe writer allocates a NEW FlatValue in the +0x148 buffer and repoints ONLY the target flat's +0x40 entry `tdbOffset` to it (so no other flat is corrupted), then `UpdateRecord`. Used to set BaseStats values, then `Price.*.value`, expecting the change to appear in-game.
- **Why it failed:** the repoint is **GetFlat-visible but gameplay-INVISIBLE**. The game's own `GetFlat` (`FUN_102b76708`) re-walks the +0x40 array and returns the new value (so every "safe" edit verified correctly) — but the gameplay/record consumers hold a **pointer to the FlatValue at its ORIGINAL buffer location**, captured when records were materialized at TweakDB-load. Moving the value to a new location and repointing the lookup table does not change what those captured pointers read. Net: ground-truth `GetFlat` says the edit landed, yet HP/RAM/prices never moved. (F-043.)
- **What to do instead:** to reach the captured consumers you must write **in place** at the original FlatValue location (`flatDataBuffer + tdbOffset + 0x08`), NOT repoint — BUT only on a flat whose FlatValue is **unique** (FA-014). The interning-safe path is correct only as a *targeting check* (does the right flat resolve?), never as a way to manifest a visible effect. Better still, write the structure gameplay actually reads (materialized record / live entity), not the flat.

---

### FA-014: Blanket in-place scalar edits across many flats (price ×10, HP/RAM ×N "shotgun")

- **Date:** 2026-06-03
- **Tried by:** this session (`MultiplyPrices`, `BoostStatFlats` in-place)
- **Source:** F-043; crash reports `Cyberpunk2077-2026-06-03-144927.ips` (OOM) and `…-151514.ips` (bad-access).
- **Approach:** `EditScalarFlatInPlace` (overwrite bytes at the original FlatValue location — the path that DOES reach gameplay) applied in bulk: ×10 all 232 `Price.*.value`, and ×10 a set of HP/RAM stat flats.
- **Why it failed:** **value interning is heavy — 3,306,462 flats collapse onto only 195,614 distinct FlatValues (~17:1)** (measured live). One in-place write therefore mutates EVERY flat that pooled that value. Editing shared values corrupts unrelated flats; when a corrupted value is consumed as an allocation size/count or a pointer/index, the game traps:
  - boost run → `EXC_BREAKPOINT` / `BRK #1` = the engine's **allocator OOM/pool-exhaustion** handler (`FUN_100024298`), triggered by ×10-ing `BaseStatPools.*.initialValue` (a percentage the engine consumes → absurd derived size).
  - price run → `EXC_BAD_ACCESS` / `KERN_PROTECTION_FAILURE` at a wild pointer = a ×10'd shared price magnitude that some flat used as a pointer/offset.
- **What to do instead:** never blanket-edit in place. Edit a **single** flat whose FlatValue is **unique** (build a `tdbOffset → share-count` map over the +0x40 array, require count==1) — zero collateral, no crash (`MultiplyPricesUnique` did this for 7 unique-valued prices without crashing). Use sane multipliers, small batches, and NEVER touch sentinels/percentages (FA-015).

---

### FA-015: Editing `BaseStats.*.max` / `BaseStatPools.*.initialValue` to change max HP/RAM

- **Date:** 2026-05-31 … 2026-06-03
- **Tried by:** this session
- **Source:** F-036, F-039, F-043.
- **Approach:** treat `BaseStats.Health.max` / `BaseStats.Memory.max` and `BaseStatPools.Player_{Health,Memory}_Base.initialValue` as the player's max-HP / max-RAM magnitude and edit them.
- **Why it failed:** those are the WRONG fields. `BaseStats.*.max` reads **`-1e7` (an internal range sentinel, not the player's max)** — editing it is inert (F-036). `BaseStatPools.*.initialValue` is a **starting PERCENTAGE (base 100)**, not a magnitude — and ×10-ing it fed an absurd value into the stat-pool init computation and **crashed the allocator** (FA-014 / F-043). The player's max HP/RAM are **computed per-entity** by the seeder `FUN_103a99a60` folding a modifier list (base + additives × multipliers), so there is NO single stored max-HP/RAM Float to overwrite. (F-039.)
- **What to do instead:** do not edit max-stat sentinels or pool percentages. The only TweakDB lever for max HP/RAM is to add a `ConstantStatModifier` (statType=Health/Memory) to the player's stat-modifier group — but see FA-016 for why even that did not manifest.

---

### FA-016: Create+append a `ConstantStatModifier` to change the live player's HP/RAM (the DoubleRam.yaml recipe)

- **Date:** 2026-06-03 … 2026-06-04
- **Tried by:** this session (`TestCreateRecordAppend`, `ApplyLiveStatBoosts`)
- **Source:** F-041, F-042, F-043; session-log 2026-06-03/06-04. Recipe = the official Nexus mods DoubleRam.yaml / DoubleRamRegen.yaml.
- **Approach:** replicate the real Windows mod exactly — CREATE a new `ConstantStatModifier` (statType=BaseStats.Memory/Health, modifierType=AdditiveMultiplier, value) via the game's own `CreateTDBRecord`, then `!append-once` its id onto `Character.Player_Stat_Pools_Modifier.statModifiers`, then `UpdateRecord`. Tried RAM cap +50/100%, HP cap +50/100%, and RAM regen +25.
- **Why it failed:** **no visible effect — on an existing save OR a brand-new game.** The CREATE+APPEND itself is correct and verified (`GetRecord` resolves the new record; the array grows 121→124 — F-041/F-042), but the player's stat computation never reflects it. Root (best current understanding, F-043): entity stat-init / the seeder reads **materialized record instances and/or DynArray `data` pointers snapshotted at TweakDB-load, BEFORE our apply-trigger runs** — and our flat-level append + `UpdateRecord` do not propagate into those captured structures (same captured-pointer mechanism as FA-013). On an existing save the player's pools are additionally baked at character-setup and cached in the live StatsContainer (F-030/F-038). The new-game test was expected to re-seed fresh and DID NOT change HP/RAM either — so either `UpdateRecord` fails to refresh the materialized `statModifiers` the seeder reads, or `Character.Player_Stat_Pools_Modifier` is not the group macOS player stat-init actually consumes.
- **What to do instead:** stop trying to reach computed player stats through the flat/record layer. Two unblocked-but-hard routes: (A) write the **live `StatsContainer`** directly (the structure gameplay reads) — requires solving the F-038/FA-012 gameInstance-root problem to reach the player entity; (B) RE the seeder `FUN_103a99a60` to determine exactly which materialized structure it reads for the player's modifier list and whether `UpdateRecord` must also rewrite the materialized record's `statModifiers` DynArray in place. Until one is solved, TweakDB edits are verified at the engine's accessor level but do NOT manifest as visible player-stat changes on macOS. **[SUPERSEDED for the project goal by F-044: a direct live-memory write — not a TweakDB edit — produced the first visible change (money). Route A's structural heap-scan idea is essentially what F-044 used, without needing the gameInstance root.]**

---

### FA-017: Write the Memory **StatPool max** (`poolRecord+0x1d4`) to raise the displayed RAM cap

- **Date:** 2026-06-10
- **Tried by:** this session (`DoStatPoolMax`, `TWEAKXL_POKE_POOLMAX=1`)
- **Source:** RE workflow over `Cyberpunk2077.c` — GetStatPoolValue `FUN_103a58d78` @12403850, record lookup `FUN_103a58b90` @12403749. Verified pool-record offsets: `+0x1cf` validity byte `0xFF`, `+0x1d0` current-as-percent float [0,100], `+0x1d4` absolute-max float; displayed pool value = `(pct/100)*max`.
- **Approach:** structurally locate the Memory StatPool record (signature `[0xFF@+0x1cf, pct∈[0,100]@+0x1d0, max==32@+0x1d4, plausible ptr@+0x20]`) and `mach_vm_write` a new max (`64.0`) into `+0x1d4`. Matched 1–3 records cleanly, readback `=64`, no crash.
- **Why it failed:** **the displayed cyberdeck RAM cap does not read the StatPool max.** The cap stayed `/32`; and every `max==32` record matched had `pct==0.00`, i.e. not what the HUD shows. The cap is `GetStatValue(Memory)` from the **StatsContainer**, not `GetStatPoolMaxPointValue`. Worse, the pool max is **re-derived from the Memory stat every regen tick** (`FUN_103a7c5cc` fetches it via vtable+0x208), so the `+0x1d4` write is overwritten anyway — it is a downstream copy, not the source the player sees.
- **What to do instead:** target the source the HUD reads, or — since that value isn't materialized as its displayed integer (FA-019) — use a **stored counter** (F-044) instead of a derived pool.

---

### FA-018: Write the **Memory stat by raw id `0x1bc`** in the player StatsContainer

- **Date:** 2026-06-10
- **Tried by:** this session (`DoStatPoke`, `TWEAKXL_POKE_STATID=0x1bc`)
- **Source:** RE decision — cyberdeck RAM cap = `GetStatValue(Memory)` = `FUN_103a7b8e8(container, 0x1bc)` → `entArr+slot*0x10+0x0c`; `0x1bc` is the id the recompute `FUN_103a7d4bc` @12430610 passes for Memory.
- **Approach:** find the player StatsContainer (structural scan), linear-search `idArr` for id `0x1bc`, write `64.0` into that cell + re-apply.
- **Why it failed:** **`0x1bc` is not in the player StatsContainer's `idArr`** — the lookup aborts (`statId 0x1bc not in container`). The container keys live entries by **hashed runtime ids** (observed: `0x3700`, `0xb000b01`, `0x81920800`, `0xdf527d52`), not the raw enum `0x1bc`; the `0x1bc` the decompile uses belongs to a different internal container/getter. The StatsContainer's runtime-id hashing is not statically recoverable (same unknown as F-038's name-hash stat ids).
- **What to do instead:** don't look up the displayed stat by raw enum id. Either delta-isolate the cell by value-change, or — since the displayed value isn't materialized at all (FA-019) — switch to a stored counter (F-044).

---

### FA-019: **Value-scan / marker-probe** the StatsContainer for the displayed RAM cap (32) or max health (322)

- **Date:** 2026-06-10
- **Tried by:** this session (`DoStatProbeCells` container/float mode + `DoStatUnequipDelta`, `TWEAKXL_POKE_PROBE_CELLS=1` / `TWEAKXL_POKE_UNEQUIP=1`)
- **Source:** the cap-source RE concluded the displayed cap is a StatsContainer float cell, so locate it by its on-screen value.
- **Approach:** (a) marker-probe — find every player-scale container cell holding the displayed value, write a distinct marker (`base+i`) to each, read the on-screen number to identify the cell; (b) unequip-delta — snapshot cells holding `32`, have the user unequip the cyberdeck expecting `32→23`, keep cells that became `23`; (c) `±0.6` tolerance to catch a fractional float that rounds to the integer.
- **Why it failed:** **derived/displayed stats are NOT materialized as their displayed value.** RAM cap `32`: 22 player cells held `32`; marker-writing all left the cap at `/32` — none drives it (computed from base+cyberdeck at read, and/or cached in a UI widget that only refreshes on a gear event which itself recomputes). Max health `322`: **zero** cells held `322` (even ±0.6) despite an 8188-stat player container — the displayed total is never stored as a single float. Unequip-delta found **0** cells going `32→23` because removing the cyberdeck doesn't deterministically yield `23` (user swaps to another device / RAM becomes unusable), so there is no fixed `B`. (Two process bugs also surfaced and were fixed: a fixed-delay scan fired on **menu** data before the save loaded — a count≈7224 container exists at the main menu — fixed by a user-driven go-signal; loose container matching produced **misaligned** false-positive cells — fixed by requiring 8-byte-aligned `idArr`/`entArr`.)
- **What to do instead:** stop chasing derived/displayed stats by value — they are computed, not stored. Target a **stored counter** that exists verbatim in memory (money/eddies — F-044, components, XP), or isolate a derived stat by a *deterministic* value-change delta (not the cyberdeck, whose removal is non-deterministic).

---

### FA-020: Staged a third-party mod (`NightCityAlive`) in `/tmp` to unblock a redscript compile test, instead of a durable path

- **Date:** 2026-06-10 (staged) → discovered lost 2026-07-02
- **Tried by:** this session, while verifying the redscript pipeline (F-046/F-047)
- **Approach:** `NightCityAlive` had a syntax error incompatible with this `scc` version and aborted the whole `r6/scripts` compile (all-or-nothing). To get a clean compile for the test, it was moved aside with `mv r6/scripts/NightCityAlive /tmp/NightCityAlive.disabled`, intending to restore it after the test.
- **Why it failed:** `/tmp` is not durable — it was cleared (most likely a reboot) between sessions. When the runtime-verification work resumed, `NightCityAlive` was gone from both `/tmp` and `r6/scripts/`, and no copy was found anywhere else on disk (`mdfind` came up empty). The mod is presumed lost; the user will need to re-download it (e.g. from Nexus Mods) if they want it reinstated.
- **What to do instead:** when temporarily relocating a **user's own files** (mods, saves, configs — anything not reproducible from this repo) to unblock a test, use a durable location inside the project or the user's home directory (e.g. `~/cp2077-mac-mods-backups/` or a git-ignored folder under the repo), never `/tmp`. `/tmp` is fine for our own generated/reproducible artifacts (logs, launcher scripts, scratch builds) but never for the only copy of something the user owns.
