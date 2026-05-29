# Phase 1 Implementation Plan — v1.0 TweakXL Foundation

**Status:** DRAFT (pending T-002e completion)
**Owner:** Conductor → distributed to Hookline / Schema / Patchwork

This document is the handoff from Phase 0 (research) to Phase 1 (engineering). It defines the actual code to write, who writes what, dependencies between pieces, and the order things should land.

It is built on the Phase 0 facts (F-001..F-017, FA-001..FA-011). If a Phase 1 task seems to contradict a Phase 0 fact, **stop** — the plan is wrong, not the facts.

---

## What Phase 0 Settled

These are the load-bearing FACTS that the implementation plan rests on. Any change to these invalidates parts of the plan.

| FACT | Summary | Where it affects Phase 1 |
|------|---------|--------------------------|
| F-001 / F-003 | Stock 2.3.1 binary accepts `DYLD_INSERT_LIBRARIES` without re-sign | Distribution: no re-sign step in installer for v1.0 |
| F-004 / F-007 | `runtime = static + slide`; slide varies per launch | Every runtime address computation in our code |
| F-011 | Global `TweakDB*` at `0x1080c92d0` in `__bss`; getter `FUN_102b73c7c`; initializer `FUN_102b73b50`; constructor `FUN_102b73db8` | Singleton-access primitive in red4ext-mac |
| F-013 | TweakDB struct = 0x168 bytes; records are plain hash table (not fn-ptr dispatch) | Applicator design — direct memory writes |
| F-015 | Full struct field map: 3 hash maps at +0x58/+0x88/+0x108, value buffer at +0x148, etc. | TweakDB accessor primitives in red4ext-mac |
| F-016 | TweakDB has NO vtable | Rules out vtable-hook approach; nothing to install in vtable |
| F-017 | Flats use plain hash table + value buffer (NOT fn-ptr dispatch). H-001 disproved. | Applicator: walks hash map, writes value buffer — no code hooks |
| FA-001 | `__TEXT` is immutable under code signing | No inline patching anywhere |
| FA-002 | No offline patching | Runtime hooking only (D-001) |
| D-005 | Reuse psiberx `.tweak` parser via `TWEAKXL_MAC_OFFLINE` | Patchwork doesn't write a `.tweak` parser from scratch |

**One unknown remaining (T-002e):**
- Which hash map (`+0x58` or `+0x108`) is flats vs queries
- The apply-mods trigger point (H-007)

The implementation can largely proceed assuming the Windows-analogy answer (`+0x58` = flats), with T-002e providing confirmation before mutation code is enabled.

---

## v1.0 Component Breakdown

For v1.0, we ship a single dylib (`libtweakxl-mac.dylib`) that contains both the runtime primitives and the mod-loader. The framework / plugin split (`red4ext-mac` ≠ `tweakxl-mac`) is preserved in source structure for v1.x but is one binary at runtime.

```
src/
├── red4ext-mac/
│   ├── src/
│   │   ├── loader/Loader.cpp              (dylib entry, slide capture)
│   │   ├── runtime/Symbols.cpp            (Mach-O parse, image base/slide)
│   │   ├── runtime/SingletonAccess.cpp    (read global TweakDB*)
│   │   ├── runtime/TweakDB.cpp            (struct accessors at F-015 offsets)
│   │   ├── runtime/ApplyTrigger.cpp       (hook the populate-complete point)
│   │   └── runtime/HashMap.cpp            (FNV-1a key → walk bucket array → entry)
│   └── include/RED4macOS/                 (public headers)
│
└── tweakxl-mac/
    └── src/
        ├── plugin/Plugin.cpp              (orchestration; reads runtime, calls applicator)
        ├── parsers/YAML.cpp               (yaml-cpp → op AST)
        ├── parsers/Tweak.cpp              (psiberx PEGTL via TWEAKXL_MAC_OFFLINE)
        ├── model/Operation.hpp/cpp        (op AST types)
        ├── applicator/Applicator.cpp      (op → TweakDB mutation)
        └── loader/ModScanner.cpp          (walks r6/tweaks/, dispatches to parsers)
```

---

## Task Assignments

### 🪝 Hookline (cp2077-hook-engineer) — Runtime Primitives

**P1.1 — Slide capture primitive** (1-2 hours of agent time)
- File: `src/red4ext-mac/src/loader/Loader.cpp`, `src/red4ext-mac/src/runtime/Symbols.cpp`
- Behavior:
  - `__attribute__((constructor))` fires at dylib load
  - Registers `_dyld_register_func_for_add_image` to detect Cyberpunk2077 main image load
  - On detection, computes and stores image base + slide
  - Exposes accessor: `IntPtr GetSlide()` and `IntPtr GetImageBase()`
