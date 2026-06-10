# DECISIONS.md — Architectural Decision Records

**Owner:** any agent may add an ADR; `doc-keeper` reviews for completeness.

ADRs are dated, immutable. Don't edit a decision — supersede it with a new entry.

---

## Format

```markdown
### D-NNN: <one-line title>

- **Date:** YYYY-MM-DD
- **Status:** proposed | accepted | superseded by D-MMM
- **Context:** <what problem are we solving>
- **Decision:** <what we chose>
- **Alternatives considered:** <list with brief reason for rejection>
- **Consequences:** <what this means for the codebase>
```

---

## Entries

### D-001: Runtime hooking, not offline patching

- **Date:** 2026-05-28
- **Status:** accepted
- **Context:** We need to apply mods to TweakDB and other game state. Two architectures considered: (a) offline patch the on-disk binaries before launch, (b) hook the game at runtime and modify in-memory state.
- **Decision:** Runtime hooking via a `DYLD_INSERT_LIBRARIES` injected dylib (red4ext-mac), with TweakXL-style mods applied at TweakDB init time.
- **Alternatives considered:**
  - Offline patching of `tweakdb.bin` — rejected because it can't support RED4ext plugins, can't handle mod load order/dependencies cleanly, and runs into binary format compatibility issues (see FA-002).
- **Consequences:**
  - Users must re-sign the game binary with `disable-library-validation`.
  - We need a full hooking infrastructure (red4ext-mac) before any mod can run.
  - Compatibility with Windows RED4ext plugin authors (recompile only, no source changes) is achievable.

---

### D-002: GOT / VTable / function-pointer-table hooking only — no inline hooking

- **Date:** 2026-05-28
- **Status:** accepted
- **Context:** ARM64 inline hooking would let us patch any function at any offset, but macOS code signing blocks `__TEXT` writes.
- **Decision:** All hooks must target `__DATA` segment: GOT entries, vtables, function-pointer tables, ad-hoc function pointers. No inline `__TEXT` modification.
- **Alternatives considered:**
  - Inline hooking via `mprotect` — rejected, see FA-001.
  - Disabling SIP / running unsigned — rejected, raises the user bar too high and breaks Hardened Runtime guarantees for non-modded apps.
- **Consequences:**
  - Some game functions may be un-hookable if they're never accessed through an indirect call.
  - Need to find alternative dispatch points for direct calls.
  - Trampolines for original-call preservation still need executable memory — requires `MAP_JIT` + `allow-jit` entitlement.

---

### D-003: C++17 baseline

- **Date:** 2026-05-28
- **Status:** accepted
- **Context:** Need to pick a C++ standard for the entire project.
- **Decision:** C++17 minimum. May use C++20 features case-by-case if justified (record here when adopting).
- **Alternatives considered:**
  - C++20 from the start — rejected because some embedded SDK consumers may build with older toolchains.
  - C++14 — rejected because we want `std::variant`, structured bindings, `std::filesystem`, `if constexpr`.
- **Consequences:**
  - SDK headers must compile clean as C++17.
  - Plugins built against the SDK can use newer standards internally.

---

### D-004: Multi-agent project layout (OpenClaw convention)

- **Date:** 2026-05-28
- **Status:** accepted
- **Context:** Project is too large for a single agent / single session. User wants handoff between Claude Code, Codex, and other agents under OpenClaw orchestration.
- **Decision:** Adopt the OpenClaw multi-agent convention:
  - `AGENTS.md` at repo root as universal entry point.
  - `.openclaw/agents/*.md` for per-agent role contracts.
  - `state/status.yaml`, `state/tasks.yaml`, `state/session-log.md` as machine-parseable shared state.
  - Three-tier truth model: `FACTS.md` / `HYPOTHESES.md` / `FAILED_APPROACHES.md`.
- **Alternatives considered:**
  - Claude-only with CLAUDE.md — rejected because not portable to other agents.
  - Free-form docs — rejected because prior sessions show docs drift and contradict without enforced structure.
- **Consequences:**
  - All agents (including the user) must respect file-ownership rules.
  - Reading overhead at session start is higher (read AGENTS.md + HANDOFF.md + relevant docs) but compensated by reduced re-discovery cost.

---

### D-005: Reuse psiberx's `.tweak` parser via `TWEAKXL_MAC_OFFLINE`

- **Date:** 2026-05-29
- **Status:** accepted (approved by Lucas 2026-05-29)
- **Context:** F-002 confirmed that the `TWEAKXL_MAC_OFFLINE` build flag in psiberx's Windows TweakXL repo cleanly separates the `.tweak` parser layer (Source / Grammar / Parser) from the Windows runtime layer (Buffer / Manager / Reflection). With the flag defined, the parser compiles with only C++20 stdlib + PEGTL — both already vendored in psiberx's repo.
- **Decision:** Mod-loader (Patchwork) will reuse `reference/windows-tweakxl/src/Red/TweakDB/Source/Parser.cpp` directly with `TWEAKXL_MAC_OFFLINE` defined, instead of porting the PEGTL grammar from scratch. YAML parsing remains our own work (separate yaml-cpp layer).
- **Alternatives considered:**
  - Port `.tweak` PEGTL grammar from scratch — rejected as duplicative effort with no compatibility benefit (would have to match psiberx behavior exactly anyway).
  - Fork the Windows TweakXL repo and maintain a divergent copy — rejected; the upstream source-with-flag approach is lower maintenance.
