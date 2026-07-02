---
# cp2077-mac-mods-pyri
title: Make redscript reach gameplay on macOS (Phase 5 hook)
status: scrapped
type: task
priority: normal
created_at: 2026-07-02T08:42:53Z
updated_at: 2026-07-02T21:42:39Z
blocked_by:
    - cp2077-mac-mods-4paj
---

Follow-up to cp2077-mac-mods-4paj / F-048. The macOS script-blob loader is path-based and works, but its resolver (FUN_103d9a028/FUN_103d9a8dc) runs ONLY from the per-frame hot-reload watcher (FUN_103d996e0/FUN_103d99938) when the reload-dirty flag is set (*(param+0x57) && param[0xb]). Retail never raises it, so final.redscripts is never loaded (F-047). Two candidate levers: (1) locate the boot-time initial-load path (if separate from this settings resolver) and hook/enable it; (2) force the reload-dirty flag so the existing path-based resolver runs. Both are Phase-5-class hooks, blocked by the same __TEXT inline-hook constraint (FA-001) tracked in the Phase 5 plan.

## Todo
- [x] Find who calls the reload watcher + owner of gate flags — DONE (F-049): found the BOOT path instead (more important)
- [x] Writers of +0x57/+0x58 — MOOT: boot path doesn't use those reload flags; it uses the obj+0x150 empty-string gate (F-049)
- [x] Check FUN_103d99e44 — CONFIRMED boot-time load entry (via FUN_1035f0a08/FUN_103d9e494); gate = obj+0x150 empty-string test
- [x] Decide → empirical stat-focused boot trace defined (F-049); needs sudo (user runs via ! prefix)

## Progress 2026-07-02 (F-049)

Traced the full boot chain. KEY CORRECTION to F-048: the resolver IS reached at boot, not only from the reload watcher.

- Boot chain: FUN_1035f0a08 (engine init, engine+0x308=framework) -> FUN_103d9e494 (subsystem bring-up incl. Scripts phase) -> FUN_103d99e44 -> resolver.
- Gate FUN_10002b9d4(obj+0x150) = "std::string empty?"; EMPTY (retail default) -> full builder FUN_103d9a028 (builds+stats master/final.redscripts, compiles).
- Resolve worker FUN_101712a3c = raw _stat() syscall (not a depot-registry lookup) -> disproves the no-syscall idea; a real stat should be visible to fs_usage.
- Engine-file object DAT_1090031f8 vtable PTR_FUN_106f501c0: +0xc0 returns this+8 (base dir string), +0x38 -> FUN_10170e84c -> _stat worker.

Contradiction to resolve empirically: full builder should stat master/final.redscripts at boot, yet F-047 saw zero redscript FS access. Recorded as F-049 with 4 candidate explanations.

BLOCKED ON USER (needs sudo, run via ! prefix): stat-focused boot trace:
  sudo fs_usage -w -f filesys Cyberpunk2077 | grep -iE 'redscripts|r6/cache'
over a cold boot to main menu, capturing stat/getattrlist (NOT just open). Presence/absence of a master/final.redscripts stat decides hook-target vs downstream-failure.

## Reasons for Scrapping

The premise is refuted by F-050. This bean assumed the redscript blob loader is never reached at retail boot and needs a Phase-5 hook (or a reload-flag trigger) to run. A sudo fs_usage trace proved the shipping macOS game process stat64s + opens r6/cache/final.redscripts (fd 113) at boot with NO hook — the loader runs natively. There is nothing to hook to "make redscript reach the loader." The genuinely-open question (does the loaded bytecode execute?) is not a hooking task; tracked in the new bean.
