# ROADMAP.md — Strategic Multi-Phase Plan

**Owner:** `lead` writes the strategic narrative; `doc-keeper` keeps it consistent with `state/status.yaml`, `FACTS.md`, and `FAILED_APPROACHES.md`.

This is the big-picture sequencing doc: *how* we get from "we can write live game memory" to "Windows
Cyberpunk 2077 mods run on macOS." It complements — does not replace — the canonical version matrix in
`ARCHITECTURE.md` ("Long-Term Roadmap") and `state/status.yaml:version_roadmap`, and the task-level phase
specs (`PHASE_1_PLAN.md`, `PHASE_2_PLAN.md`). Sequencing rationale: **D-007** (and the scope slicing in
**D-006**).

---

## The ambition

Every gameplay-changing mod that works on Windows works on macOS, unchanged. That spans four mod families,
each served by a different Windows tool and each needing a different macOS capability:

| Mod family | Windows tool | What it edits | macOS capability needed |
|---|---|---|---|
| Gameplay data (stats, prices, items, loot) | TweakXL (`.yaml`/`.tweak`) | TweakDB (in-memory data) | data write + **propagation to the structures gameplay reads** |
| Assets (textures, meshes, sounds, new items) | ArchiveXL / REDmod | `.archive` resources | native archive load (replacement) + **resource-loader hooks** (extensions) |
| Logic (behaviour changes) | redscript | the script blob | **script-loader hook** + redscript compile |
| Live scripting / overlay | CET (Cyber Engine Tweaks) | runtime, via Lua + ImGui | Lua VM + **ImGui-on-Metal + render/input hooks** |

Almost everything advanced is gated on one missing primitive: **`__DATA` function interception on Apple
Silicon** (inline `__TEXT` hooking is blocked — FA-001). So the roadmap is shaped around (a) shipping
what's reachable *without* hooks first, and (b) building the hook primitive as the central unlock.

---

## Where we are now (2026-06-10)

- **Proven (F-044):** the **live-memory-write path reaches gameplay.** A direct `mach_vm_write` changed the
  player's eddies 310,915 → 400 on an existing save — no reload, no crash, cleanly revertable. This is the
  first visible, verified, native-macOS in-game change.
- **Proven (F-036/F-041/F-042):** the full TweakDB **write / create / append** stack works and verifies at
  the game's own accessors (`GetFlat`/`GetRecord`).
- **Walled (F-043/FA-016):** TweakDB edits do **not** propagate to live gameplay — per-entity structures
  (StatsContainer, materialized record instances) are snapshotted before our apply-trigger and never
  refreshed by a flat edit. The apply-trigger fires *after* DB build and **cannot fire earlier** (FA-001 /
  F-016 / F-017). So the wall is downstream materialization, not DB-build timing.
- **Learned (FA-017/18/19):** derived/displayed stats (RAM cap 32, max HP 322) are computed from
  base+modifiers and **never materialized** as their displayed value → unreachable by value-scan. **Stored
  counters** (money, components, XP) exist verbatim in memory → reachable and writable. This is *why*
  Phase 2 targets stored counters.

---

## Phases

Sequenced per **D-007**: ship the proven capability first, then the cheap-but-unvalidated win, then the hard
data-propagation RE, then the hook primitive, then scripting. Each phase is independently valuable.

### Phase 0 — Foundation research ✅ (closed 2026-05-29)
TweakDB layout, slide/singleton access, apply-trigger mechanism, codesigning/injection. See
`status.yaml:phase_0_summary`, F-001…F-019.

### Phase 1 — v1.0 TweakXL engineering ✅ ~85% (in progress)
Inject → slide → singleton → apply-trigger → flat resolve → edit → `UpdateRecord`, all proven and
`GetFlat`-verified. YAML→parse→apply wired end-to-end for scalar Assign. Detail: `PHASE_1_PLAN.md`.
*Open tail:* the propagation wall (moved to Phase 4) and the deferred apply surface (`.tweak` P1.11b,
non-scalar/array ops P1.10b).

