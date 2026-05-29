# AGENTS.md — Universal Agent Entry Point

**Project:** `cp2077-mac-mods`
**Purpose:** Port the Cyberpunk 2077 Windows modding stack (RED4ext + TweakXL) to native macOS Apple Silicon, so existing Nexus mods work unchanged on Mac.
**Audience:** Any AI agent (Claude, Codex, Gemini, custom) spawned via the OpenClaw runtime, or running standalone.

---

## You Are An Agent. Start Here.

1. Read this file completely.
2. Read `docs/HANDOFF.md` — step-by-step bring-up for new agents.
3. Read `state/status.yaml` — current phase + next action.
4. Read `docs/FAILED_APPROACHES.md` before writing any code. Do not repeat anything in there.
5. Identify which agent role you are (see `.openclaw/agents/`) and follow that role's contract.

If your role is not assigned in `state/tasks.yaml`, ask the user before proceeding.

---

## Mission

> Windows Cyberpunk 2077 mods must work on macOS **without modification**. Modders should not have to change YAML, `.tweak`, or `.dylib` plugin code.

**Compatibility is the #1 success criterion.** Not feature count, not performance, not architectural elegance.

**Long-term goal:** every gameplay-changing Windows mod works on macOS — TweakXL, ArchiveXL, CET, Codeware, redscript. The full ecosystem. This is the *destination*, not v1.0.

**v1.0 scope (first deliverable):** TweakXL YAML/`.tweak` mods load and apply in-game. Basic RED4ext `.dylib` plugin loading works. 5+ Nexus TweakXL mods verified working. <1 crash/hour. See D-006 for the slicing rationale.

**v1.x / v2.0 scope (post-v1.0, see `docs/ARCHITECTURE.md` Long-Term Roadmap):**
- v1.x — ArchiveXL parity (custom assets)
- v2.0 — CET (Lua + ImGui-on-Metal), redscript runtime, Codeware

**Out of scope for v1.0 (deferred, not abandoned):** ArchiveXL, CET, Codeware, redscript runtime. These all become in-scope post-v1.0 per the roadmap.

**Permanently out of scope:** Windows DLL cross-loading, Intel Macs, DRM bypass, anything beyond what Windows mods already do.

---

## Project State Lives in Three Places

| Where | Contents | Who writes it |
|-------|----------|---------------|
| `state/status.yaml` | Current phase, blockers, next action | `doc-keeper` agent only |
| `state/tasks.yaml` | Open task queue with agent assignments | `doc-keeper`, agents claim their own |
| `state/session-log.md` | Append-only log of significant events | Every agent at session end |

If you need to know "where are we?" — read `status.yaml`. If you need to know "what should I do?" — read `tasks.yaml`. If you need to know "how did we get here?" — read `session-log.md`.

---

## The Three-Tier Truth Model

Every claim about the project lives in **exactly one** of these files:

| File | Contains | Rules |
|------|----------|-------|
| `docs/FACTS.md` | Validated, reproducible facts | Append-only. Each entry: date, evidence, how-to-verify. Remove only if invalidated, and log the removal. |
| `docs/HYPOTHESES.md` | Theories being tested | Must resolve to FACTS or FAILED_APPROACHES. Stale hypotheses (>2 weeks unattended) get flagged. |
| `docs/FAILED_APPROACHES.md` | What was tried and didn't work | **Append-only, never remove.** Each entry: approach, why it failed, source/date. |

**If you find a contradiction, stop. Fix the docs first. Then write code.**

---

## How to Add Knowledge to the Project

- **Validated something new?** → Append to `FACTS.md` with date + evidence.
- **Want to test a theory?** → Add to `HYPOTHESES.md`, then run the experiment.
- **An approach didn't work?** → Append to `FAILED_APPROACHES.md` immediately, before context fades.
- **Made an architectural choice?** → Add an ADR entry to `DECISIONS.md`.
- **Finished a work session?** → Append a one-paragraph entry to `state/session-log.md`.

Do NOT update `status.yaml` directly unless you are the `doc-keeper` agent.

---

## Agent Roles

Defined in `.openclaw/agents/`:

| Agent | Role |
|-------|------|
| **`lead`** | **Orchestrator. Talks to user, delegates to specialists, coordinates. Read this first if you're acting as the user-facing agent.** |
| `researcher` | Ghidra analysis, runtime probing, web research, validates facts |
| `hook-engineer` | GOT/VTable hooking implementation, ARM64 trampolines |
| `tweakdb-engineer` | TweakDB binary parsing + runtime memory access |
| `mod-loader` | YAML/`.tweak` parsing, applies mod ops to TweakDB |
| `reviewer` | Code review, sanity checks before merge |
| `tester` | In-game validation, crash log analysis, test mod runs |
| `doc-keeper` | Maintains FACTS/STATUS, prunes stale claims, owns truth files |

The **lead** is the user's point of contact. Specialists work on behalf of the lead. If you don't know which agent you are, default to **lead** (which routes work to specialists).

Read your role file before starting work. It defines your inputs, outputs, tools, and handoff triggers.

---

## Critical Constraints (Inherited from Old Work)

These are the hard-won lessons. **Read `docs/FAILED_APPROACHES.md` for the full list.**

1. **macOS code signing blocks `__TEXT` segment writes.** Inline hooking does not work. Use GOT / VTable / function-pointer-table hooking on `__DATA`.
2. **macOS TweakDB is NOT Windows TweakDB.** The data structure is different — likely a hash table, not a sorted array. Do not port Windows TweakXL's `GetFlat`/`SetFlat` logic blindly.
3. **The game binary must be re-signed with `disable-library-validation` entitlement** for any `DYLD_INSERT_LIBRARIES` to work. See `docs/reference/macos-codesigning.md`.
4. **VTable layouts change with game updates.** Any vtable-slot-specific finding is dated; re-validate before trusting.
5. **Single source of truth on game version is `Cyberpunk2077.app/Contents/Info.plist`.** Always log it.

---

## Build & Test

See `docs/BUILD.md`. There is nothing meaningful to build yet — the project is in Phase 0 (re-validation).

---

## Conventions

- **C++17** (open to C++20 if a specific feature needs it; record in `DECISIONS.md`)
- **CMake 3.15+**
- **No** emoji in code or commit messages unless the user explicitly asks
- **No** generated documentation unless requested
- **Commits:** conventional style (`feat:`, `fix:`, `docs:`, `refactor:`). Keep messages terse.
- **Comments:** only when WHY is non-obvious. Never restate WHAT the code does.

---

## Open Questions

Live in `docs/HYPOTHESES.md` with `open-question:` prefix. If you cannot proceed without an answer, escalate to the user via your runtime's question mechanism (Claude Code: `AskUserQuestion`; OpenClaw: gateway question to operator).

---

## Where to Find Things

| Need | Path |
|------|------|
| Game binary | `~/Library/Application Support/Steam/steamapps/common/Cyberpunk 2077/Cyberpunk2077.app/Contents/MacOS/Cyberpunk2077` |
| TweakDB binary | `<game-dir>/r6/cache/tweakdb.bin` |
| Mod target dir | `<game-dir>/r6/tweaks/` |
| Crash reports | `~/Library/Logs/DiagnosticReports/` |
| Ghidra project | `reference/ghidra/` |
| Test mods | `reference/example-mods/` |
| Windows TweakXL ref | `reference/windows-tweakxl/` |

---

## When in Doubt

Re-read this file. If still in doubt, ask the user. Never invent state or assume facts not in `FACTS.md`.
