# ARCHITECTURE.md

> **Phase 0 simplified the architecture significantly.** The original design assumed a full GOT/VTable/fn-ptr-table hooking framework would be needed. Phase 0 research (F-011..F-017) discovered that for **v1.0 (TweakXL)**, none of that is required — TweakDB on macOS is plain data accessible via a global singleton pointer.
>
> The full hooking framework is still required for **v1.x (ArchiveXL)** and **v2.0 (CET/redscript)** — see the Long-Term Roadmap. v1.0 is a much smaller surface than originally planned.

## v1.0 System Overview (Simplified — Post Phase 0)

```
┌─────────────────────────────────────────────────┐
│              User-Installed Mods                │
│       (Nexus Mods, .yaml / .tweak files)        │
└──────────────────────┬──────────────────────────┘
                       ↓
┌──────────────────────▼──────────────────────────┐
│              tweakxl-mac (dylib)                │
│  - Reads global TweakDB* @ 0x1080c92d0 + slide  │
│  - Hooks DB-populate completion (T-002e finds)  │
│  - Parses r6/tweaks/ → op AST                   │
│  - Applies ops: walks hash maps, writes value   │
│    buffer at TweakDB+0x148                      │
└──────────────────────┬──────────────────────────┘
                       ↓
┌──────────────────────▼──────────────────────────┐
│      red4ext-mac (minimal v1.0 runtime)         │
│  - Mach-O symbol resolution (slide capture)     │
│  - Singleton-pointer accessor                   │
│  - Single hook on apply-trigger (approach TBD   │
│    by T-002e — possibly observed callback)      │
│  NO GOT hooks, NO VTable hooks, NO fn-ptr-table │
│  hooks, NO trampolines, NO MAP_JIT for v1.0.    │
└──────────────────────┬──────────────────────────┘
                       ↓
┌──────────────────────▼──────────────────────────┐
│       Stock Cyberpunk 2077.app (2.3.1)          │
│  Stock binary already has the two entitlements  │
│  needed for DYLD injection (F-001, F-003).      │
│  NO re-signing required for v1.0.               │
└─────────────────────────────────────────────────┘
```

## v1.0 Runtime Flow

1. User launches game via Steam (no special wrapper needed if `DYLD_INSERT_LIBRARIES` is set in environment, or via a launcher script in `tools/`).
2. `dyld` loads `Cyberpunk2077` then `libtweakxl-mac.dylib` (the all-in-one v1.0 dylib).
3. Dylib constructor fires:
   - Captures slide via `_dyld_register_func_for_add_image`
   - Records image base for the global pointer computation
4. Dylib registers a wait/hook for the DB-populate completion (mechanism per T-002e).
5. Game initializes TweakDB (`FUN_102b73b50`) — populates from `r6/cache/tweakdb.bin`.
6. On populate complete: our trigger fires.
   - Read global TweakDB* via `*(0x1080c92d0 + slide)`
   - Scan `r6/tweaks/` for mod files
   - Parse each (YAML via yaml-cpp, `.tweak` via psiberx PEGTL grammar per D-005)
   - Build op AST
   - For each op: compute FNV-1a hash of TweakDBID, walk appropriate hash map, mutate flat-value buffer at `TweakDB+0x148`
7. Game continues with modifications applied to the in-memory TweakDB.

**Note:** This is dramatically simpler than the v1.0 plan from Phase 0 startup, when we expected to need a full hooking infrastructure. The actual macOS TweakDB layout (F-013/F-015/F-017) makes direct data manipulation the simplest path.

## Component Boundaries

### v1.0 — Minimal red4ext-mac

| Module | v1.0 Required? | Notes |
|--------|---------------|-------|
| `loader/` | ✅ Required | dylib entry point. v1.0 = single dylib; plugin discovery deferred. |
| `runtime/Symbols` | ✅ Required (lite) | Just need image base + slide capture. No general symbol resolution for v1.0. |
| `runtime/SingletonAccess` | ✅ Required (NEW) | Read `*(0x1080c92d0 + slide)` → `TweakDB*`. Simple primitive. |
| `runtime/Hooks/ApplyTrigger` | ✅ Required | Single hook on DB-populate completion (per T-002e). Approach depends on what T-002e finds — possibly observer callback, NOT a code hook. |
| `runtime/TweakDB` | ✅ Required | Direct struct accessors at F-015 offsets. Hash map walk + value buffer write. |
| `runtime/Memory` | ❌ DEFERRED → v1.x | No trampolines in v1.0. |
| `runtime/Hooks/GOT` | ❌ DEFERRED → v1.x/v2.0 | Not needed for direct data manipulation. |
| `runtime/Hooks/VTable` | ❌ RULED OUT | TweakDB has no vtable (F-016). |
| `runtime/Hooks/FuncPtrTable` | ❌ RULED OUT | No fn-ptr dispatch anywhere (F-017). |
| `runtime/RTTI` | ❌ DEFERRED → v2.0 | Not needed for TweakXL; CET/redscript will need it. |
| `sdk/` | ❌ DEFERRED → v1.x | Plugin SDK isn't needed for single-dylib v1.0. |

### v1.0 — tweakxl-mac

| Module | Status | Notes |
|--------|--------|-------|
| `plugin/Plugin.cpp` | not started | Dylib entry, ties components together |
| `parsers/YAML` | not started | yaml-cpp integration, YAML → op AST |
| `parsers/Tweak` | not started | psiberx parser via `TWEAKXL_MAC_OFFLINE` (D-005) |
| `model/Operation` | not started | In-memory op representation |
| `applicator/` | not started | Op → TweakDB struct mutation (direct memory writes) |

### v1.x — Additional components needed for ArchiveXL

