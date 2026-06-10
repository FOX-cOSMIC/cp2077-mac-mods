# HANDOFF.md — New Agent Bring-Up

You have just been spawned into this project. You probably have no memory of prior sessions. This document brings you up to speed in the minimum time.

---

## ⮕ Latest state (2026-06-10) — read this first

- **✅ MILESTONE (F-044): first visible, verified, native-macOS in-game change.** A direct live-memory write (`mach_vm_write`) changed the player's eddies **310,915 → 400** on an existing save — no reload, no crash, cleanly reverted. The **live-write path reaches gameplay**, where every TweakDB-flat edit before it verified at the game's `GetFlat` yet stayed invisible (the F-043/FA-016 wall).
- **Why money and not RAM/HP:** displayed/derived stats (RAM cap 32, max HP 322) are computed from base+modifiers and **never materialized** as their displayed value → unreachable by value-scan (FA-017/18/19). **Stored counters** (money, components, XP) exist verbatim in memory → writable. The pool-max (`+0x1d4`) and the `0x1bc` stat-id routes are dead ends (FA-017/FA-018).
- **Direction (D-007):** see `docs/ROADMAP.md`. **Phase 2 (next): productize the live-edit tool** — stored-counter targets (money/components/points/XP), set-value + revert, no-crash validated writes — detail in `docs/PHASE_2_PLAN.md`. Then Phase 3 native-archive textures, Phase 4 TweakDB→gameplay parity, Phase 5 hooking, Phase 6 scripting.
- **Repro F-044:** `/tmp/run_probecells.sh` (`TWEAKXL_POKE_PROBE_CELLS=1 TWEAKXL_POKE_GO_FILE=/tmp/red4ext_go`); in-world `echo i<value> > /tmp/red4ext_go`; read marker `= base+index`; `echo 0` to revert. Live-write code is in `src/red4ext-mac/src/runtime/TweakDB.cpp` (`DoStatProbeCells`/`ProbeScanAndMark`/`RevertMarkers`), env-gated, wired in `Loader.cpp`. **Next action is in `state/status.yaml`.**

---

## Step 1 — Read These Files in Order

1. `AGENTS.md` (repo root) — universal entry, conventions, constraints
2. This file (you are here)
3. `state/status.yaml` — current phase, blockers, next action
4. `docs/ROADMAP.md` — the phased plan + where we are (then `docs/PHASE_2_PLAN.md` for the active phase)
5. `docs/FACTS.md` — what is currently true
6. `docs/FAILED_APPROACHES.md` — what NOT to do
7. `state/tasks.yaml` — open tasks; find one assigned to your role or unassigned

If your assigned task references specific files, read those next.

---

## Step 2 — Confirm Your Role

Open `.openclaw/agents/<your-role>.md`. The file defines:

- **Inputs:** files you may read
- **Outputs:** files you may write
- **Tools:** what tools you need (filesystem, web, Ghidra, build, game launch)
- **Handoff triggers:** when to stop and hand off to a different agent

If your role is not represented in `.openclaw/agents/`, you are probably running standalone. In that case, default to the closest match and document the deviation in `session-log.md`.

---

## Step 3 — Check for Drift

Before you do real work, sanity check that the project state matches reality:

- `git status` — anything uncommitted?
- `state/status.yaml` `last_updated` — when was the last session?
- Compare `state/status.yaml` `current_phase` with the latest entry in `state/session-log.md` — do they agree?

If state and session log disagree, **stop and reconcile**. Don't build on a contradiction.

---

## Step 4 — The Critical macOS Constraints

Internalize these before writing any code:

1. **Inline hooking does not work.** The `__TEXT` segment is read-only under macOS code signing. Use GOT, VTable, or function-pointer-table hooks (anything in `__DATA`).
2. **The game must be re-signed** with `com.apple.security.cs.disable-library-validation` for `DYLD_INSERT_LIBRARIES` to work. See `docs/reference/macos-codesigning.md`.
3. **macOS TweakDB layout ≠ Windows TweakDB layout.** Do not port Windows TweakXL `GetFlat`/`SetFlat` blindly.
4. **VTable layouts can change between game builds.** Any vtable-index claim must be dated and re-validated.

---

## Step 5 — Doing the Work

- **Read before write.** Always check FACTS, HYPOTHESES, FAILED_APPROACHES first.
- **Small, validated steps.** Don't write a 500-line PR. Land one validated finding at a time.
- **Validate on the actual game.** Compile-clean is not enough. The `tester` agent must confirm in-game behavior.
- **Update docs at the moment of discovery,** not at end-of-session when context has faded.
- **One claim per FACTS entry.** Compound claims are harder to invalidate cleanly.

---

## Step 6 — Closing Out

When you finish (or hand off):

1. **Append to `state/session-log.md`** — what happened, what changed, what's blocking, what's next.
2. **Update `state/tasks.yaml`** — close completed tasks, add follow-ups discovered during the work.
3. **Update `state/status.yaml`** (only if you are `doc-keeper`) — current phase, next action.
4. **Commit your changes** with a conventional message.

---

## Common Pitfalls (Lessons from Old Sessions)

- **"Worked once" ≠ "works."** Hook fires under one condition doesn't mean it fires in the relevant condition. Always test the actual scenario you care about.
- **Don't trust old findings as-is.** Game updates move things. Re-validate on the current binary.
- **Don't combine investigations.** "Let me also check X while I'm here" is how findings get muddled. One question at a time.
- **Crash logs are JSON, not text.** Parse `.ips` files with Python; don't grep them.

---

## If You Are Stuck

1. Re-read AGENTS.md and FAILED_APPROACHES.md — is the approach already known to fail?
2. Check if a `researcher` agent should be invoked first to validate assumptions.
3. Ask the user (via your runtime's question mechanism). Don't invent answers.
4. Log the block in `session-log.md` and `state/status.yaml.blockers`.

---

## Where to Get Quick Context

| Need | File |
|------|------|
| Project goal | `AGENTS.md` (top) |
| Current state | `state/status.yaml` |
| What works | `docs/FACTS.md` |
| What doesn't | `docs/FAILED_APPROACHES.md` |
| Architecture | `docs/ARCHITECTURE.md` |
| Build instructions | `docs/BUILD.md` |
| Codesigning | `docs/reference/macos-codesigning.md` |
| ARM64 hooking | `docs/reference/arm64-hooking.md` |
| TweakDB format | `docs/reference/tweakdb-binary-format.md` |
| Windows TweakXL API | `docs/reference/windows-tweakxl-api.md` |
