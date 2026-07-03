---
# cp2077-mac-mods-q90k
title: Verify redscript bytecode actually EXECUTES on macOS (fresh test mod)
status: completed
type: task
priority: high
created_at: 2026-07-02T21:42:39Z
updated_at: 2026-07-03T07:35:25Z
---

F-050 proved the game opens/mmaps r6/cache/final.redscripts at boot (loader runs natively; no hook needed). Still unproven: that the compiled bytecode is linked into the script VM and its @wrapMethod/@replaceMethod hooks run in gameplay. F-047's on-screen-message test showed no visible effect, but its stated cause (blob not loaded) is now known false — so the null is either a flawed test mod (wrong hook target / a toast API that does not render on this build) or a post-load validation/version gate silently rejecting the blob. Author a minimal, robustly-observable .reds (prefer a file/stdout-checkable signal like FTLog captured via launch_modded.sh stdout, plus a guaranteed-visible gameplay change), scc-compile, launch, observe. If it fires -> redscript works natively (re-sequence Phase 6 much earlier, big win). If not -> RE the post-open path (blob validate/version/VM-register) as the real gate. NOTE old MacRedscriptTest mod was cleaned up; needs a new one.

## Progress 2026-07-02
- [x] Authored fresh test mod r6/scripts/MacRedscriptTest/test.reds (@wrapMethod PlayerPuppet.OnGameAttached -> FTLog + LogChannel sentinel MACREDSCRIPT_SENTINEL_7F3A). File-checkable via game stdout; does not rely on toast rendering.
- [x] scc-compiled clean; final.redscripts grew 15.4MB -> 16.1MB (our bytecode included).
- [ ] AWAITING USER: run scratchpad/launch_redscript_test.sh (no sudo), load a save, quit; grep stdout for sentinel.
- [ ] Interpret: sentinel present -> redscript executes natively (big win, re-sequence Phase 6). Absent -> RE post-open blob validate/version/VM-register path.

## Pivot 2026-07-02 (log sink dead-end -> visible test)
- Launch-with-stdout test: game reached InGameGeneric (loaded in), but stdout had only 7 Steam/GameServices error lines; unified log (log show, process==Cyberpunk2077) had 0 lines. Conclusion: redscript FTLog/LogChannel has NO log sink on this build (RED4ext normally provides it) -> logging is an unreliable execution signal here.
- Switched test.reds to a gameplay-VISIBLE signal: @wrapMethod PlayerPuppet.OnGameAttached -> TransactionSystem.GiveItem(Items.money, 7,770,000). Compiles clean (API valid for this build). 
- [ ] AWAITING USER: launch (Steam ok, blob already compiled), load save, check eddies. ~7.77M+ => redscript EXECUTES natively. Unchanged => RE post-open blob validate/version/VM-register gate.

## Static analysis 2026-07-03 (F-051) — loader is REAL
While the money test is pending, traced the post-open path. The full load->parse->validate->register->link chain is IMPLEMENTED on macOS (RedScriptsHost vtable+8 = FUN_10223a748, ~290 lines; parse FUN_1021f0204; validate FUN_1021fbf90; commit FUN_1021e897c). Only the in-process compiler FUN_103d963a4 is a stub (return 0) — correct, since scc compiles externally. Two (silent) failure modes only: parse fail ('Failed to load scripts') or RTTI validation fail ('Validation failed for N types'), both to the no-sink diagnostics callback. Prediction: since scc is this build's native toolchain, the blob should validate -> redscript should execute. If money test fails, cause is FUN_1021fbf90 validation (next: surface that error sink).

## Summary of Changes — COMPLETED (F-052)
RESULT: redscript EXECUTES natively on macOS. Test save had 310,915 eddies; after loading with the @wrapMethod OnGameAttached -> TransactionSystem.GiveItem(money,7,770,000) mod, wallet read 8,080,915 (310,915+7,770,000 exact). Confirms end-to-end: blob loaded (F-050) -> parsed/validated/registered (F-051) -> executed with working native-function binding + @wrapMethod, on a stock install with NO RED4ext/CET/hooks. Recorded F-052. Test mod removed and final.redscripts recompiled clean. Roadmap implication (for doc-keeper): redscript runtime moves from 'v2.0/needs hook' to 'done/verified'; does not change v1.0 TweakXL scope.
