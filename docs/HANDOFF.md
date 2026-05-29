# HANDOFF.md — New Agent Bring-Up

You have just been spawned into this project. You probably have no memory of prior sessions. This document brings you up to speed in the minimum time.

---

## Step 1 — Read These Files in Order

1. `AGENTS.md` (repo root) — universal entry, conventions, constraints
2. This file (you are here)
3. `state/status.yaml` — current phase, blockers, next action
4. `docs/FACTS.md` — what is currently true
5. `docs/FAILED_APPROACHES.md` — what NOT to do
6. `state/tasks.yaml` — open tasks; find one assigned to your role or unassigned

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