- **Consequences:**
  - Mod-loader's `.tweak` support is reduced to "wire up the existing source + provide CMake glue."
  - Build-time dependency on `reference/windows-tweakxl/` being present (already symlinked into our repo).
  - Future psiberx updates to the parser flow through automatically when we re-pull the reference repo.
  - YAML parsing is still our work — the offline flag does not cover the YAML layer.
  - If psiberx removes or breaks the `TWEAKXL_MAC_OFFLINE` path upstream, we fall back to vendoring our own snapshot.
- **Validates:** F-002.

---

### D-006: Scope slicing — narrow v1.0, broad long-term, explicit roadmap

- **Date:** 2026-05-29
- **Status:** accepted
- **Context:** "Make Windows Cyberpunk 2077 mods work on macOS" covers a wide spectrum: TweakXL (data tweaks), ArchiveXL (custom assets), CET (Lua scripting + ImGui), Codeware / redscript (scripting extensions). Trying to land all of these in v1.0 would push first usable release months out. Trying to land none of them post-v1.0 would betray the project's stated mission. Need a clear way to express both at once.
- **Decision:** v1.0 stays narrow — RED4ext-mac framework solid + TweakXL working + ≥5 Nexus TweakXL mods verified + stable for 1 hr of play. Post-v1.0 milestones are sliced explicitly and documented in `ARCHITECTURE.md` as a "Long-Term Roadmap" section. The full Windows mod ecosystem is the **long-term goal**; TweakXL is the **first deliverable**.
- **Alternatives considered:**
  - All-in v1.0 (TweakXL + ArchiveXL + CET + redscript) — rejected; pushes first release past viable timeline, dilutes feedback signal from real users.
  - v1.0 = TweakXL only with no public roadmap — rejected; undersells the long-term vision and gives modders/users no expectation of what's coming.
  - Build CET first (highest-impact mod target) — rejected; CET depends on a working RED4ext plugin system, and TweakXL is a smaller surface to validate the framework.
- **Consequences:**
  - v1.0 scope in `state/status.yaml.v1_progress` stays as-is (5 criteria).
  - New `long_term_vision` field added to `status.yaml` listing v1.x / v2.x scopes.
  - `ARCHITECTURE.md` gets a "Long-Term Roadmap" section.
  - `AGENTS.md` mission statement makes the full-ecosystem goal explicit while keeping v1.0 as the immediate target.
  - Researcher prioritization continues to favor TweakXL-blocking unknowns (H-001..H-004) over CET / ArchiveXL preparation work.

---

### D-007: Sequence after F-044 — live-edit tool first, then native textures, then TweakDB parity, then hooking, then scripting

- **Date:** 2026-06-10
- **Status:** accepted (chosen by Lucas 2026-06-10)
- **Context:** F-044 produced the first visible, verified macOS in-game change via a **direct live-memory write** (money 310,915→400), while the TweakDB-flat path stayed walled (F-043/FA-016) and derived stats proved unreachable by value-scan (FA-017/18/19). Exploration also found the native archive mod path (`archive/pc/mod/`) already present on the macOS install. With several viable tracks open (live-write tool, native-archive textures, TweakDB→gameplay parity, the `__DATA` hooking primitive, scripting), we needed an explicit order.
- **Decision:** Sequence the work as recorded in `docs/ROADMAP.md`: **Phase 2 — productize the live-edit tool** (stored counters: money/components/points/XP; set-value + revert; no-crash validated writes) **first**, then Phase 3 native-archive textures, then Phase 4 TweakDB→gameplay parity, then Phase 5 the `__DATA` hooking foundation, then Phase 6 scripting (redscript, CET).
- **Alternatives considered:**
  - **Textures/assets first** — likely the highest payoff/effort (native path appears pre-wired), but unvalidated; deferred to Phase 3 so we ship the *proven* capability first.
  - **TweakDB gameplay parity first** — the original v1.0 promise, but the hardest (materialized-structure RE, FA-016) with uncertain payoff; deferred to Phase 4.
  - **Hooking foundation first** — the central long-term unlock, but most infrastructure for the slowest near-term payoff; deferred to Phase 5.
- **Consequences:**
  - New `docs/ROADMAP.md` (strategic narrative) + `docs/PHASE_2_PLAN.md` (task-level) are the source of truth for sequencing; `ARCHITECTURE.md` and `status.yaml` point to them.
  - `status.yaml:next_action` becomes Phase 2 (live-edit tool); the RAM-ID line is retired (superseded by FA-017/18/19 + F-044).
  - The live-edit tool is positioned honestly as a *native trainer-class capability + Phase-4 stepping stone*, not Windows-mod-file parity.
- **References:** F-044, FA-016/17/18/19, D-006.
