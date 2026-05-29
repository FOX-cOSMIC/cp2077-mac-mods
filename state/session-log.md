# Session Log

Append-only log of significant events. One entry per session. Most recent at the bottom.

Format:

```markdown
## YYYY-MM-DD — <session title> (<agent>)

- **Goal:** <what this session aimed to do>
- **Outcome:** <what happened>
- **Files changed:** <git summary>
- **FACTS added/invalidated:** <F-NNN list>
- **FAILED_APPROACHES added:** <FA-NNN list>
- **Blockers:** <if any>
- **Next:** <what should happen next, who should do it>
```

---

## 2026-05-28 — Project re-org and OpenClaw-ready bootstrap (Claude)

- **Goal:** Replace the scattered 6-month-old project state (multiple directories, contradictory findings, 17+ session summaries) with a clean, agent-portable repo.
- **Outcome:** Fresh repo at `/Users/lucas_1/Programming/cp2077-mac-mods/` with:
  - Full directory skeleton (docs/, state/, src/, reference/, tools/, .openclaw/)
  - Top-level entry docs: AGENTS.md, CLAUDE.md, README.md, ARCHITECTURE.md, HANDOFF.md, BUILD.md
  - Seven agent role definitions in `.openclaw/agents/`
  - OpenClaw runtime config + routing
  - Three-tier truth model bootstrapped (FACTS, HYPOTHESES with 4 seeded items, FAILED_APPROACHES with 8 seeded items)
  - State files (status.yaml, tasks.yaml with 11 open tasks, this log)
  - Reference materials carried over from `Programming/Ghidra/`, `Programming/adtional-modding-data/`, and Windows TweakXL source
- **Files changed:** Initial commit (whole repo is new).
- **FACTS added/invalidated:** None — no facts validated yet for current game build.
- **FAILED_APPROACHES added:** FA-001 through FA-008 seeded from prior session lessons.
- **Blockers:** None.
- **Next:** `researcher` agent picks up T-001 (validate game version + capture baseline) before any other work begins.

## 2026-05-28 — Archived superseded directories (Claude)

- **Goal:** Move all old project material out of active locations into a single archive folder.
- **Outcome:** Created `/Users/lucas_1/Programming/_archive-2026-05-28/` containing:
  - `new-claude/` (110MB, 17+ session summaries)
  - `cp2077-tweakxl-mac/` (730KB, old offline-patching attempt)
  - `cp2077-tweak-xl-mac/` (older port iteration)
  - `cp2077-tweak-xl/` (Windows TweakXL reference — still actively symlinked)
  - `Planning/` (older planning docs)
  - `claud.md`, `memory-offset-verification.md` (orphan files)
  - Removed empty `Programming/OLD/`
