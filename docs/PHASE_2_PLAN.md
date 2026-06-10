# PHASE_2_PLAN.md — Live-Edit Tool (productize F-044)

**Status:** planned (not started) · **Owner:** `lead` (impl) + `tester` (in-game validation) · **Depends on:** F-044, FA-017/18/19, D-007

Phase 2 turns the working live-memory-write probe (`DoStatProbeCells`, which produced F-044) into a small,
safe, *usable* native macOS **live-edit tool** for **stored counters**. It is the first shippable
capability and the empirical groundwork for Phase 4 (writing materialized structures). It is **not**
Windows-mod-file parity — see `ROADMAP.md`.

---

## What prior work settled (load-bearing)

| Fact / Approach | Why it shapes Phase 2 |
|---|---|
| F-044 | A single `mach_vm_write` to a stored counter changes the value in-game, no reload, no crash, revertable. The mechanism to productize. |
| FA-017/18/19 | Derived/displayed stats (RAM cap, max HP) are computed, never materialized → **out of scope** for Phase 2. Targets must be **stored counters** (money, components, attribute/perk points, XP). |
| FA-014 | Blanket scattershot writes crash. **Single, structurally-validated writes only.** |
| F-038 | No `gameInstance` root → we locate targets by value/structure scan, not by an object graph. Fine for stored counters. |

---

## Already built — reuse, do not rewrite

All in `src/red4ext-mac/src/runtime/TweakDB.cpp` unless noted:

- `DoStatProbeCells()` — the go-signal loop (wait → scan → mark → revert). The skeleton Phase 2 extends.
- `ProbeScanAndMark(value, intRaw, tol, base, maxCells)` — raw-int + float-tolerance scan + distinct-marker
  write. Phase 2 adds a **set-to-exact-value** mode beside the marker mode.
- `RevertMarkers()` / `StartMarkerReapply()` / `g_markers` (`MarkerRec{addr, orig, marker}`) — the
  revert-to-original and hold-against-recompute machinery (records originals → restorable).
- `SafeWrite` / `SafeReadU32` / `SafeRead` / `PlausiblePtr` / `CollectScannableRegions` — safe primitives.
- Go-file control channel (`TWEAKXL_POKE_GO_FILE`, e.g. `/tmp/red4ext_go`): user-driven trigger + value;
  mode prefix (`i` = raw-int counter, plain = float). Launchers `/tmp/run_probecells.sh`.

---

## Tasks

### P2.1 — Set-to-value mode (beside marker mode)
Today the loop writes distinct markers (`base+i`) to identify a cell. Add a mode that, given a value the
user has already identified, **writes one chosen value** to the confirmed cell(s) and holds it.
- Go-file grammar: extend to `set <oldValue> <newValue>` (and keep `i`/float scan + `0`=revert).
- Behaviour: scan for `<oldValue>` (raw-int or float+tol), write `<newValue>` to matches, record originals
  in `g_markers`, hold via the existing re-apply thread.
- Acceptance: signal `set i310915 1000000` in-world → money reads 1,000,000; `0` restores 310,915.

### P2.2 — Named stored-counter targets
A small registry mapping friendly names → how to find/edit the counter, so the user signals `money=1000000`
instead of remembering raw values.
- Start with **money/eddies** (proven). Add **crafting components**, **attribute/perk points**, **Street
  Cred / XP** — each validated in-game by the F-044 marker method first (find by current value), then
  recorded with its disambiguation recipe.
- For counters with several mirror addresses (money had ~10), document which is authoritative and write all
  safe mirrors (valid in-range values can't crash).
- Acceptance: a `targets` table (name → scan recipe) with ≥2 validated entries beyond money.

### P2.3 — Safety + validation hardening
- **Never write unvalidated/scattershot.** Keep the 8-byte-alignment + structural gates; for raw-int
  counters, require the candidate set to be small (cap + log when capped — no silent truncation).
- **Always revertable:** every write records its original; a single `revert` restores all and idles.
- **Range sanity:** refuse absurd values that could feed allocation sizes (the FA-014 lesson) — clamp to a
  sane per-target max, logged.
- **Fail safe on unknown build:** gate the whole tool behind a binary-version/offset check (see P2.5).
- Acceptance: a deliberate bad signal (out-of-range, no match, freed cell) degrades to a logged no-op, never
  a crash.

### P2.4 — Control channel + UX cleanup
- Keep the go-file channel (works headless) but document a clean command vocabulary
  (`scan`, `set`, `<name>=<value>`, `revert`, `status`).
- One launcher script (replace the ad-hoc `/tmp/run_*.sh`) with inline help; logs to
  `/tmp/red4ext-mac.log` with a stable `[live-edit]` tag.
- Acceptance: a new user can run the launcher + a 3-line cheat-sheet and set money without reading code.

### P2.5 — Version/offset gate (cross-cutting, starts here)
- On load, verify the running binary matches the build the offsets were validated against (hash or version
  string). If not: **disable writes**, log loudly. Protects against a game patch silently moving
  `0x1080c92d0` or function addresses.
- Acceptance: launching against a mismatched build logs a clear refusal and performs no writes.

### P2.6 — Tests + user guide
- Unit-test the new value-parsing / target-registry logic deterministically (no game), in the existing
  test style (`src/.../*_test.cpp`, wired in CMake).
- Write `docs/guides/live-edit.md` (or a `BUILD.md` section): build, launch, in-world signal a value,
  revert, safety notes (RAM-only, reload restores).
- Acceptance: `ctest` green; a tester can follow the guide end-to-end and report success in `session-log.md`.

---

## Order of operations

1. **P2.1 set-to-value** (smallest extension of proven code) → validate money set/restore in-game.
2. **P2.5 version gate** early (cheap, protects everything after).
3. **P2.2 named targets** — validate components / points / XP one at a time (one investigation each, per
   HANDOFF pitfalls).
4. **P2.3 safety hardening** alongside P2.2 as edge cases surface.
5. **P2.4 UX** + **P2.6 tests/guide** to close out.

Each step ends with a `tester` confirming the actual in-game behaviour (compile-clean is not enough).

---

## What "done" looks like (Phase 2 acceptance)

- [ ] Set + restore **money** to a chosen value, in-game, no reload, no crash.
- [ ] ≥2 more **named** stored counters (components / points / XP) set + restored.
- [ ] Every write is revertable; `revert` restores all originals.
- [ ] Tool refuses to write on an unrecognised game build.
- [ ] `ctest` green for the new logic; `docs/guides/live-edit.md` lets a fresh user do it.
- [ ] No save-file mutation; all effects RAM-only and reload-reversible.

---

## Explicitly NOT in Phase 2

- Derived/displayed stats (RAM cap, max HP) — not materialized (FA-017/18/19). Out of scope.
- TweakDB `.yaml` mod propagation — that's **Phase 4** (`ROADMAP.md`).
- Any `__TEXT`/code hooking, ArchiveXL, redscript, CET — Phases 5–6.
- A GUI mod-manager — CLI/headless only for now (consistent with v1.0 scope, D-006).

---

## Open questions (resolve before/within the phase)

- Are money's mirror addresses stable across sessions, or must each be re-scanned? (F-044 saw the count
  vary 10 vs 3 across launches.)
- Do components / points / XP each have one authoritative store, or several? (Validate per target.)
- Is there a recompute that fights the write for any target (as the StatPool max did, FA-017)? If so, the
  re-apply thread holds it — confirm cadence per target.
