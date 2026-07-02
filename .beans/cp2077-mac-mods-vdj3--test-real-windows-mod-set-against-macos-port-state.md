---
# cp2077-mac-mods-vdj3
title: Test real Windows mod set against macOS port state
status: completed
type: task
priority: normal
created_at: 2026-07-02T09:03:15Z
updated_at: 2026-07-02T09:21:56Z
---

User supplied 10 mods+deps to test: Judy Romanced Enhanced, Underwear loose patch, Appearance Menu Mod (CET), TweakXL, ArchiveXL, RED4ext, redscript, Native Settings UI, CET 1.37.1 scripting fixes, AtomiicX Summer Laces Bikini. Inventory file types + binary arch, triage each against known facts (F-045 archive not loaded, F-047/F-048 redscript not reached at runtime, RED4ext/CET Windows PE), then determine what (if anything) is testable natively now vs blocked on Phase 5 hook work.

## Findings — full triage (2026-07-02)

**All 4 framework binaries are Windows PE32+ x86-64** (`file` confirmed): RED4ext.dll + winmm.dll, ArchiveXL.dll, TweakXL.dll, CET version.dll + cyber_engine_tweaks.asi, and redscript's scc.exe/scc_lib.dll. None can load into the ARM64 Mach-O `Cyberpunk2077` process. RED4ext's injection vector (a proxy `winmm.dll` beside `.exe` in `bin/x64/`) has no target: the Mac install has **no `bin/x64/`** and a Mach-O game, not a PE .exe. So the entire RED4ext/CET plugin stack is unloadable as shipped.

**Native substitutes present but insufficient for these mods:**
- redscript: native `engine/tools/scc` exists (F-046) and WILL compile `.reds`, but F-047/F-048 prove the compiled blob never reaches runtime — reload-gated resolver never fires. So reds mods compile, don't take effect.
- TweakXL: the project's `libtweakxl-mac.dylib` is an env-var-driven live-memory-edit research tool, NOT an `r6/tweaks/*.yaml` loader — it cannot consume a real TweakXL yaml tweak.

**Per-mod status (none visibly testable natively now):**
| Mod | Payload | Needs | Blocked by |
|-----|---------|-------|------------|
| Judy Romanced Enhanced | 1 .archive | archive/pc/mod scan | F-045 (no mod-folder scan) |
| Underwear loose patch | 1 .archive | archive/pc/mod scan | F-045 |
| AtomiicX Summer Laces Bikini | .archive + .xl + .yaml | ArchiveXL + TweakXL | F-045 + no ArchiveXL port + port isn't a yaml loader |
| Appearance Menu Mod | 18×.archive + .xl + CET Lua | CET + ArchiveXL | CET unloadable (PE + D3D11) + no ArchiveXL port + F-045 |
| Native Settings UI | CET Lua | CET | CET unloadable |
| RED4ext / TweakXL / ArchiveXL / CET / redscript(win) | PE DLLs/asi/exe | — | Windows PE, no injection vector on Mach-O |

**Conclusion:** the mod set is the canonical Windows stack; on macOS every path is blocked on Phase-5-class hooking that is not done (RED4ext-equivalent loader + resource-depot archive registration + a redscript runtime trigger). Installing the PE frameworks is a no-op (nowhere to inject). No launch performed — it would only re-confirm F-045/F-048.

## Summary of Changes
Inventoried and arch-checked all 10 zips; triaged each against F-045/F-046/F-047/F-048 and the native port's actual capabilities. Result: nothing in this set produces a visible native result yet; all blocked on the Phase-5 hook work already tracked (archive loader; redscript trigger cp2077-mac-mods-pyri; a RED4ext-equivalent for plugins). No follow-up beans needed beyond existing hook tasks.
