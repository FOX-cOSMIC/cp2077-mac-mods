# ARCHITECTURE.md

## System Overview

```
┌─────────────────────────────────────────────────┐
│              User-Installed Mods                │
│       (Nexus Mods, .yaml / .tweak files,        │
│        future: .dylib RED4ext plugins)          │
└──────────────────────┬──────────────────────────┘
                       ↓
┌──────────────────────▼──────────────────────────┐
│              tweakxl-mac (plugin)               │
│  - Scans r6/tweaks/ for .yaml / .tweak files    │
│  - Parses mod ops (Assign / Append / Remove)    │
│  - Applies to TweakDB via red4ext-mac runtime   │
└──────────────────────┬──────────────────────────┘
                       ↓
┌──────────────────────▼──────────────────────────┐
│         red4ext-mac (core framework)            │
│  - Mach-O symbol resolution                     │
│  - GOT / VTable / function-pointer hooking      │
│  - Plugin lifecycle (load / init / unload)      │
│  - TweakDB runtime access primitives            │
└──────────────────────┬──────────────────────────┘
                       ↓
┌──────────────────────▼──────────────────────────┐
│  Re-signed Cyberpunk 2077.app (macOS native)    │
│  - disable-library-validation entitlement       │
│  - DYLD_INSERT_LIBRARIES = libred4ext.dylib     │
└─────────────────────────────────────────────────┘
```

## Runtime Flow

1. User launches game via Steam/GOG/etc. with `DYLD_INSERT_LIBRARIES` set (via a wrapper script in `tools/`).
2. `dyld` loads `Cyberpunk2077` then `libred4ext.dylib`.
3. red4ext-mac initializes:
   - Parses Mach-O of running process for symbol resolution
   - Sets up hook tables
   - Scans `plugins/` directory for `.dylib` plugins
   - Loads each plugin's `RED4MACOS_Load()` entry point
4. tweakxl-mac plugin loads, registers hooks on TweakDB lifecycle.
5. When TweakDB initialization runs, tweakxl-mac:
   - Scans `r6/tweaks/` for mod files
   - Parses YAML / `.tweak` into internal op representation
   - Applies ops to the in-memory TweakDB via red4ext-mac primitives
6. Game runs with modifications active.

## Component Boundaries

### red4ext-mac

| Module | Responsibility | Status |
|--------|----------------|--------|
| `loader/` | dylib entry point, plugin discovery | not started |
| `runtime/Symbols` | Mach-O parse, ASLR-aware symbol resolution | not started |
| `runtime/Memory` | RW→RX page allocation for trampolines | not started |
| `runtime/Hooks/GOT` | Function pointer hooks in `__DATA` | not started |
| `runtime/Hooks/VTable` | VTable slot replacement | not started |
| `runtime/Hooks/FuncPtrTable` | Function-pointer-table entry replacement (macOS-specific) | not started |
| `runtime/RTTI` | Hook game's type system, allow plugins to register types | not started |
| `runtime/TweakDB` | Primitives: GetFlat / SetFlat / record ops on macOS layout | not started |
| `sdk/` | Public headers for plugin authors | not started |

### tweakxl-mac

| Module | Responsibility | Status |
|--------|----------------|--------|
| `plugin/Plugin.cpp` | RED4MACOS_Load entry, lifecycle | not started |
| `parsers/YAML` | yaml-cpp integration, YAML→op AST | not started |
| `parsers/Tweak` | PEGTL grammar, `.tweak`→op AST | not started |
| `model/Operation` | In-memory op representation | not started |
| `applicator/` | Op → TweakDB mutation via red4ext-mac primitives | not started |

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