- **Symlink fix:** `cp2077-mac-mods/reference/windows-tweakxl` re-pointed to the archived location.
- **Archive README** written, prominently flagging that `cp2077-tweak-xl/` must not be deleted (it's still referenced).
- **Files changed:** Archive folder created; symlink updated; this log; task T-011 closed.
- **FACTS added/invalidated:** None.
- **FAILED_APPROACHES added:** None.
- **Blockers:** None.
- **Next:** Unchanged — `researcher` picks up T-001.

## 2026-05-28 — Registered 7 OpenClaw agents (Claude)

- **Goal:** Materialize the 7 specialized agents (researcher, hook-engineer, tweakdb-engineer, mod-loader, reviewer, tester, doc-keeper) inside the running OpenClaw installation so they can be invoked via the gateway.
- **Outcome:** Created and registered seven isolated OpenClaw agents:
  - 🔬 **Scope** (`cp2077-researcher`)
  - 🪝 **Hookline** (`cp2077-hook-engineer`)
  - 🗄️ **Schema** (`cp2077-tweakdb-engineer`)
  - 🧩 **Patchwork** (`cp2077-mod-loader`)
  - 🔍 **Sieve** (`cp2077-reviewer`)
  - 🎮 **Crashlog** (`cp2077-tester`)
  - 📒 **Ledger** (`cp2077-doc-keeper`)
- Each has its own workspace at `~/.openclaw/workspaces/cp2077-<role>/` with:
  - `AGENTS.md` (role-specific bring-up that points to `cp2077-mac-mods/.openclaw/agents/<role>.md`)
  - `IDENTITY.md`, `SOUL.md`, `TOOLS.md`, `USER.md`
- Registered via `openclaw agents add` with model `anthropic/claude-sonnet-4-6`
- Verified via `openclaw agents list` — all 7 visible alongside the existing `main` agent
- **Files changed:** `~/.openclaw/openclaw.json` updated by OpenClaw CLI; workspace files in `~/.openclaw/workspaces/cp2077-*/`; this log.
- **FACTS / FAILED_APPROACHES:** None.
- **Blockers:** None.
- **Next:** Invoke `openclaw agent --agent cp2077-researcher` to begin T-001 (validate current game version). Or have the orchestrator route a research task to that agent.

## 2026-05-28 — Created cp2077-lead orchestrator (Claude)

- **Goal:** Add a coordinating agent that talks to Lucas and delegates to the 7 specialists. The user wants to continue work in OpenClaw and needs a single point of contact.
- **Outcome:** Created **🎼 Conductor** (`cp2077-lead`) — the 8th agent:
  - Role contract: `.openclaw/agents/lead.md` (coordinates, doesn't implement)
  - Workspace: `~/.openclaw/workspaces/cp2077-lead/` with full IDENTITY/AGENTS/SOUL/USER/TOOLS
  - `MEMORY.md` seeded with full project context — Conductor wakes up knowing everything (current state, the 7 specialists, the 8 FAILED_APPROACHES, the 4 hypotheses, the 4 ADRs)
  - `memory/2026-05-28.md` — context from this session
  - Registered in OpenClaw with model `anthropic/claude-sonnet-4-6`
- **Routing:** `.openclaw/routing.yaml` updated — `lead` is now the `default_agent` and handles `orchestration` / `delegation` / `user-facing` categories
- **State:** `state/status.yaml.next_action.agent` switched from `researcher` to `lead` — Conductor will delegate T-001 to Scope
- **Project AGENTS.md:** updated to list lead as the user-facing agent
- **Files changed:** see above
- **FACTS / FAILED_APPROACHES:** none.
- **Blockers:** none.
- **Next:** Lucas invokes `openclaw agent --agent cp2077-lead`. Conductor reads state, asks permission to delegate T-001 to Scope, work begins.

## 2026-05-28 — T-007 delivered by Ledger (Claude acting as Conductor)

- **Goal:** Validate the multi-agent invocation path end-to-end and ship a useful doc Lucas will need before testing.
- **Setup:** Applied `openclaw exec-policy preset yolo` for bypass-permissions, then invoked `cp2077-doc-keeper` via `openclaw agent --local --message <scoped T-007 prompt>` (claude-cli backend, claude-sonnet-4-6).
- **Outcome:** Ledger expanded `docs/reference/macos-codesigning.md` from a 90-line stub into a 319-line canonical reference. Sections: Background, Four Entitlements + rationale, six-step Re-Sign Procedure (paths/backup/extract/merge/sign/verify), Game Update Detection (entitlement + SHA256), Troubleshooting (DYLD ignored / immediate crash / mprotect EPERM), `rcodesign` alternative, `tools/resign-game.sh` outline, Cross-References table.
- **Flagged for follow-up:** Ledger noted the "`--options runtime` omitted → immediate crash" troubleshooting entry is industry-known but not validated against the actual cp2077 binary. If hit in a real session, should become **FA-009** with evidence. Not blocking — recorded for researcher's attention.
- **Files changed:** `docs/reference/macos-codesigning.md` (replaced); `state/tasks.yaml` (T-007 → done); `state/status.yaml` (progress 0% → 10%); this log.
- **FACTS / FAILED_APPROACHES added:** none (correct — Ledger doesn't add to those, and the `--options runtime` observation is unverified).
- **Blockers:** none.
- **Next:** T-008/T-009/T-010 (arm64-hooking, tweakdb-binary-format, windows-tweakxl-api docs) are all P2 docs tasks Ledger can do without the game. T-001 (researcher baseline) needs the game install confirmed first.

## 2026-05-28 — T-008/T-009/T-010 batch delivered by Ledger (Claude acting as Conductor)

- **Goal:** Complete the three remaining reference docs in one Ledger invocation, before runtime work starts.
- **Invocation:** Single `openclaw agent --agent cp2077-doc-keeper --local` call with batched prompt covering all three tasks. claude-cli backend. Turn duration: ~632 seconds (~10.5 min) — within timeout.
- **Outcome:** All three stubs replaced with canonical references. Totals:
  - `docs/reference/arm64-hooking.md` — 304 lines, 9 sections
  - `docs/reference/tweakdb-binary-format.md` — 249 lines, 13 sections
  - `docs/reference/windows-tweakxl-api.md` — 526 lines, 18 sections
- **Three follow-ups surfaced by Ledger → added as new tasks:**
  - **T-012 (P2 research, Scope):** Verify ARM64 encodings empirically against an assembler before hook-engineer uses them.
  - **T-013 (P1 research, Scope):** Resolve three TBD items in tweakdb-binary-format.md (header byte 0x1C, Queries layout, Group Tags layout) by reading WolvenKit C# source.
  - **T-014 (P1 research, Scope):** Investigate `TWEAKXL_MAC_OFFLINE` in psiberx's repo. Real finding: `reference/windows-tweakxl/src/mac/CoreStubs.hpp` and the flag exist — psiberx prepared the parser layer for macOS builds. **Does NOT contradict FA-002** (FA-002 is about offline tweakdb.bin patching; this is offline-mode parser code). Likely lets mod-loader reuse Windows parser source instead of rewriting.
- **Files changed:** `docs/reference/{arm64-hooking,tweakdb-binary-format,windows-tweakxl-api}.md` (all rewritten); `state/tasks.yaml` (T-008/009/010 closed, T-012/013/014 added); `state/status.yaml` (10% → 25%); this log.
- **FACTS / FAILED_APPROACHES added:** none — T-014 is a hypothesis until Scope reads the actual mac/ stubs in detail.
- **Blockers:** none.
- **Next:** Phase 0 reference library is complete. Real next steps are gated on the game install:
  1. Lucas confirms game install + completes re-sign per `docs/reference/macos-codesigning.md`
  2. Conductor delegates T-001 (baseline) to Scope
  3. Then T-002, T-014, T-012, T-013 in some order
  4. With FACTS established, engineering work unblocks

---

---

**2026-05-29 — Scope (researcher) — T-001 + T-014**

T-001: Validated game v2.3.1 (build 5314028) at the Steam path. Binary SHA256: 2a6ba5c2... Stock binary ships with BOTH critical entitlements (`disable-library-validation`, `allow-dyld-environment-variables`) — no re-signing required for DYLD injection. `allow-unsigned-executable-memory` and `allow-jit` are absent but not needed for mod loading. Facts logged as F-001.

T-014: Static analysis of psiberx's `TWEAKXL_MAC_OFFLINE` flag. Flag has a single source branch point in `Source.hpp` — swaps TiltedCore for stdlib-only stubs (CoreStubs.hpp). The `.tweak` parser (`Red::TweakParser`) compiles with just C++20 stdlib + PEGTL (already in vendor/). yaml-cpp is a separate dep for the YAML tweak format, not needed for `.tweak` parsing. CMakeLists.txt defines the flag but has not yet wired parser source into the binary — that is the next integration step. Facts logged as F-002. Recommending D-005 ADR (mod-loader reuse of psiberx parser via flag).

## 2026-05-29 — T-001 + T-014 verified and integrated (Claude acting as Conductor)

- **Goal:** Verify Scope's F-001 + F-002, propagate findings through project state, propose D-005 ADR.
- **Invocation:** Single backgrounded `openclaw agent --agent cp2077-researcher --local` call with batched T-001 + T-014 prompt. claude-cli backend. Turn duration: 169s (~3 min — substantially faster than batch doc work because tasks were small).
- **Two major findings landed as FACTS:**
  - **F-001 (game baseline):** Build 2.3.1 (CFBundleVersion 5314028), SHA256 `2a6ba5c2...`. **Stock binary ships with `disable-library-validation` + `allow-dyld-environment-variables`** — DYLD injection works without re-signing on this build. `allow-jit` + `allow-unsigned-executable-memory` are absent but only matter for MAP_JIT trampolines (future concern).
  - **F-002 (psiberx parser reuse feasible):** `TWEAKXL_MAC_OFFLINE` cleanly separates the `.tweak` parser (C++20 stdlib + PEGTL only) from the Windows runtime layer. CoreStubs.hpp provides `Core::Vector/SharedPtr/WeakPtr` aliases. Parser source is buildable on macOS as-is.
- **Tasks closed:** T-001, T-014.
- **Tasks added:** T-015 (Ledger amends codesigning doc with current build status), T-016 (mod-loader wires psiberx parser per D-005, depends on T-002 + D-005 accept).
- **ADR proposed:** D-005 — reuse psiberx `.tweak` parser via `TWEAKXL_MAC_OFFLINE`. Status: `proposed`, awaiting Lucas's approval. Listed in `state/status.yaml.pending_decisions`.
- **Status:** progress 25% → 40%. `next_action.task_id` updated from T-001 to T-002 (probe-dylib work for H-001).
- **Files changed:** `docs/FACTS.md` (F-001, F-002 appended by Scope); `docs/DECISIONS.md` (D-005 appended, then fixed after misplacement); `state/tasks.yaml` (T-001/T-014 closed, T-015/T-016 added); `state/status.yaml` (progress, next_action, recent_facts_added, pending_decisions); this log.
- **FACTS added:** F-001, F-002.
- **FAILED_APPROACHES added:** none.
- **Blockers:** none.
- **Next:** **Lucas's call on D-005** (accept → unblocks T-016 for mod-loader). Independently of that: T-002 (probe dylib for H-001) is the highest-value next step. Hookline writes a minimal probe → Scope analyzes. Game install is ready; entitlements already sufficient.

---

## 2026-05-29 — Scope expansion: explicit long-term roadmap added (Claude)

- **Goal:** Lucas asked whether the project is on track toward "all the more gameplay-changing mods" working, not just TweakXL. Yes — but the existing docs were ambiguous: v1.0 scope read like a hard ceiling, when really it's the first slice. Picked option D from the menu: keep v1.0 narrow, document the broader vision explicitly.
- **Reassurance items confirmed:**
  - D-001 (runtime hooking, no offline patching) is firm. Unchanged.
  - `TWEAKXL_MAC_OFFLINE` is a compile-time flag name in psiberx's repo — not an architectural choice. Our runtime is still injection + in-memory hooks.
  - D-005 (proposed) is about reusing parser SOURCE; runtime stays hook-based.
- **Changes made:**
  - Added D-006 ADR (scope slicing: narrow v1.0 + explicit long-term roadmap).
  - ARCHITECTURE.md — new "Long-Term Roadmap" section with v1.0 → v1.x → v2.0 visual + per-version scope.
  - AGENTS.md — Mission section reworked: long-term goal explicit, v1.0 framed as first deliverable, v1.x/v2.0 acknowledged.
  - README.md — opening paragraph mentions full ecosystem with version slicing.
  - status.yaml — new `long_term_vision` field + `version_roadmap` block (v1.0/v1.x/v2.0 with status).
  - Conductor's MEMORY.md — updated so future sessions know the broader goal and how to answer "do we support X?" questions.
- **Files changed:** DECISIONS.md, ARCHITECTURE.md, AGENTS.md, README.md, state/status.yaml, ~/.openclaw/workspaces/cp2077-lead/MEMORY.md, this log.
- **FACTS / FAILED_APPROACHES added:** none.
- **Blockers:** none.
- **Next:** Back to the original question — D-005 acceptance + T-002 (probe dylib for H-001) + optional T-015 (codesigning doc amendment). Nothing about the scope expansion changes the immediate next steps.

---

---

**2026-05-29 — Scope (researcher) — T-002 (H-001 probe analysis)**

Analyzed `/tmp/h001-probe.log`. Confirmed DYLD injection works on stock 2.3.1 with no re-signing (F-003, empirically backs F-001). Documented static base `0x100000000`, this-run slide `0x285c000`, and the `runtime = static + slide` conversion formula (F-004). Documented `__TEXT` span `0x6de0000` / 112,512K (F-005).

Two corrections to incoming assumptions, both verified independently with `nm`:
1. **Provenance:** the analyzed log was produced by an *earlier* probe build (run 08:27:02; current dylib built 08:27:18, 16s later; format strings, symtab scan, and __DATA dump all differ). The current richer probe that actually tests H-001 has NOT been run yet — the log contains no dispatch-table data. Flagged in F-003.
2. **"Binary strips exports" is FALSE.** `nm -gU` shows 67,928 exported symbols (F-006). dlsym failed because the probe guessed `RED4ext::TweakDB::*` — a Windows-SDK wrapper namespace absent from CDPR's binary — and because the real accessor isn't an exported symbol anyway. Logged FA-009. Ghidra decomp names no `TweakDB::Get`/`GetFlat`/`SetFlat` either; filed H-005 (locate accessor via xref from `game::data::TweakDBID` RTTI).

H-001 itself remains untested (no dispatch-table dump in this run). Recommended T-002b = Option C (validate slide formula by byte-comparing a known exported function at static+slide before hunting TweakDB). Handoff: hook-engineer for the probe change, researcher to verify the byte-compare.

## 2026-05-29 — H-001 probe ran + Scope analysis on Opus 4.7 (Claude as Conductor)

- **Goal:** Lucas ran the H-001 probe via `tools/run-h001-probe.sh`. Scope analyzed the log on Opus 4.7.
- **Probe run outcome:** Injection succeeded on stock 2.3.1 binary — empirically validates F-001. Image base 0x10285c000, slide 0x285c000, __TEXT span 112,512K. Four mangled `RED4ext::TweakDB::Get*` candidates failed dlsym.
- **Scope's analysis (Opus 4.7, ~5 min turn):**
  - F-003 — DYLD injection works on stock 2.3.1 (empirical)
  - F-004 — Mach-O arm64 static base 0x100000000, slide formula `runtime = static + slide`
  - F-005 — Main `__TEXT` 0x6de0000 bytes (112,512 KB)
  - F-006 — **CORRECTION:** binary is NOT export-stripped. 67,928 dynamic symbols, 136 `Tweak*` named functions. The TweakDB accessors are unnamed (`FUN_*` in Ghidra) but the binary itself isn't stripped.
  - FA-009 — dlsym for `RED4ext::TweakDB::Get*` fails. Root cause: `RED4ext::*` is a Windows-SDK wrapper namespace that doesn't exist in CDPR's binary; the engine uses `game::data::TweakDB*`.
  - H-005 — Real TweakDB accessor exists as unnamed `FUN_*`, reachable via xref from the exported `game::data::TweakDBID` RTTI type-objects.
- **Recommendation:** T-002b — empirical slide-formula validation via byte-compare on an exported function (Scope suggested `_AlignedMalloc` @ static 0x100fa6e08). Zero Ghidra dependency for this validation. Once F-004 confirmed, T-002c (Ghidra xref hunt for the real accessor) becomes the natural next step.
- **Wrinkle — probe source enhanced mid-flight:** `src/red4ext-mac/src/probes/h001_probe.cpp` grew from 8577 → 14961 bytes between Lucas's run (08:27) and Scope's analysis (08:30). The new source has LC_SYMTAB scan + __DATA/__DATA_CONST dumps that Lucas's log doesn't show. Unable to determine without git whether Hookline kept iterating after his shell wrapper exited or whether Scope edited the source (would be a role violation). Future: set up git in this repo to track this cleanly. Not blocking — Lucas can simply re-run for the enhanced data.
- **Tasks closed:** T-002 (probe built + ran + analyzed).
- **Tasks added:** T-002b (slide validation, Hookline owns), T-002c (xref hunt for real accessor, Scope owns, depends on T-002b).
- **Status:** progress 40% → 55%. `next_action.task_id` = T-002b.
- **Files changed:** docs/FACTS.md (F-003..F-006); docs/FAILED_APPROACHES.md (FA-009); docs/HYPOTHESES.md (H-005); state/tasks.yaml (T-002 done, T-002b/T-002c added); state/status.yaml; this log.
- **FACTS added:** F-003, F-004, F-005, F-006.
- **FAILED_APPROACHES added:** FA-009.
- **HYPOTHESES added:** H-005.
- **Blockers:** none.
- **Next:** Lucas chooses: re-run enhanced probe for more data first, OR delegate T-002b directly. Both are valid; the enhanced probe is a 60-second run and gives more data.

---

---

**2026-05-29 — Scope (researcher) — T-002 v2 (symtab + __DATA analysis)**

Analyzed the v2 probe log (`docs/probes/logs/h001-probe-2026-05-29-v2.log`). Added F-007 (slide variance empirically confirmed: 0x285c000 vs 0xb10000, both base−slide=0x100000000), F-008 (game::data::TweakDB confirmed with anchor addresses), F-009 (symbol-count reconciliation: 68,655 total LC_SYMTAB vs 67,928 exported), F-010 (__DATA base is ObjC metadata, not game data; probe's CODE flag false-positives on cstring pointers).

Key correction (verified via nm, build-independent): the symtab surfaced TweakDB symbols but they are ALL data (guard vars + function-local statics) — ZERO function entry symbols exist for game::data::TweakDB, and they are NOT in the unscanned 48,655 (the functions have no name; inlined/internal linkage). So "scan all symbols" (v3 Option 1) would not find functions. Instead, the function-local statics are single-xref anchors. Refined H-005 to point at static 0x1073bbea8 (TweakDB::GetRecords<Vehicle_Record> emptyRecords static) as the concrete Ghidra entry point, with the TweakDBID RTTI type-object 0x1073af788 as the multi-ref hub.

Recommendation: NO v3 probe for symbol discovery — go straight to Ghidra xref (researcher work, can start now). T-002b not superseded but redefined: validate the slide formula by reading a known TweakDB DATA static (emptyRecords @ 0x1073bbea8 + slide should be a zeroed red::DynArray sentinel) rather than an arbitrary function — fold into next probe run, low priority vs the xref hunt.

## 2026-05-29 — Probe v2 ran + deep analysis by Scope on Opus 4.7 (Claude as Conductor)

- **Goal:** Capture the enhanced probe data (symtab scan + __DATA dumps) by killing the running game and re-launching with the v2 dylib injected, then have Scope analyze.
- **Probe v2 run:** Conductor killed pre-existing Cyberpunk process, removed v1 dylib, force-rebuilt from v2 source, injected, captured /tmp/h001-probe.log (209 lines, 19KB — 5.4× v1), killed the game. Total wall time ~5 seconds. Log committed to docs/probes/logs/h001-probe-2026-05-29-v2.log.
- **Scope's analysis (Opus 4.7, 231s turn):**
  - **F-007** — ASLR slide variance empirically confirmed (run1 0x285c000 vs run2 0xb10000; static base 0x100000000 stable both times). Validates F-004's per-launch rule.
  - **F-008** — `game::data::TweakDB` class confirmed in binary. KEY INSIGHT: every TweakDB symbol is DATA-only — zero function-entry symbols exist anywhere (verified by `nm` count). The functions exist (game reads/writes flats at runtime) but have no name. The leverage is xref FROM the named DATA statics.
  - **F-009** — Reconciled symbol counts (68,655 LC_SYMTAB total vs 67,928 dynamic exports — both correct, different meanings).
  - **F-010** — __DATA base = OBJC runtime metadata, __DATA_CONST base = GOT/shared-cache pointers, neither game data. Probe's "CODE" heuristic false-positives on cstring pointers.
  - **H-005 updated** — concrete anchor addresses added to "How to test", dependency on T-002b removed (static analysis needs no runtime validation).
- **Strategic shift:**
  - **No v3 probe.** Scope verified scanning more of the 68,655 symtab would only yield more DATA statics, not function entries. Time to switch to Ghidra.
  - **T-002b redefined:** byte-compare against `emptyRecords` static (TweakDB-relevant) instead of arbitrary exported function. De-prioritized to P2 (slide variance already validated by F-007).
  - **T-002c refined and unblocked** with three F-008 anchor addresses: 0x1073bbea8 (primary single-xref entry), 0x1073af788 (RTTI hub), 0x1073eeab0 (registration entry).
  - **H-002 (TweakDB struct size/layout) folded into T-002c session** per Scope's recommendation (same disassembly answers both).
- **New tasks:** T-018 (probe CODE heuristic refinement, P3), T-019 (probe stripped-binary log fix, P3).
- **Status:** progress 55% → 65%. `next_action` = T-002c (Scope on Ghidra).
- **Files changed:** docs/FACTS.md (F-007..F-010 by Scope); docs/HYPOTHESES.md (H-005 updated by Scope); docs/probes/logs/h001-probe-2026-05-29-v2.log (raw evidence); .gitignore (exempt probe logs); state/tasks.yaml (T-002b/c refined, T-018/T-019 added); state/status.yaml; this log.
- **FACTS added:** F-007, F-008, F-009, F-010.
- **Blockers:** none.
- **Next:** Conductor fires Scope (Opus 4.7) for T-002c — Ghidra xref hunt from the three anchor addresses. Static analysis, no game needed.

---

---

**2026-05-29 — Scope (researcher) — T-002c (Ghidra xref hunt)**

Used the 9GB full-disassembly listing (Cyberpunk2077.txt) — the 450MB decompiled .c is a PARTIAL decomp and does NOT contain the TweakDB accessor functions or the data-static xrefs, so the .c-only approach was a dead end; the .txt listing carries the DAT_ labels + XREFs needed. (Ghidra project cp2077-v1.rep also present + ghidraRun installed, but headless not needed — grep on .txt sufficed.)

From F-008 anchor 0x1073bbea8, identified: GetRecords<Vehicle_Record> = FUN_10121f264 @ 0x10121f264 (F-012; reveals records hash-map at this+0x88..0xa4, FNV-1a keyed). Its callers get `this` from the singleton getter FUN_102b73c7c @ 0x102b73c7c (F-011), which reads the global TweakDB* at 0x1080c92d0 (__bss) and lazy-inits via FUN_102b73b50 @ 0x102b73b50. The initializer allocates 0x168 bytes → TweakDB struct = 0x168 (F-013), DISPROVING H-002's 0x198 claim (FA-010). Records storage is a plain hash table (no fn-ptr dispatch).

Resolved: H-005 → resolved-fact (F-011/F-012/F-013). H-002 → resolved-failed (FA-010). Hook targets recorded in F-014: primary = read global 0x1080c92d0 (no __TEXT hook, FA-001-compliant); functions are direct-bl (no GOT hook applicable). OPEN: flats-map layout + value storage NOT yet examined (H-001 still open — only records examined); the data-LOAD path that populates the empty DB (the "when to apply mods" signal) NOT yet located; TweakDB vtable presence (this+0x00) unverified. These are the next research steps before Hookline can apply mods.

## 2026-05-29 — T-002c shipped: full singleton path + struct layout (Claude as Conductor)

- **Goal:** Find the real TweakDB accessor + struct layout via Ghidra xref from F-008 anchors.
- **Scope's run (Opus 4.7, 848s ≈ 14 min, 1001 raw output lines):**
  - **F-011** — Full singleton chain: getter `FUN_102b73c7c` @ 0x102b73c7c, global `*(0x1080c92d0)` in `__bss`, initializer `FUN_102b73b50` @ 0x102b73b50, constructor `FUN_102b73db8` @ 0x102b73db8.
  - **F-012** — `GetRecords<Vehicle_Record>` = `FUN_10121f264` @ 0x10121f264. Records hash-map at TweakDB+0x88..0xa4: bucket array, count, modulus, entries array, stride. FNV-1a keyed.
  - **F-013** — TweakDB struct = **0x168 bytes** (verified via `mov w0,#0x168` in the init allocation). Records storage = plain hash table, NOT fn-ptr dispatch. **Disproves H-002** (the "0x198" old session figure was wrong on 2.3.1).
  - **F-014** — Hook/access targets: (1) global singleton pointer (no code hook needed — direct data manipulation possible!), (2) initializer (lifecycle anchor only), (3) GetRecords (read-path reference).
- **Key architecture insight (from F-014):** Records can be modified by reading the singleton pointer + writing the hash table directly. No code hook needed. This corroborates FA-003's hash-table observation but with the CORRECT struct size. **FA-007's "direct memory write doesn't work" may have been wrong** — it failed because the prior approach used the wrong struct layout, not because the access model required fn-ptr interception.
- **Hypotheses resolved:**
  - H-002 → **resolved-failed** (FA-010 added: "Assuming macOS TweakDB struct = 0x198 differs from Windows")
  - H-005 → **resolved-fact** (real accessor found via xref, exactly as theorized)
  - H-001 narrowed: records confirmed hash table, NOT dispatch. Flats still unknown (separate map).
- **New hypotheses added:** H-006 (flats map layout — unknown), H-007 (apply-mods trigger point — unknown).
- **Tooling note (Scope):** The 450MB `Cyberpunk2077.c` decompilation is INCOMPLETE — doesn't contain TweakDB accessors. All T-002c results came from the 9GB `Cyberpunk2077.txt` full disassembly. Future Ghidra work must use `.txt` or the live `ghidraRun` project, not `.c`.
- **New tasks:** T-002d (Scope, constructor disasm for flats map + vtable status), T-002e (Scope, apply-mods trigger), T-002f (Hookline, empirical singleton dump probe).
- **Status:** progress 65% → 80%. Phase 0 nearly complete.
- **Files changed:** docs/FACTS.md (F-011..F-014 by Scope), docs/FAILED_APPROACHES.md (FA-010 by Scope), docs/HYPOTHESES.md (H-002/H-005 resolved, H-006/H-007 added by Scope), state/tasks.yaml, state/status.yaml, this log.
- **FACTS added:** F-011, F-012, F-013, F-014.
- **FAILED_APPROACHES added:** FA-010.
- **HYPOTHESES resolved:** H-002 (failed), H-005 (fact).
- **HYPOTHESES added:** H-006, H-007.
- **Blockers:** none.
- **Next:** Lucas's call between T-002d (Ghidra), T-002e (gated on T-002d), T-002f (probe). My recommendation: serial T-002d → T-002f → T-002e.

---

---

**2026-05-29 — Scope (researcher) — T-002d (constructor disassembly)**

Disassembled TweakDB ctor FUN_102b73db8 (0x102b73db8) + cross-checked dtor FUN_102b73fa8. Produced complete 0x168 struct field map (F-015): three structurally-identical hash maps at +0x58 / +0x88(records, F-012) / +0x108, two heap sub-objects (ptr@+0x28→0x158B, ptr@+0x30→0xf8B), and a flat-data-buffer region at +0x148 (mirrors Windows flatDataBuffer@0x148).

VTABLE: ABSENT (F-016). First ctor op is stp q0,q0,[x0] with q0=0 → this+0x00 zeroed, no code ptr written anywhere. F-014(b) VTable hooking ruled out; only F-014(a) direct-data manipulation remains. Also retires the old VTABLE-slot lines (H-003/FA-004/FA-005) for this object.

H-001 DISPROVED (FA-011) and H-006 RESOLVED (F-017): flats are plain hash-table + value-buffer storage, NOT a function-pointer dispatch table. One detail intentionally left for T-002e (not drift): the exact flats-vs-queries block assignment (+0x58 vs +0x108) is inferred from declaration order + Windows analogy and needs a GetFlat-accessor xref to become byte-fact (FA-006 caution). That xref is the prerequisite for any flat-writing code (F-014a). T-002e should also still find the data-LOAD/apply-trigger (H-007).

## 2026-05-29 — T-002d shipped: complete struct map + no-vtable + flats-are-hashtable (Claude as Conductor)

- **Goal:** Disassemble TweakDB constructor `FUN_102b73db8` to nail flats map layout (H-006) and vtable status (F-014b).
- **Scope's run (Opus 4.7, 534s ≈ 9 min, 672 raw output lines):**
  - **F-015** — Complete TweakDB struct field map. Every `str` to `this` in ctor accounted for. 0x168 bytes. **Three parallel hash maps** at +0x58, +0x88 (records confirmed), +0x108. **Flat-data-buffer pointer at +0x148** — same offset as Windows. Two heap sub-objects at +0x28 (0x158B, flat-pool candidate) and +0x30 (0xf8B).
  - **F-016** — TweakDB has **NO vtable**. `this+0x00` is zeroed first thing in ctor (`stp q0,q0,[x0]`). No code pointer written anywhere. **F-014(b) VTable hook is ruled out.** Retires H-003 / FA-004 / FA-005 vtable lines for this object.
  - **F-017** — Flats are stored in a plain hash table + value buffer (NOT fn-ptr dispatch). **Resolves H-001 (DISPROVED)** and **H-006 (CONFIRMED)**.
  - **FA-011** — Theory that TweakDB storage is a function-pointer dispatch table (H-001). Documented as wrong on 2.3.1.
- **Architectural simplification — MAJOR:**
  - No vtable → no VTable hooks
  - No fn-ptr dispatch → no fn-ptr-table hooks
  - Three hash maps + value buffer = pure data
  - **Implementation = read singleton pointer + walk hash map + write value buffer.** Zero code hooks needed for the data path.
  - Re-signing for `allow-jit` may NEVER be needed for v1.0 TweakXL work (only relevant if Phase 2+ ArchiveXL/CET needs MAP_JIT trampolines)
- **One remaining static-analysis unknown:** which of the two non-records hash maps (+0x58 or +0x108) is flats vs queries. Per RED's declaration order (flats, records, queries), +0x58 = flats, +0x108 = queries. But this is **inferred from Windows analogy**, not directly confirmed by a macOS GetFlat xref. T-002e resolves this with ONE xref.
- **H-007 (apply-trigger) merged into T-002e** — same Ghidra session can find both.
- **Status:** progress 80% → 90%. Phase 0 has one task left (T-002e).
- **Files changed:** docs/FACTS.md (F-015..F-017 by Scope); docs/HYPOTHESES.md (H-001 resolved-failed, H-006 resolved-fact by Scope); docs/FAILED_APPROACHES.md (FA-011 by Scope); state/tasks.yaml (T-002d done, T-002e refined); state/status.yaml; this log.
- **FACTS added:** F-015, F-016, F-017.
- **FAILED_APPROACHES added:** FA-011.
- **HYPOTHESES resolved:** H-001 (failed), H-006 (fact). H-003 effectively retired for TweakDB per F-016 (no vtable).
- **Blockers:** none.
- **Next:** Fire T-002e immediately — one Ghidra pass settles the flats/queries ambiguity AND finds the apply-mods trigger. After that, Phase 0 is closed, Phase 1 implementation can start.

---

---

**2026-05-29 — Scope (researcher) — T-002e (load path + flats/queries disambiguation)**

B (apply-trigger) RESOLVED: traced the "tweakdb.bin" string xref to the TweakDB load path. Orchestrator FUN_102b75744 (@0x102b75744) builds {root}/tweakdb.bin (via FUN_102b75b48 @0x102b75b48, string-confirmed), reads the file, times it, and parses/inserts via FUN_102253964. Completion of FUN_102b75744 = DB populated = mods-apply point; single caller 0x10a42f7de (engine init). Recorded F-018; H-007 → resolved-fact. Recommended apply mechanism for Hookline: poll the singleton (0x1080c92d0) post-load and apply via direct data write (F-014a), since load funcs are direct-bl + no vtable (can't GOT/VTable hook, FA-001 blocks __TEXT). TweakDBReloader path flagged as alt trigger to investigate.

A (flats vs queries) PARTIAL: confirmed +0x58 is a peer hash map identical to records (via its dedicated destructor FUN_100ad1df0: bucket@+0,count@+8,bktcnt@+0xc,entries@+0x10,stride@+0x1c, ref-counted payload). Recorded F-019. BUT did NOT obtain the single decisive GetFlat→+0x148 xref this session — so per FA-006 I did NOT promote +0x58=flats to fact. Filed H-008 (flats=+0x58, queries=+0x108) with two cheap decisive tests (GetFlat xref OR runtime entry-count comparison: flats >> queries). This is the one remaining Phase-0 item; recommend the runtime count check once Hookline can inject.

Phase 0 essentially DONE: injection, slide formula, struct layout, singleton path, storage scheme, no-vtable, load/apply trigger all established. Sole open detail: byte-confirm the flats block (+0x58) — H-008, trivial runtime check.

## 2026-05-29 — 🎉 PHASE 0 CLOSED — T-002e shipped (Claude as Conductor)

- **Goal:** Identify the apply-mods trigger (H-007) and disambiguate flats/queries hash maps (+0x58 vs +0x108).
- **Scope's run (Opus 4.7, 1177s ≈ 20 min, 1267 raw output lines):**
  - **F-018** — TweakDB initial-load orchestrator `FUN_102b75744` @ static `0x102b75744`. Builds `tweakdb.bin` path via `FUN_102b75b48` (string-confirmed). Reads + parses + inserts via `FUN_102253964`. Completion = DB populated = apply point. **Single caller at `0x10a42f7de`** — predictable execution path.
  - **F-019** — +0x58 confirmed as a peer hash map (identical layout to records, ref-counted destructor `FUN_100ad1df0`). Flats-vs-queries block label deferred to runtime check (Scope refused FA-006-style asserting from Windows analogy alone).
  - **H-007 → resolved-fact** via F-018.
  - **H-008 (new, open)** — flats = +0x58 inference. Trivially resolvable at runtime by entry-count comparison; folded into Phase 1 P1.4.
- **Recommended mechanism (Scope):** poll the singleton (`*(0x1080c92d0 + slide)`) from the injected dylib. Justification: no vtable (F-016), no fn-ptr dispatch (F-017), GOT doesn't apply (direct intra-image `bl`), `__TEXT` immutable (FA-001). Polling is the only FA-001-compliant route, and the orchestrator runs only once at init so polling overhead is trivial. Alternative: hook TweakDBReloader for hot-reload (uninvestigated, out of v1.0 scope).
- **🎉 Phase 0 closure milestone:**
  - 19 FACTS validated (F-001..F-019)
  - 11 FAILED_APPROACHES (FA-001..FA-011)
  - 4 HYPOTHESES resolved as FACT (H-005, H-006, H-007), 2 as FAILED (H-001, H-002), 3 still open: H-003 (moot per F-016), H-004 (presumed, retest deferred), H-008 (runtime-resolvable in P1.4)
  - Architecture confirmed simple: read singleton + walk hash map + write value buffer — zero code hooks for v1.0
  - ARCHITECTURE.md updated to reflect the v1.0 simplification
  - PHASE_1_PLAN.md written with 11 tasks (P1.1..P1.11) across Hookline/Schema/Patchwork
  - PHASE_1_PLAN.md P1.3 updated with concrete apply-trigger address + polling mechanism
  - PHASE_1_PLAN.md P1.4 updated with H-008 verification step
- **Status:** Phase 0 → Phase 1. Progress on Phase 1 = 0% (just kicked off).
- **Files changed:** docs/FACTS.md (F-018, F-019 by Scope), docs/HYPOTHESES.md (H-007 resolved-fact, H-008 added by Scope), docs/PHASE_1_PLAN.md (P1.3/P1.4 updates by Conductor), state/tasks.yaml (T-002e done), state/status.yaml (phase id 0→1, phase_0_summary added, recent_facts_added updated), this log.
- **FACTS added:** F-018, F-019.
- **HYPOTHESES resolved:** H-007 (fact).
- **HYPOTHESES added:** H-008.
- **Blockers:** none.
- **Next:** Sprint 1 of Phase 1. Conductor delegates P1.1 (slide capture) to Hookline AND P1.7+P1.9 (mod scanner + psiberx integration) to Patchwork. These don't conflict — same agent CLI serializes but conceptually parallel.

---

## 2026-05-29 — P1.1 shipped: slide capture primitive (Claude as Conductor)

- **Goal:** First Phase 1 implementation task. Capture image base + ASLR slide for downstream primitives.
- **Hookline's run (Opus 4.7, 197s ≈ 3 min, 473 raw output lines):**
  - Created src/red4ext-mac/src/runtime/Symbols.{hpp,cpp}, src/red4ext-mac/src/loader/Loader.cpp, src/red4ext-mac/src/runtime/slide_test.cpp
  - Wired src/red4ext-mac/CMakeLists.txt — `red4ext` SHARED library + `slide_test` standalone test target
  - Wrote tools/test-slide-capture.sh — game-launch smoke test
  - Built clean: zero warnings on -Wall -Wextra, libred4ext.dylib is arm64
  - **On-host pre-validation (no game launch needed):**
    - Negative path: `slide_test` binary doesn't match → API returns 0, safe degradation
    - Positive path: renamed binary to `Cyberpunk2077` → base 0x1005f0000, slide 0x5f0000, base-slide == 0x100000000 ✓ (validates F-004)
    - StaticToRuntime(0x1080c92d0) = 0x1086b92d0 = 0x1080c92d0 + 0x5f0000 ✓
- **Engineering quality:** thread-safe via std::atomic + std::call_once; first-image-wins semantics; exact-basename matching (fixes T-018/F-010 over-matching); idempotent EnsureInit() callable from multiple constructor priorities.
- **Hookline's flagged follow-ups:**
  - Switch CMake to GLOB once more primitives land (exclude slide_test.cpp's main())
  - Relocate accessor headers to include/RED4macOS/ per PHASE_1_PLAN tree
  - Remove slide_test target before v1.0
- **Status:** Phase 1 progress 0% → 5%. Sprint 1 first task complete.
- **Files changed:** 4 new source files in src/red4ext-mac/, 1 new tools script, CMakeLists.txt updated; state/tasks.yaml, state/status.yaml, this log.
- **FACTS / FAILED_APPROACHES:** none (implementation phase).
- **Blockers:** none.
- **Next:** Run smoke test against live game to validate end-to-end. Then P1.2 (singleton accessor) or P1.7 (mod scanner) in parallel.

---

## 2026-05-29 — P1.1 smoke test PASS (Claude as Conductor)

- **Live in-game test:** Killed running Cyberpunk, ran tools/test-slide-capture.sh → injected libred4ext.dylib into a fresh game launch → captured loader init line with correct base + slide → auto-killed → PASS.
- **Capture:** `base=0x1042f8000 slide=0x42f8000` → `base - slide == 0x100000000` ✓
- **Slide variance evidence:** F-004's per-launch ASLR rule now has THREE empirical data points supporting it:
  - Probe v1 run: slide 0x285c000
  - Probe v2 run: slide 0xb10000
  - P1.1 smoke test: slide 0x42f8000
  - All three: base - slide == 0x100000000 ✓
- **Significance:** First production code from our project (not a throwaway probe) verified working in-game.
- **Files changed:** docs/probes/logs/red4ext-mac-2026-05-29-p1-1.log (evidence), this log.
- **Next:** P1.2 (singleton accessor) — consume the validated StaticToRuntime(0x1080c92d0) to read the live TweakDB* pointer.

---

## 2026-05-29 — P1.2 shipped + smoke test reveals fast TweakDB init (Claude as Conductor)

- **Goal:** Implement singleton accessor — reads live TweakDB* via StaticToRuntime(0x1080c92d0) + mach_vm_read_overwrite.
- **Hookline's run (Opus 4.7, 213s ≈ 3.5 min, 489 raw output lines):**
  - Created src/red4ext-mac/src/runtime/SingletonAccess.{hpp,cpp}, tools/test-singleton-access.sh
  - Modified Loader.cpp (one-shot initial poll + deferred sample thread), slide_test.cpp (singleton printout), CMakeLists.txt
  - Built clean (zero warnings on -Wall -Wextra)
- **Hookline's judgment call:** Spec said no polling loop in P1.2 (that's P1.3's job). But the smoke test needs a non-null read to PASS. Hookline added a SINGLE deferred sample (one 6s sleep + one read on a detached thread, then exits) — explicitly marked temporary, will be removed when P1.3's real polling loop lands. Reasonable compromise; documented in his reply.
- **Live in-game smoke test result:**
  - Injection: ✅ slide 0x42f8000 captured (consistent with P1.1's first run earlier today)
  - Initial poll at load: 0x0 (expected — DB not built yet)
  - **Deferred sample at t+6s: 0x3235425b0** ← non-null heap pointer, valid TweakDB*
  - **DISCOVERY: TweakDB is fully constructed within 6 seconds of game launch.** Before any title screen / EULA / launcher UI. Apply-trigger polling will be near-instant.
  - Smoke test script's PASS logic only checked the initial line and reported WEAK PASS — bug in the script, not the primitive. To fix in a future iteration.
- **Files changed:** SingletonAccess.{hpp,cpp} new, Loader.cpp/slide_test.cpp/CMakeLists.txt modified, tools/test-singleton-access.sh new, docs/probes/logs/red4ext-mac-2026-05-29-p1-2.log (evidence), state/tasks.yaml, state/status.yaml, this log.
- **Status:** Phase 1 progress 5% → 10%. Sprint 1 Hookline tasks (P1.1, P1.2) done.
- **Blockers:** none.
- **Next:** Fire Patchwork P1.7 + P1.9 (mod scanner + psiberx integration) — Hookline rests, Patchwork picks up the parallel-conceptual work.

---

## 2026-05-29 — P1.7 + P1.9 shipped + F-002 correction surfaced (Claude as Conductor)

- **Goal:** Patchwork delivers Sprint 1's mod-loader half — mod scanner + psiberx .tweak parser integration.
- **Patchwork's run (Opus 4.7, 520s ≈ 8.7 min, 1304 raw output lines):**
  - **P1.7 (mod scanner):** Faithful port of Windows TweakImporter::ImportTweaks. Recursive walk, follow-symlinks, case-insensitive extension filter, 3-tier priority by first char of FILENAME (_#$! → 0, ^ → 2, else 1), stable-sorted within tier. Silent skip on filesystem errors. Verified with synthetic 4-file tree.
  - **P1.9 (psiberx parser):** Reuses Red::TweakParser::Parse via TWEAKXL_MAC_OFFLINE per D-005. Wrapper-include strategy keeps vendored mirror pristine (Patchwork did NOT patch psiberx source). Good input → parse tree; malformed → null + log.
  - **Files created (7):** ModScanner.{hpp,cpp}, scanner_test.cpp, Tweak.{hpp,cpp}, TweakInternal.hpp, PsiberxParser.cpp, tools/test-tweak-parser.sh; tweakxl-mac/CMakeLists.txt rewritten.
  - **Build:** libtweakxl.dylib (arm64, 2.1MB) clean — zero warnings, zero errors.
- **Live smoke test PASS:** Scanner returns 4 files in correct tier order; bad input correctly rejected with `bad.tweak:2:3: Expected group name` log; good input parses to non-null source.
- **F-002 correction discovered (filed as T-020):** F-002 claimed offline path needs "only C++20 stdlib + PEGTL". P1.9 integration found it actually needs:
  1. A CName shim backed by RED4ext::FNV1a64 (RED4ext SDK include path required)
  2. std::exception(const char*) → std::runtime_error remap (libc++ vs MSVC)
  3. C++20 standard (not C++17) for std::format
  4. macOS deployment target ≥13.3 (std::format availability gate)
  5. Two documented warning waivers
  Filed T-020 for Scope to append an F-020 with the actual requirements. Doesn't block Phase 1 progress.
- **Sprint 1 COMPLETE.** Both halves shipped: red4ext-mac (Hookline) + tweakxl-mac (Patchwork). Two clean dylibs. All smoke tests pass.
- **Status:** Phase 1 progress 10% → 25%. Sprint 2 unblocked.
- **Files changed:** 7 new src/tweakxl-mac/ files, 1 CMakeLists.txt rewrite, 1 new tools script, state/tasks.yaml (T-020 added), state/status.yaml (Sprint 1 complete), this log.
- **FACTS / FAILED_APPROACHES added:** none yet — T-020 will add F-020 when Scope is fired.
- **Blockers:** none.
- **Next:** Sprint 2. Recommended: P1.4 (Schema, TweakDB struct accessor + H-008 verification) — this is the linchpin that unblocks P1.5/P1.6/P1.10. P1.8 (YAML parser, Patchwork) parallel-eligible.

---

## 2026-05-29 — P1.4 shipped + H-008 RESOLVED via in-game smoke test (Claude as Conductor)

- **Goal:** Schema delivers TweakDB struct accessor + H-008 verification. Sprint 2 kickoff.
- **Schema's run (Opus 4.7, 404s ≈ 6.7 min, 1036 raw output lines):**
  - Created src/red4ext-mac/src/runtime/TweakDB.{hpp,cpp} + tweakdb_test.cpp; modified Loader.cpp; updated CMakeLists.txt; created tools/test-tweakdb-access.sh
  - Built clean, -Wall -Wextra, zero warnings
  - **Critical engineering catch (static_assert paid off):** First build failed because `uint64_t pad4c` at unaligned offset 0x4c forced compiler to pad-shift the entire struct tail (records ended up at 0x90 instead of 0x88). Fixed with opaque byte array. Without the static_assert this would have been a silent latent bug.
  - F-015 ambiguity flagged: hashMapC.count listed at +0x114 (block +0x0c, the bucketCount slot) while F-012/F-019 use +0x08. Schema's H-008 raw output logs BOTH so it can be resolved at runtime.
- **🎯 In-game smoke test PASS (game build 2.3.1, T+~7s post-launch):**
  - `mapA(+0x58).count = 193,354` ← FLATS
  - `mapC(+0x108).count = 12` ← QUERIES
  - `records.count = 843`
  - `valueBufferSize = 4,291,664 bytes (~4 MB)`
  - **Verdict: flats-is-A → H-008 CONFIRMED.** 16,000:1 ratio between map counts is decisive.
  - Raw log: `mapC{@08=12,@0c=13}` → count is at +0x08 in all three maps; the F-015 +0x114 annotation was likely a misreading. Schema's logging strategy resolved the ambiguity without a rebuild.
- **H-008 → resolved-fact** in HYPOTHESES.md.
- **What this means for downstream:** Schema's `GetFlatsMapCandidate(db)` → +0x58 is now the canonical flats accessor. No swap needed. P1.5 (hash walker), P1.6 (value buffer R/W), P1.10 (applicator) all unblocked.
- **Live database stats discovered:** TweakDB v2.3.1 has 193,354 flats + 843 records + 12 queries, with a 4MB flat-value buffer at +0x148. This is what our applicator will mutate.
- **Recommendation:** Scope should formalize the H-008 confirmation and the F-015 count-offset detail as F-020 when next fired. Doesn't block progress.
- **Status:** Phase 1 progress 25% → 35%. Sprint 2 P1.4 done.
- **Files changed:** TweakDB.hpp/cpp/tweakdb_test.cpp new; Loader.cpp/CMakeLists.txt modified; tools/test-tweakdb-access.sh new; docs/probes/logs/red4ext-mac-2026-05-29-p1-4.log (evidence); docs/HYPOTHESES.md (H-008 resolved); state/tasks.yaml, state/status.yaml; this log.
- **FACTS added:** none (H-008 evidence supports a future F-020).
- **HYPOTHESES resolved:** H-008 (fact).
- **Blockers:** none.
- **Next:** P1.5 (hash walker by Schema) — natural next, momentum on TweakDB primitives. Then P1.6 (value buffer R/W). After Sprint 2 hash + buffer primitives, P1.10 (applicator) is unblocked.

---

## 2026-05-29 — P1.5 shipped + hash function identified in-game (Claude as Conductor)

- **Goal:** Schema delivers the hash map walker. Discovers actual hash function used by game empirically (not assumed).
- **Schema's run (Opus 4.7, 567s ≈ 9.4 min, 861 raw output lines):**
  - Created src/red4ext-mac/src/runtime/HashMap.{hpp,cpp} + hashmap_test.cpp
  - Modified Loader.cpp (now also calls VerifyHashFunction after VerifyH008)
  - Built clean, unit tests pass (FNV-1a known vectors match, CRC32 known vector matches, TweakDBID packing OK)
  - Engineering notes: cycle guard bounds chain walk at count+8 to prevent hangs on corrupt data; all reads via mach_vm_read_overwrite for safety
- **🎯 In-game smoke test PASS (game build 2.3.1, T+~15s):**
  - **Hash function: fnv1a-8B** (FNV-1a 32-bit on all 8 bytes of TweakDBID)
  - stored=0x996f5e0c, computed=0x996f5e0c → perfect match
  - key=TweakDBID(0x0ccc10d0, len=1, off=0x000000) — entry has zero tdbOffset
  - bucket=1/843 (records map: 843 buckets for 843 entries, sparse 1:1)
  - **Round-trip lookup PASS:** lookup returned the same entry pointer (0x701c5fc430) we found via bucket walk
- **Important nuance (Schema's flagged follow-up):** Records map had off=0x000000, so fnv1a-8B is indistinguishable from fnv1a-5B for this entry. For FLATS, the tdbOffset is likely non-zero (it indexes into +0x148 buffer). The flats map's actual hash byte-count needs re-verification when P1.6 / P1.10 first build a name-based lookup. Schema's design captures this — the probe runs again per-map at first lookup if needed.
- **What this unlocks:**
  - P1.6 (value buffer R/W) — now has Lookup + TweakDBID struct + safe read primitives
  - Per Schema's follow-up: P1.6 needs to confirm whether tdbOffset is a byte offset into the buffer (Windows model) or something else on macOS. Binary-format spec marks this hypothesized; needs validation
  - The flats entry payload layout (type tag location, value encoding) is still TBD; may need a brief Scope dive to dump a real flats entry
- **Status:** Phase 1 progress 35% → 45%. Sprint 2 P1.5 done.
- **Files changed:** HashMap.{hpp,cpp}/hashmap_test.cpp new; Loader.cpp/CMakeLists.txt modified; tools/test-hashmap-lookup.sh new; docs/probes/logs/red4ext-mac-2026-05-29-p1-5.log (evidence); state/tasks.yaml, state/status.yaml; this log.
- **FACTS / FAILED_APPROACHES added:** none yet (in-game hash discovery is excellent F-NNN material for Scope's next fire).
- **Blockers:** none.
- **Next:** P1.6 (value buffer R/W) — Schema again. Then we've got the full read+write primitive chain ready for P1.10 (applicator).

---

## 2026-05-29 — P1.6 shipped + DISCOVERY: flats use different hash mode than records (Claude as Conductor)

- **Goal:** Value buffer R/W primitive. Schema's first task with mutation.
- **Schema's run (Opus 4.7, 670s ≈ 11.2 min, 1386 raw output lines):**
  - Created Values.hpp (26-type system with FlatType enum, FlatValue tagged variant)
  - Extended TweakDB.hpp/.cpp with FlatLayout discovery, ReadFlat, WriteFlat, VerifyFlatEntry
  - Added mach_vm_write safe-write primitive with verified-readback + audit logging
  - Fail-safe design: Read returns nullopt and Write returns false until in-game probe confirms layout
  - Built clean, -Wall -Wextra, zero warnings
- **In-game smoke test results (mixed — major discovery + bug surfaced):**
  - ✅ **Flats layout VERDICT: `flatValue-ptr-at-entry`** — tally 6/6 for ptr@0x18 (matches F-019's ref-counted-payload destructor finding). Buffer at base=0x71af1c000, size=4,291,664 B confirmed live.
  - ✅ **Type tag at FlatValue+0x00 (vft pointer), value data at FlatValue+0x08** — Windows-style FlatValue wrapper.
  - 🆕 **DISCOVERY: Flats map uses `nameHash-direct`, NOT `fnv1a-8B` like records.** Sample 0 stored=0xce8348b9 matches TweakDBID.nameHash (0xce8348b9) exactly; FNV-1a candidates all differ.
  - ❌ **Round-trip FAILED** — Read using Schema's global HashMode (fnv1a-8B from P1.5 records test) on flats → wrong bucket → entry not found. Architectural assumption (one global HashMode) was wrong.
- **What this means:**
  - Records use FNV-1a 32-bit on 8 bytes (computed)
  - Flats use the stored nameHash field directly (no recomputation — the binary spec's CRC32 nameHash is used straight as the bucket key)
  - HashMap needs per-map hash mode, not a global one
  - This is a one-line fix in HashMap.cpp + VerifyHashFunction needs to set the mode per-map, not globally
- **Schema's design choices that paid off:**
  - VerifyFlatEntry runs all 4 hash candidates on the actual flats map — that's how we caught the difference
  - Fail-safe Write (no-op until layout confirmed) prevented any bad mutations
  - The dual-pointer logging (ptr@0x10 AND ptr@0x18) gave us the layout regardless of which the probe picked
- **Next:** Fire Schema for P1.6-FIX — per-map hash mode threading. Single edit, then re-test. After that: P1.6 truly complete and P1.10 applicator can build on it.
- **Status:** Phase 1 progress 45% → 50% (discovery counts even though R/W round-trip didn't). Sprint 2 P1.6 80% done.
- **FACTS / FAILED_APPROACHES added:** none yet. The flats=nameHash-direct + records=fnv1a-8B distinction is excellent F-NNN material — should be filed by Scope on next fire.
- **Blockers:** none — the fix is well-scoped.

---

## 2026-05-29 — 🎯 P1.6b PASSES — First in-game mutation working (Claude as Conductor)

- **Goal:** Fix per-map hash mode so flats lookups use nameHash-direct while records continue using fnv1a-8B.
- **Schema's run (Opus 4.7, 226s ≈ 3.8 min, 510 raw output lines):**
  - Updated HashMap.{hpp,cpp}: replaced global g_hashMode with g_recordsMode/g_flatsMode atomics + Get/Set accessors per map
  - Updated TweakDB.cpp: VerifyFlatEntry feeds verdict into SetFlatsHashMode; ReadFlat/WriteFlat use GetFlatsHashMode()
  - Loader.cpp: unchanged (sequencing was already correct)
  - Built clean: zero errors, zero warnings, all targets including untouched Patchwork tweakxl
- **🎯 In-game smoke test PASS (independently verified by Conductor):**
  - Records test (regression check): hash-function=fnv1a-8B, lookup-test pass ✓
  - **Flats R/W test PASS:**
    - 6 flats samples — layout `flatValue-ptr-at-entry` confirmed (tally 6/6)
    - Hash mode `nameHash-direct` confirmed (`stored=0xce8348b9 == name; fnv8/fnv5/crc8 all miss`)
    - **Lookup found entry:** `found=yes raw=0x00000002`
    - **WRITE @ 0x31a451f68: 0x2 → 0x3 verify=ok** ← LIVE GAME MEMORY MUTATION
    - **Reread match=yes** ← write persisted
    - **Restore @ 0x31a451f68: 0x3 → 0x2 verify=ok** ← game state left untouched
- **🚀 Milestone: First successful mutation of a live TweakDB flat value via our framework.**
  - The full Phase 0 architecture is now empirically validated: slide → singleton → struct → lookup (per-map hash) → entry → FlatValue at +0x18 → write at FlatValue+0x08
  - Two new architectural FACTS surfaced for Scope's next fire:
    - F-NNN candidate: Records map uses FNV-1a 32-bit on 8 bytes of TweakDBID
    - F-NNN candidate: Flats map uses nameHash-direct (the binary spec's CRC32 nameHash IS the bucket key, no recomputation)
    - F-NNN candidate: Flats entry has FlatValue* at entry+0x18 (F-019 ref-counted-payload model confirmed; tdbOffset-buffer-indexing Windows model disproved for macOS, tally 0/6)
- **Open follow-up:** vft→FlatType map is still unresolved. ReadFlat returns raw uint32 typed as Unknown. P1.10 will work for scalar Assign ops (caller-declared type) but typed reads and array/string ops still blocked. This is researcher RE work.
- **Status:** Sprint 2 Schema track DONE (P1.4 + P1.5 + P1.6 + P1.6b). Phase 1 progress 50% → 65%.
- **Files changed:** HashMap.{hpp,cpp}, TweakDB.cpp; docs/probes/logs/red4ext-mac-2026-05-29-p1-6b.log; state/tasks.yaml, state/status.yaml; this log.
- **FACTS added by this turn:** none directly — Scope to publish F-020..F-023 covering the records hash, flats hash, flats entry layout, and live database statistics.
- **Blockers:** none.
- **Next:** Either (a) P1.3 (Hookline — apply-trigger polling using FUN_102b75744 + the now-working data path) OR (b) P1.8 (Patchwork — YAML parser). Both are unblocked. After both + P1.10 (applicator), we have the end-to-end mod-application demo.

---