- Largely reusable from `src/red4ext-mac/src/probes/h001_probe.cpp` (Hookline's own existing code)
- Acceptance: a unit test that injects the dylib, prints base + slide, exits

**P1.2 — Singleton-access primitive** (30 min)
- File: `src/red4ext-mac/src/runtime/SingletonAccess.cpp`
- Behavior:
  - `TweakDB* GetTweakDB()` → returns `*(0x1080c92d0 + slide)` if non-null, else nullptr
  - Safe: uses `mach_vm_read_overwrite` to never crash if pointer is invalid
- Acceptance: when the dylib is injected at game launch, polling returns null until populate completes, then returns the live pointer

**P1.3 — Apply-trigger hook** (1-3 hours, depends on T-002e's findings)
- File: `src/red4ext-mac/src/runtime/ApplyTrigger.cpp`
- Approach depends entirely on T-002e:
  - If T-002e finds a callback registry → register our callback
  - If T-002e finds an observable side-effect (e.g., a flag flipping) → polling thread
  - If T-002e finds a vtable-less indirect call → GOT-style hook on it
- Acceptance: a callback fires exactly once per game session, after TweakDB is fully populated, before any gameplay reads it

### 🗄️ Schema (cp2077-tweakdb-engineer) — TweakDB Accessor Primitives

**P1.4 — TweakDB struct accessor** (2-3 hours)
- File: `src/red4ext-mac/src/runtime/TweakDB.cpp`
- Behavior:
  - Wraps the raw `TweakDB*` from P1.2 with typed accessors
  - All offsets per F-015: hash maps at +0x58/+0x88/+0x108, value buffer at +0x148
  - Methods: `GetRecordsMap()`, `GetFlatsMap()`, `GetQueriesMap()`, `GetValueBuffer()`, `GetValueBufferSize()`, `GetValueBufferCapacity()`
- No mutation methods at this layer — mutation happens in the applicator
- Acceptance: dump the running TweakDB's records map count and value buffer size; verify both are non-zero after game's TweakDB load

**P1.5 — Hash map walker** (2 hours)
- File: `src/red4ext-mac/src/runtime/HashMap.cpp`
- Behavior:
  - Given a hash map base and a TweakDBID, walks the bucket-index array to find the entry
  - FNV-1a hashing (per F-012)
  - Returns pointer to the entry, or nullptr if not found
- Acceptance: look up a known TweakDBID (e.g., `Items.Preset_Ajax_Default`) in the records map; assert the found entry's hash matches

**P1.6 — Value buffer reader/writer** (1-2 hours)
- File: `src/red4ext-mac/src/runtime/TweakDB.cpp` (extends P1.4)
- Behavior:
  - Read/write typed values at offsets within the value buffer at +0x148
  - Type-tag handling per the Windows TweakDB type system (22 types per `docs/reference/tweakdb-binary-format.md`)
- Acceptance: read a known flat value (e.g., `Player.maxHealth`); modify it; verify the game observes the new value

### 🧩 Patchwork (cp2077-mod-loader) — Parsers + Applicator

**P1.7 — Mod scanner** (1 hour)
- File: `src/tweakxl-mac/src/loader/ModScanner.cpp`
- Behavior:
  - Recursively walks `r6/tweaks/` for `.yaml`, `.yml`, `.tweak` files
  - Sort tier: `_#$!` first, `^` last, others middle (matches Windows behavior per `windows-tweakxl-api.md`)
  - Dispatches each file to the right parser

**P1.8 — YAML parser** (3-4 hours)
- File: `src/tweakxl-mac/src/parsers/YAML.cpp`
- Behavior:
  - yaml-cpp dep
  - Top-level mapping → entries (flat assignments + record blocks)
  - Eight `$`-attributes: `$type`, `$value`, `$base`, `$props`, `$game`, `$dlc`, `$instances`
  - Nine array mutation tags: `!append`, `!append-once`, `!append-from`, `!prepend`, `!prepend-once`, `!prepend-from`, `!merge`, `!remove`, `!remove-all`
  - Output: op AST (`model/Operation.hpp`)
- Acceptance: parse `reference/example-mods/` YAML samples; produce expected op AST

**P1.9 — `.tweak` parser via psiberx (D-005)** (1 hour)
- File: `src/tweakxl-mac/src/parsers/Tweak.cpp`
- Behavior:
  - `#define TWEAKXL_MAC_OFFLINE` in build
  - Include `reference/windows-tweakxl/src/Red/TweakDB/Source/Parser.hpp` etc.
  - Adapt psiberx's `TweakSource` output → our `Operation` AST
- Acceptance: parse a `.tweak` file from `reference/example-mods/`; produce equivalent op AST as Windows

**P1.10 — Applicator** (3-4 hours)
- File: `src/tweakxl-mac/src/applicator/Applicator.cpp`
- Behavior:
  - Takes op AST + `TweakDB*` (from Schema's primitives)
  - For each op:
    - Compute TweakDBID hash
    - Walk appropriate hash map
    - For Assign on flat: write to value buffer at the entry's offset
    - For Append/Remove on array flat: realloc + memmove the array within the buffer
    - For record cloning: replicate entry into records map with new ID
  - Atomic per-mod: all-or-nothing
- Acceptance: apply a real Nexus mod from `reference/example-mods/`; verify the in-game stat change matches Windows behavior

**P1.11 — Plugin orchestrator** (1 hour)
- File: `src/tweakxl-mac/src/plugin/Plugin.cpp`
- Behavior:
  - Registers apply-trigger callback (uses P1.3)
  - On trigger: gets singleton (P1.2), scans mods (P1.7), parses (P1.8 + P1.9), applies (P1.10)
  - Logs results

### 🔍 Sieve (cp2077-reviewer)
Reviews each PR before merge. No code written.

### 🎮 Crashlog (cp2077-tester)
In-game tests after each component lands. Reports back via session log.

### 📒 Ledger (cp2077-doc-keeper)
Updates `state/status.yaml` after each task completes. Reconciles any new contradictions.

---

## Dependency Graph

```
P1.1 (slide) ──┬─→ P1.2 (singleton) ──┬─→ P1.4 (struct) ──→ P1.5 (hashmap)
               │                       │
               │                       └─→ P1.6 (value buf)
               │
               └─→ P1.3 (apply-trigger) ──┐
                                          │
P1.7 (scanner) ──→ P1.8 (YAML) ──┐        │
                                  │        │
                P1.9 (psiberx) ───┴→ P1.10 (applicator) ──→ P1.11 (plugin)
                                                              ↑
                                                              │
                                                  P1.6 + P1.3 ┘
```

**Parallelism opportunities:**
- P1.1 (Hookline) + P1.7/P1.8/P1.9 (Patchwork) can land in parallel — different agents, different files
- P1.4/P1.5/P1.6 (Schema) can land after P1.2 but before P1.10

**Critical path:** P1.1 → P1.2 → P1.4 → P1.6 → P1.10 → P1.11

---

## Order of Operations

### Sprint 1 (Foundation primitives — 1-2 sessions)
1. P1.1 — slide capture (Hookline)
2. P1.2 — singleton access (Hookline)
3. P1.7 — mod scanner (Patchwork)
4. P1.9 — psiberx integration (Patchwork)
5. Crashlog tests: confirm singleton reads, parser runs on samples

### Sprint 2 (TweakDB access + YAML)
6. P1.4 — struct accessor (Schema)
7. P1.5 — hash map walker (Schema)
8. P1.8 — YAML parser (Patchwork)
9. Crashlog tests: hash map lookup of known TweakDBID, YAML parses

### Sprint 3 (Mutation + apply trigger)
10. P1.3 — apply trigger (Hookline) — gated on T-002e
11. P1.6 — value buffer R/W (Schema)
12. P1.10 — applicator (Patchwork)
13. Crashlog tests: modify a flat in memory, verify game observes it

### Sprint 4 (Integration + real mods)
14. P1.11 — plugin orchestrator (Patchwork)
15. End-to-end test: drop a Nexus mod, verify in-game behavior
16. Sieve reviews everything before main merge

### Sprint 5 (Stability + release)
17. 1-hour stability test (Crashlog)
18. Bug fixes
19. User install guide (Ledger)
20. v1.0 release tag

---

## What Done Looks Like (v1.0 Acceptance)

- [ ] 5+ real Nexus TweakXL mods load and apply unchanged
- [ ] In-game stat changes match Windows TweakXL behavior
- [ ] No crashes during 1-hour play session
- [ ] User can install with: drop dylib in known location + edit env var (no re-sign required for stock 2.3.1)
- [ ] `docs/USER_GUIDE.md` exists and works for someone who isn't Lucas

---

## What Phase 1 Explicitly Excludes

| Excluded | Where it lands |
|----------|---------------|
| ArchiveXL parity (custom assets) | v1.x |
| CET (Lua scripting + ImGui-on-Metal) | v2.0 |
| Codeware / redscript runtime | v2.0 |
| Plugin SDK for third-party dylibs | v1.x |
| MAP_JIT trampolines / inline hooks | v1.x or v2.0 |
| Mod manager UI | Never (CLI tooling only for now) |

---

## Open Questions Before Sprint 1 Starts

1. **T-002e outcome** — flats are at +0x58 or +0x108? Apply-trigger function address?
2. **Build system** — CMake config for the `TWEAKXL_MAC_OFFLINE` flag + psiberx include path needs first iteration
3. **Test harness** — Crashlog needs a reproducible "load this mod, observe this in-game" runbook

These do not block Sprint 1's primitives (P1.1, P1.2). They block P1.10's applicator.

---

## Living Document

This plan will be updated after T-002e lands. Expected revisions:
- Concrete address for apply-trigger in P1.3
- Confirmation of which hash map is flats (or revised offsets if Scope finds something unexpected)
- Maybe a couple of new gotchas discovered along the way
