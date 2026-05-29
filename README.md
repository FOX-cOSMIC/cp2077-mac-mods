# cp2077-mac-mods

Port the **full** Cyberpunk 2077 Windows modding stack — RED4ext, TweakXL, ArchiveXL, CET, Codeware, redscript — to native macOS Apple Silicon, so existing Nexus mods work unchanged on Mac. Sliced across versions: TweakXL first (v1.0), assets next (v1.x), scripting after (v2.0). See `docs/ARCHITECTURE.md` for the full roadmap.

## Status

**Phase 0 — Bootstrap & Re-validation.** No working binary yet. See `state/status.yaml` for current state.

## Why

Cyberpunk 2077 got an official native Mac port in July 2025. The game runs great, but every quality-of-life mod that needs CET, ArchiveXL, or TweakXL is Windows-only. This project closes that gap — starting with TweakXL (v1.0) and progressively adding the rest of the ecosystem.

## What's Different from Windows TweakXL?

The Windows version uses RED4ext (DLL injection + inline hooking). On macOS:
- Inline hooking is blocked by code signing — must use GOT/VTable/function-pointer-table hooks
- The TweakDB in-memory layout differs from Windows
- Users must re-sign the game binary with `disable-library-validation` entitlement

See `docs/ARCHITECTURE.md` for the full picture.

## Layout

| Path | What |
|------|------|
| `AGENTS.md` | Entry point for AI agents working on this repo |
| `docs/` | All project documentation (facts, decisions, references) |
| `state/` | Machine-parseable current state, task queue, session log |
| `.openclaw/` | Multi-agent runtime config + agent role definitions |
| `src/` | C++ source (mostly empty until Phase 1) |
| `reference/` | Read-only carry-overs: Ghidra files, example mods, Windows TweakXL source |
| `tools/` | Helper scripts (binary re-signing, build helpers) |

## Building

There is nothing meaningful to build yet. Once Phase 1 begins, see `docs/BUILD.md`.

## License

TBD. Will likely be MIT to match the original TweakXL.

## Credits

- **[psiberx](https://github.com/psiberx)** — original TweakXL author
- **[WolvenKit](https://github.com/WolvenKit)** — TweakDB binary format research
- **RedModding community** — documentation and reverse engineering work

Not affiliated with CD Projekt Red.
