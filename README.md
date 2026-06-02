# cp2077-mac-mods

**Bringing the Cyberpunk 2077 PC modding stack to the native macOS (Apple Silicon) port** — starting with [RED4ext](https://github.com/WopsS/RED4ext) + [TweakXL](https://github.com/psiberx/cp2077-tweak-xl), so that data/tweak mods written for the Windows version can run on Mac.

Cyberpunk 2077 shipped a native Apple Silicon build in 2025. It runs great — but the modding frameworks the community relies on (RED4ext, TweakXL, ArchiveXL, CET, Codeware, redscript) are Windows-only. This project closes that gap, one layer at a time:

- **v1.0 — Foundation:** a RED4ext-equivalent loader + TweakXL (TweakDB data/`.tweak`/YAML flat mods). ← current focus
- **v1.x — Assets:** ArchiveXL parity (custom archives, mesh/texture/sound).
- **v2.0 — Scripting:** CET (Lua + ImGui-on-Metal), redscript, Codeware.

> ⚠️ **Experimental research project.** This is reverse-engineering / interoperability work, not a finished, installable mod loader. Expect rough edges. See **Status** below for exactly what works today.

## Status

**Phase 1 (Foundation) — framework proven end-to-end, ~75%.** The full pipeline works in-game and is verified against the game's own code:

`DYLD` injection → image-slide capture → TweakDB singleton → apply-trigger (polling) → flat lookup → **interning-safe flat write** → record re-materialization (`UpdateRecord`).

A flat edit is confirmed by calling the game's *own* `GetFlat` accessor and observing the new value (not just our own write-back). YAML/`.tweak` parsing and the applicator run end-to-end.

**What's not done yet:** a *visibly* gameplay-changing demo end-to-end, broad mod compatibility, stability hardening, and a user install guide. Notably, some player stats (e.g. base HP / cyberdeck RAM) are *computed per-entity at runtime* rather than stored as editable values, so not every "edit a number" mod maps to a simple flat write. Current state, blockers, and the next action live in [`state/status.yaml`](state/status.yaml); the detailed engineering record is in [`docs/FACTS.md`](docs/FACTS.md) and [`state/session-log.md`](state/session-log.md).

## How it differs from Windows RED4ext/TweakXL

The Windows stack relies on DLL injection + **inline hooking**. On macOS that doesn't translate directly:

- **Inline hooking is blocked** — the `__TEXT` segment is read-only under macOS code signing. We use data-side techniques (GOT/vtable/function-pointer tables) and a polling apply-trigger instead.
- **The in-memory TweakDB layout differs** from Windows — the struct offsets, flat-value representation, and accessors were re-derived for the macOS build.
- **Injection** uses `DYLD_INSERT_LIBRARIES`; the shipped build already carries the entitlements that make it work.

See [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) for the full picture.

## Repository layout

| Path | What |
|------|------|
| `src/` | C++ sources — `red4ext-mac/` (loader + TweakDB runtime), `tweakxl-mac/` (parser + applicator) |
| `docs/` | Engineering record — `FACTS.md`, `FAILED_APPROACHES.md`, `ARCHITECTURE.md`, references |
| `state/` | Machine-readable project state — `status.yaml`, `session-log.md` |
| `tools/` | Helper scripts (Ghidra analysis, probes, build/run helpers) |
| `reference/` | Pointers to external reference material (see note below) |
| `.openclaw/`, `AGENTS.md`, `CLAUDE.md` | Conventions for AI agents working on the repo |

**`reference/` note:** large external inputs (Ghidra projects, third-party mods, the gibbed TweakDB schema) are **not** vendored here — they're fetched/symlinked locally and are git-ignored. Where one is needed, the relevant doc gives the fetch command (e.g. `git clone https://github.com/gibbed/Cyberpunk-TweakDB-Schema.git reference/gibbed-schema`).

## Building

Requires CMake 3.15+ and a recent Apple Clang. See [`docs/BUILD.md`](docs/BUILD.md). In short:

```sh
cmake -B build -S .
cmake --build build           # -> build/src/{red4ext-mac,tweakxl-mac}/lib*.dylib
```

## Disclaimer

This is an **independent, non-commercial interoperability / research project**. It is **not affiliated with, endorsed by, or sponsored by CD PROJEKT S.A.**

- **No game code or assets are redistributed.** This repository contains only original source, documentation, and analysis notes.
- Cyberpunk 2077, REDengine, and all related code, assets, and data are the property of **CD PROJEKT S.A.** and its licensors. "Cyberpunk 2077" is a trademark of CD PROJEKT S.A.
- Running this software requires your own legally obtained copy of the game. Reverse engineering and modification of a game may be restricted by its EULA; **you are responsible for ensuring your use complies with the applicable license and laws in your jurisdiction.**
- Use at your own risk. Modifying a running game process can crash it or corrupt saves — keep backups.

The MIT [`LICENSE`](LICENSE) applies only to the original work in this repository.

## Credits

- **[psiberx](https://github.com/psiberx)** — TweakXL / TweakDB research the Windows behavior is modeled on
- **[WopsS](https://github.com/WopsS/RED4ext)** — RED4ext
- **[WolvenKit](https://github.com/WolvenKit)** & **[gibbed](https://github.com/gibbed)** — TweakDB format & schema research
- **The RedModding community** — documentation and reverse-engineering groundwork

Not affiliated with CD PROJEKT RED.