### Phase 2 — Live-Edit Tool 🎯 (NEXT — productize F-044)
Turn the `DoStatProbeCells` probe into a usable, safe, native macOS **live-edit tool** for **stored
counters** (eddies, crafting components, attribute/perk points, Street Cred / XP).
- **Capabilities:** named targets; **set-to-value** (not just markers); **revert/restore originals**;
  hold-against-recompute; structural validation before every write; clear logging.
- **Needs:** only what's already built — `mach_vm_read_overwrite` / `mach_vm_write`, the scan + marker +
  revert machinery in `src/red4ext-mac/src/runtime/TweakDB.cpp`.
- **Done looks like:** the user can set and restore a handful of *named* stored values reliably, no crash,
  no save corruption; a short user guide exists.
- **Honest framing:** this is a *native live-edit tool* (trainer-class), **not** Windows-mod-file parity.
  It's a real, demonstrable deliverable and the empirical foundation for Phase 4's materialized-structure
  writes. Detail: `PHASE_2_PLAN.md`.

### Phase 3 — Textures / Assets (`.archive` mods) — ⚠ NOT a free win (F-045)
**Tested 2026-06-10 — native drop-in archive loading is NOT wired on macOS (F-045).** The game eagerly
mmaps its *entire* base archive set from `archive/Mac/` at startup but **never scans any mod folder**
(`archive/pc/mod`, `archive/Mac/mod`, `/mods/`) — confirmed by both `lsof` and `fs_usage`. The pre-installed
`archive/pc/mod/bike_fix_nca.archive` is a no-op Windows-path leftover.
- **What it now needs:** a **loader/hook that registers a mod archive group** with the game's resource
  depot — i.e. this depends on **Phase 5 (hooking)**. Pure replacement archives are *not* free here.
- **Sequencing:** Phase 3 therefore moves **behind Phase 5**, or merges into it (hook the archive-group
  init / resource-depot scan to add a mod path).
- **Done looks like:** a hook registers a mod `.archive`; a known replacement visibly changes the game;
  install steps documented.

### Phase 4 — TweakDB → gameplay parity (the hard RE)
Make `.yaml` gameplay mods actually take effect: refresh the **materialized StatsContainer / record
instances** the entity seeder reads (FA-016 "route B"), reusing Phase 2's live-write techniques (write the
materialized structure directly, post-apply). Also finish the deferred TweakXL surface (`.tweak` apply,
non-scalar/array ops) so the parser→applicator is full-featured wherever it *does* reach.
- **Done looks like:** a stock Windows TweakXL `.yaml` (e.g. a price or stat mod) is visibly in effect.
- *Hard and RE-heavy; this is the original v1.0 promise's last mile.*