| Module | v1.x Required? | Notes |
|--------|---------------|-------|
| `runtime/Hooks/GOT` | ✅ Likely | Asset loader interception |
| `runtime/Memory` | ✅ Likely | Trampoline pages for original-call preservation |
| `sdk/` | ✅ Required | Plugin SDK so ArchiveXL-style plugins can build against us |

### v2.0 — Additional components needed for CET/redscript

| Module | v2.0 Required? | Notes |
|--------|---------------|-------|
| `runtime/Hooks/VTable` | ✅ Likely | Other game classes likely have vtables |
| `runtime/Hooks/FuncPtrTable` | ✅ Maybe | Depends on dispatch patterns in scripting paths |
| `runtime/RTTI` | ✅ Required | CET binds to game type system |
| MAP_JIT trampolines | ✅ Required | Re-sign for `allow-jit` + `allow-unsigned-executable-memory` |

## Key Architectural Decisions

See `docs/DECISIONS.md` for the full ADR list. Highlights:

- **Runtime hooking over offline patching.** Earlier work tried offline TweakDB binary patching; it doesn't generalize to RED4ext plugin compatibility.
- **GOT / VTable / function-pointer-table hooks only.** Inline hooking is blocked by code signing.
- **No DRM bypass.** Re-signing for entitlements only. Users must own the game.
- **C++17 baseline.** Move to C++20 only if a specific feature justifies it.

## Compatibility Contract

For a mod to "work unchanged on macOS," these must be true:

- **File formats identical:** `.yaml`, `.yml`, `.tweak` files load with the same syntax and operations as Windows TweakXL.
- **Directory layout identical:** Mods install into `r6/tweaks/` exactly as on Windows.
- **Plugin ABI compatible:** A `.dylib` plugin must implement the same logical entry points as a Windows `.dll` plugin (recompilation required; source compatibility maintained).
- **Behavior matches:** A mod that changes "gang density" on Windows must change gang density on macOS in the same way.

## What Is NOT This Project's Job

- Cross-compile Windows `.dll` plugins. Plugin authors must rebuild for macOS.
- Wrap or emulate Windows runtime semantics. macOS-native, ARM64-native.
- Provide a mod manager UI. CLI tooling only for v1.0.
- Support Intel Macs. Cyberpunk 2077 on macOS is Apple Silicon only.

---

## Long-Term Roadmap

**The full ambition:** every gameplay-changing mod that works on Windows works on macOS, unchanged. This is a multi-version effort. v1.0 is the first deliverable, not the destination. See D-006 for the slicing rationale.

```
┌────────────────────────────────────────────────────────────────┐
│ FULL ECOSYSTEM (long-term goal)                                │
│  TweakXL + ArchiveXL + CET + Codeware + redscript + future     │
└────────────────────────────────────────────────────────────────┘
                            ▲
                            │ progressively added
                            │
   ┌────────────────────────┼─────────────────────────┐
   │           v2.0 — Scripting (∞)                   │
   │  CET (Lua + ImGui), redscript runtime hookup,    │
   │  Codeware. Heavy work — Lua VM, ImGui-on-Metal,  │
   │  RTTI / RED4ext API parity at depth.             │
   └────────────────────────┼─────────────────────────┐
                            │
   ┌────────────────────────┼─────────────────────────┐
   │           v1.x — Assets                          │
   │  ArchiveXL — custom archives, mesh/texture       │
   │  replacement, item additions. Mostly file-format │
   │  + resource-loader hooks. Modest runtime cost.   │
   └────────────────────────┼─────────────────────────┐
                            │
   ┌────────────────────────┼─────────────────────────┐
   │  ← YOU ARE HERE        │                         │
   │           v1.0 — Foundation                      │
   │  RED4ext-mac framework solid + TweakXL plugin    │
   │  working. ≥5 Nexus TweakXL mods verified.        │
   │  Stable for 1 hour of play.                      │
   └──────────────────────────────────────────────────┘
                            ▲
                            │
                    ┌───────┴────────┐
                    │  Phase 0–8     │
                    │ (current work) │
                    └────────────────┘
```

### v1.0 — Foundation (current focus)

- RED4ext-mac framework: symbol resolution, hooking primitives, plugin loading lifecycle
- TweakXL-mac plugin: YAML + `.tweak` parsing, applicator to in-memory TweakDB
- 5+ real Nexus TweakXL mods verified working without modification
- Stable 1-hour play session, user install guide published

### v1.x — Custom Assets (ArchiveXL parity)

- File-format work: `.archive` reading/writing
- Resource-loader hooks: intercept the game's asset lookup to inject custom meshes / textures / sounds
- Plugin authors recompile their ArchiveXL-style plugins for macOS — source compatibility preserved
- Mostly additive — does not break v1.0 mods

### v2.0 — Scripting

The largest jump in complexity:

- **CET (Cyber Engine Tweaks):** embed Lua, hook the game's input loop, render an ImGui overlay on Metal. RED4ext API surface must reach functional parity with Windows for the existing CET Lua bindings to work.
- **redscript runtime:** hook the script loader, allow `.reds` scripts from `r6/scripts/` to be compiled and merged into the game's bytecode at load.
- **Codeware:** depends on both RED4ext API depth and redscript integration.

### Cross-Cutting (any version)

- macOS-native conveniences: a `.app` wrapper for `tools/launch.sh`, a simple mod-manager UI, drag-drop install
- Version-gating: every release ships with a verified game-build-version test matrix

### What's Never In Scope

- Windows-to-macOS DLL cross-loading. Plugin authors **must** rebuild for macOS / arm64.
- Intel Mac support. Cyberpunk on macOS is arm64 only; we follow the game.
- DRM bypass, online-mode tampering. Single-player, offline modding only.
- Anything beyond what Windows mods already do. We are a port target, not an extension.
