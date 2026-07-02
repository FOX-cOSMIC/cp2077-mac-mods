---
# cp2077-mac-mods-4paj
title: RE why RedScriptsHost isn't reached on macOS
status: completed
type: task
priority: normal
created_at: 2026-07-02T07:53:12Z
updated_at: 2026-07-02T08:43:00Z
---

F-047 established that redscript compiles cleanly (final.redscripts) but the game never opens it at
runtime on macOS. Binary strings show RedScriptsHost, ScriptsBlobFilename (CVar default
"master.redscripts"), scriptsBlobPath, PrimaryBlobPath, FallbackBlobPath, BlobPathUsed,
ShouldCompileScripts, Loaded — all present in compiled code but not observably reached.

## Todo
- [x] Ghidra-decompile FUN_103d9b4f0 (ScriptsBlobFilename CVar registration, default "master.redscripts")
- [x] Ghidra-decompile FUN_10223b0d0 (RedScriptsHost subsystem locator — likely not the file-opener)
- [x] Empirical test: copy final.redscripts -> master.redscripts, relaunch, load save — NO effect
- [x] Ghidra-decompile FUN_103d8c188 (confirms scriptsBlobPath/tweakdbBlobPath parsed from argv hashmap)
- [x] Empirical test: launch with `-scriptsBlobPath <final.redscripts abs path>` — NO effect
- [x] Empirical test: `-windowCaption X -scriptsBlobPath <path>` (argv-tokenizer sanity probe) — inconclusive (no window title captured, fullscreen)
- [x] Confirm whether the Mac build's scripts are baked into `.archive` containers (resource-depot load) instead of the loose `r6/cache/*.redscripts` blob — **DISPROVEN.** Loading is filesystem-PATH-based (base dir `DAT_1090031f8` vtable+0xc0 + variant filename + `scriptsBlobPath` override `DAT_1090dbe00`, stat'd via vtable+0x38). Recorded as F-048.
- [x] Determine correct argv syntax — MOOT: traced the `scriptsBlobPath` sink to global `DAT_1090dbe00`, which is consumed ONLY inside the reload-gated resolver `FUN_103d9a028`. No argv syntax reaches the loader while that gate stays closed, so nailing `-key value` vs `-key=value` would not have changed the empirical negatives.
- [x] If depot-load hypothesis confirmed: document as new fact — N/A (hypothesis disproven, not confirmed). Documented the *actual* mechanism as F-048 instead; the redscript-runtime work stays a Phase-5-class hook task (follow-up bean created).
- [x] sudo fs_usage/dtruss trace, if RE stalls — NOT NEEDED: static RE located the root cause (resolver called only from the reload-dirty-gated frame watcher `FUN_103d996e0`/`FUN_103d99938`); no dynamic tracing required.

## Summary of Changes

Static Ghidra RE (headless, build 2.3.1) answered this bean's core question and disproved its leading hypothesis. Findings recorded as **F-048** in `docs/FACTS.md`:

- The macOS script-blob loader is **filesystem-path-based, not archive-depot** — it builds `<base dir (DAT_1090031f8 vtable+0xc0)> + <variant filename>` (`master`/`final`/`profiling`/`final_noopts`.redscripts), applies the `scriptsBlobPath` argv override (global `DAT_1090dbe00`), and stats the path via `DAT_1090031f8` vtable+0x38 (0 = missing).
- The resolver `FUN_103d9a028`/`FUN_103d9a8dc` (via getter `FUN_103d9be38` on the Engine/Scripts settings singleton `DAT_108943550`, schema `FUN_103d9c648`) is called **only** from the per-frame scripts hot-reload watcher `FUN_103d996e0`/`FUN_103d99938`, and only when a **reload-dirty flag is set** (`*(param+0x57) && param[0xb]`, which also fires a `"StartDataReload"` job). In a retail run that flag is never raised, so the loose-blob stat/compile/load path never runs — consistent with F-047's `lsof` negatives and the failed `-scriptsBlobPath`/`master.redscripts` tests.

Remaining open question (moved to a follow-up bean, Phase-5 hook work): whether the game's *initial* boot-time script load uses a different code path, and which lever (raise the reload-dirty flag vs. hook a boot loader) makes redscript reach gameplay. That is a hooking task, not further static RE, so it does not block completing this investigation bean.
