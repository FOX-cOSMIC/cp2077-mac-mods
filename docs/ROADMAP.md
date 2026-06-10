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

### Phase 3 — Textures / Assets (native `.archive` replacement mods)
The macOS install already has `archive/Mac/content/*.archive` (loaded natively) **and**
`archive/pc/mod/bike_fix_nca.archive` present — i.e. the standard mod path exists. So *replacement* mods
(texture/mesh/sound) may load with **zero new code**.
- **Step 1 (validation experiment, not yet run):** drop a known replacement `.archive` into
  `archive/pc/mod/` and confirm it shows in-game.
- **If it loads:** a huge Windows-mod category works essentially unchanged → write the install guide;
  package it. ArchiveXL *extensions* (variants/appearances, dynamic resource paths) need Phase 5.
- **Done looks like:** a known replacement archive visibly changes the game; install steps documented.
- *Highest payoff/effort on the board; deferred behind Phase 2 by the sequencing choice.*

### Phase 4 — TweakDB → gameplay parity (the hard RE)
Make `.yaml` gameplay mods actually take effect: refresh the **materialized StatsContainer / record
instances** the entity seeder reads (FA-016 "route B"), reusing Phase 2's live-write techniques (write the
materialized structure directly, post-apply). Also finish the deferred TweakXL surface (`.tweak` apply,
non-scalar/array ops) so the parser→applicator is full-featured wherever it *does* reach.
- **Done looks like:** a stock Windows TweakXL `.yaml` (e.g. a price or stat mod) is visibly in effect.
- *Hard and RE-heavy; this is the original v1.0 promise's last mile.*

### Phase 5 — Hooking foundation (Apple Silicon) — the central unlock
Build the `__DATA` GOT / vtable / fn-ptr interception primitive (D-002). Trampolines / `MAP_JIT` gated on
re-signing for `allow-jit`.
- **Done looks like:** a reusable hook intercepts a chosen game call (e.g. a resource lookup); unblocks
  ArchiveXL-extensions, redscript, Codeware.

### Phase 6 — Scripting (redscript → CET) — v2.0
- **redscript:** hook the script loader (needs Phase 5), compile `.reds` from `r6/scripts/` into the
  game's bytecode at load.
- **CET:** Lua VM (portable) + **ImGui-on-Metal overlay + Metal render/input hooks** — the single largest
  piece.
- **Done looks like:** a redscript mod runs; (stretch) a CET overlay renders.

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