### Phase 5 — Hooking foundation (Apple Silicon) — the central unlock
Build the `__DATA` GOT / vtable / fn-ptr interception primitive (D-002) — these target writable data, so
they need **no re-sign** (the stock binary's entitlements suffice). For anything requiring executable
memory, the lever is a **re-sign with Hardened Runtime entitlements** (the game must be re-signed ad-hoc,
which the codesigning reference already documents):
- **`allow-jit` + `allow-unsigned-executable-memory`** → `MAP_JIT` trampoline pages, for original-call
  preservation / new code stubs (the GOT+trampoline route).
- **`com.apple.security.cs.disable-executable-page-protection`** → lifts the `__TEXT`-immutability wall and
  enables **true inline hooking** (patching existing instructions in place). This is the entitlement that
  scopes **FA-001**: FA-001's "`__TEXT` is immutable, inline hooking fails" holds *only for the stock,
  non-re-signed binary*; a re-sign carrying this entitlement makes inline patching viable. It is the most
  security-reducing entitlement Apple ships (explicitly discouraged) and breaks the original signature, so
  prefer the GOT/`MAP_JIT` route first and treat inline-`__TEXT` patching as the fallback when a direct
  call can't be intercepted indirectly.
- **Tradeoff / cost:** re-signing adds user friction we currently avoid (Phases 2–4 need no re-sign), and is
  build-specific — a game patch can strip entitlements (the P2.5 version gate + codesigning stale-signature
  check catch this). See `docs/reference/macos-codesigning.md` and D-002.
- **Done looks like:** a reusable hook intercepts a chosen game call (e.g. a resource lookup); unblocks
  ArchiveXL-extensions, redscript, Codeware.
- **Source:** Apple Hardened Runtime (https://developer.apple.com/documentation/security/hardened-runtime).

### Phase 6 — Scripting (redscript → CET) — compile-side present, runtime NOT wired (F-046/F-047)
- **redscript compiles cleanly on macOS (F-046)** — `engine/tools/scc` + `libscc_lib.dylib` is a native
  Apple Silicon build; `launch_modded.sh` runs `scc -compile r6/scripts` before launch and produces
  `r6/cache/final.redscripts` (confirmed by `r6/logs/redscript_*.log` and matching file mtimes).
- **⚠ Runtime does NOT load it (F-047, tested 2026-07-02).** With a clean compile confirmed, `lsof`
  (both single-shot and rapid-polled across the full startup → main-menu → save-load window, two separate
  launches) shows the game **never opens** `r6/cache/final.redscripts`, `r6/scripts`, or anything
  redscript-related. The on-screen message from a trivial `@wrapMethod` test mod never appeared in-game.
  Notably `strings` on the binary *does* contain `RedScriptsHost`/`final.redscripts`/etc. — the feature
  exists in compiled code but is dead, gated, or short-circuited before reaching `open()` on this build.
- **Revised implication:** redscript is **not** a free/cheap win — it needs the same category of fix as
  Phase 3 (F-045): either a hook that drives the existing `RedScriptsHost` code path, or RE to find/lift
  whatever gates it on macOS. It moves alongside Phase 3, likely **behind or bundled with Phase 5**.
- **CET:** Lua VM (portable) + **ImGui-on-Metal overlay + Metal render/input hooks** — the single largest
  piece; still needs Phase 5.
- **Input mods** also work today (InputLoader, F-046) — untouched by this finding.
- **Done looks like:** find/trigger the `RedScriptsHost` load path (RE or hook) so a redscript mod's effect
  is observable in-game; (stretch) a CET overlay renders.

> **Re-sequencing note (2026-07-02, F-045/F-046/F-047 supersedes the 2026-06-10 note):** redscript is
> **not** the cheap win it looked like on 2026-06-10 — the compile-time proof (F-046) did not hold at
> runtime (F-047). Both asset mods (Phase 3) and script mods (Phase 6) now depend on the same missing
> piece: understanding/driving native loader code that exists in the binary (`RedScriptsHost` et al.) but
> isn't reached on macOS. The live-edit tool (Phase 2, done) and the TweakDB→gameplay RE (Phase 4) remain
> the only capabilities reachable **without** further hooking/RE work; Phase 3, Phase 6, ArchiveXL
> extensions, and CET all now cluster around Phase 5 (or a lighter-weight "find the macOS gate" RE task
> that may precede full inline-hooking).

---

## "Well and right" — principles for every phase

- **Save-safety first.** Writes are RAM-only and revertable; never mutate save files; everything is
  restorable by reload. Phase 2 keeps originals and offers explicit revert.
- **No-crash discipline.** Single, structurally-validated writes (the F-044 lesson). Never blanket
  scattershot — that crashed the game 3× (FA-014).
- **Offset maintenance / version gate.** Every game patch can move static addresses (singleton at
  `0x1080c92d0`, function addresses). Ship a re-verification harness and a binary-version check that
  **fails safe** on an unrecognised build (HANDOFF "Common Pitfalls": game updates move things).
- **Honest docs.** Every dead end records *why* (FACTS/FAILED_APPROACHES discipline); `status.yaml` stays
  the single source of truth; one claim per FACTS entry.
- **Validate on the real game.** Compile-clean is not enough — a change is "done" only when observed
  in-game.

---

## Dependency shape

```
Phase 2 (live-edit tool) ──┐  (independent; ships now)
Phase 3 (native archives) ─┤  (independent; needs a validation experiment)
Phase 4 (TweakDB parity) ──┤  (reuses Phase 2 live-write technique)
Phase 5 (hooking) ─────────┴──► unlocks ─► Phase 6 (redscript, CET) and ArchiveXL-extensions
```

Phases 2–4 need **no** new hook primitive. Phase 5 is the gate for everything in Phase 6 and for ArchiveXL
beyond plain replacement.
