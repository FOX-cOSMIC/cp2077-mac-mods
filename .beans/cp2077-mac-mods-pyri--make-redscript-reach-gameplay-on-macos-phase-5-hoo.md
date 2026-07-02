---
# cp2077-mac-mods-pyri
title: Make redscript reach gameplay on macOS (Phase 5 hook)
status: todo
type: task
priority: normal
created_at: 2026-07-02T08:42:53Z
updated_at: 2026-07-02T08:43:00Z
blocked_by:
    - cp2077-mac-mods-4paj
---

Follow-up to cp2077-mac-mods-4paj / F-048. The macOS script-blob loader is path-based and works, but its resolver (FUN_103d9a028/FUN_103d9a8dc) runs ONLY from the per-frame hot-reload watcher (FUN_103d996e0/FUN_103d99938) when the reload-dirty flag is set (*(param+0x57) && param[0xb]). Retail never raises it, so final.redscripts is never loaded (F-047). Two candidate levers: (1) locate the boot-time initial-load path (if separate from this settings resolver) and hook/enable it; (2) force the reload-dirty flag so the existing path-based resolver runs. Both are Phase-5-class hooks, blocked by the same __TEXT inline-hook constraint (FA-001) tracked in the Phase 5 plan.
