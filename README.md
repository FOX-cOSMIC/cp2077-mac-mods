# cp2077-mac-mods

> 🧊 **On ice (2026-07-04).** The developer moved to a Windows PC and no longer needs this port day-to-day, so active development is paused here. The project is **not abandoned** — it's in a clean, documented, resumable state (see **Status** below and [`AGENTS.md`](AGENTS.md) for a fresh pickup). Issues/PRs from anyone who wants to pick it up are welcome; just don't expect fast responses from the original author for a while.

**Bringing the Cyberpunk 2077 PC modding stack to the native macOS (Apple Silicon) port** — starting with [RED4ext](https://github.com/WopsS/RED4ext) + [TweakXL](https://github.com/psiberx/cp2077-tweak-xl), so that data/tweak mods written for the Windows version can run on Mac.

Cyberpunk 2077 shipped a native Apple Silicon build in 2025. It runs great — but the modding frameworks the community relies on (RED4ext, TweakXL, ArchiveXL, CET, Codeware, redscript) are Windows-only. This project closes that gap, one layer at a time:

- **v1.0 — Foundation:** a RED4ext-equivalent loader + TweakXL (TweakDB data/`.tweak`/YAML flat mods). ← current focus
- **v1.x — Assets:** ArchiveXL parity (custom archives, mesh/texture/sound).
- **v2.0 — Scripting:** CET (Lua + ImGui-on-Metal), redscript, Codeware.

> ⚠️ **Experimental research project.** This is reverse-engineering / interoperability work, not a finished, installable mod loader. Expect rough edges. See **Status** below for exactly what works today.

> 🤖 **Built with AI.** This port has been developed almost entirely with [Claude Code](https://claude.com/claude-code). At pay-as-you-go **API** rates the work done to date would cost roughly **$600** — but it was actually run on a **Claude subscription**, so the real out-of-pocket cost was the flat subscription fee, not $600 of metered API usage.

## Status

**First visible, verified, native-macOS in-game change achieved** ✅ — a direct live-memory write changed the player's eddies **310,915 → 1,000,000** on a live save, no reload, no crash, cleanly reverted (fact **F-044**).

**redscript mods run natively on macOS** ✅ — verified in-game: a `@wrapMethod` hook, compiled by the game's own bundled `scc`, fired on save-load and called a native game system with visible effect (facts **F-050**–**F-052**). No RED4ext, no CET, no code hooking required — just the stock install's own toolchain. This is the cheapest real win in the project: any pure-redscript Nexus mod (no Codeware/ArchiveXL dependency) is a strong candidate to already work unchanged.

**Inline (`__TEXT`) hooking is confirmed blocked, not just untested** — 6 real re-signed copies of the game binary, every entitlement combination a known community fork claims works, all fail identically at the kernel level (fact **F-053**). `__DATA`/GOT/vtable hooking remains the only working primitive on this platform; a full RED4ext-equivalent plugin host is still an open problem.

**Phase 1 (TweakXL foundation) — proven end-to-end.** The data pipeline works in-game and is verified against the game's own code:

`DYLD` injection → image-slide capture → TweakDB singleton → apply-trigger (polling) → flat lookup → flat **write / create + array-append** → record re-materialization (`UpdateRecord`) — each confirmed by the game's *own* `GetFlat`/`GetRecord` accessors (not just our write-back). YAML/`.tweak` parsing and the applicator run end-to-end.

**The wall.** TweakDB-flat edits verify at the engine's accessors but do **not** propagate to live gameplay on macOS: the game reads per-entity *materialized* structures (StatsContainers, record instances) snapshotted before our edits run, so a flat change doesn't show. Reaching those is the hard, open problem ([`docs/FAILED_APPROACHES.md`](docs/FAILED_APPROACHES.md)). The first visible change therefore came from a *different* mechanism — writing the live process memory directly.

**Phase 2 — Live-Edit Tool (productized from that breakthrough).** A small, safe, native-macOS tool that locates and edits **stored counters** (money, crafting components, XP, …) in the running game:
- `set <current> <new>` for a unique value, or `find → narrow → setcand` (a two-snapshot delta) for common/small values;
- one-command **`revert`** — and every write is RAM-only, so a reload also restores;
- guarded against crashes (saturation guard, value caps) and against game patches (a Mach-O **version gate** disables writes on an unrecognised build);
- adversarially reviewed, unit-tested (`ctest`), documented in [`docs/guides/live-edit.md`](docs/guides/live-edit.md).

> **Derived stats are different:** displayed numbers like cyberdeck RAM or max HP are *computed* from base + modifiers at read time — never stored as the value you see — so they aren't reachable by a value-scan. Stored counters (money/components/XP) are. (See `FA-017`…`FA-019`.)

**What's not done yet:** general Windows-mod-file (`.yaml`) changes taking visible effect (the wall above), asset/texture mods, the rest of the scripting ecosystem (CET, Codeware, redscript mods with custom natives), stability hardening, and a packaged installer. The full plan is in [`docs/ROADMAP.md`](docs/ROADMAP.md); current state, blockers, and next action in [`state/status.yaml`](state/status.yaml); the engineering record in [`docs/FACTS.md`](docs/FACTS.md) and [`state/session-log.md`](state/session-log.md).

## How it differs from Windows RED4ext/TweakXL

The Windows stack relies on DLL injection + **inline hooking**. On macOS that doesn't translate directly:

- **Inline hooking is blocked, confirmed empirically** — the `__TEXT` segment is read-only under macOS Hardened Runtime / code signing, on the stock binary *and* across every ad-hoc re-sign entitlement combination tested, including ones a community fork claims work (fact **F-053**). We use data-side techniques (GOT/vtable/function-pointer tables) and a polling apply-trigger instead. The one untested lever left is a real (non-ad-hoc) Apple Developer ID signature — see [`docs/ROADMAP.md`](docs/ROADMAP.md) Phase 5.
- **The in-memory TweakDB layout differs** from Windows — the struct offsets, flat-value representation, and accessors were re-derived for the macOS build.
- **Visible changes go through live memory, not TweakDB** — because flat edits don't reach the game's materialized runtime structures, the live-edit tool writes the running process memory directly (`mach_vm_write`) and holds/reverts the result.
- **Injection** uses `DYLD_INSERT_LIBRARIES`; the shipped build already carries the entitlements that make it work.

See [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) for the full picture.

## Repository layout

| Path | What |
|------|------|
| `src/` | C++ sources — `red4ext-mac/` (loader + TweakDB runtime), `tweakxl-mac/` (parser + applicator) |
| `docs/` | Engineering record — `ROADMAP.md`, `PHASE_*_PLAN.md`, `FACTS.md`, `FAILED_APPROACHES.md`, `ARCHITECTURE.md`, `guides/`, references |
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
