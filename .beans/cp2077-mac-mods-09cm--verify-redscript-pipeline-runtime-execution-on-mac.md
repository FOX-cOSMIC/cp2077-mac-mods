---
# cp2077-mac-mods-09cm
title: Verify redscript pipeline runtime execution on macOS
status: in-progress
type: task
created_at: 2026-07-01T23:13:03Z
updated_at: 2026-07-01T23:13:03Z
---

Phase 6 (early) verification per docs/ROADMAP.md F-046. Compile-side proof is DONE: with
NightCityAlive moved aside (broken .reds syntax aborted the whole compile), scc cleanly
compiled r6/scripts/MacRedscriptTest/test.reds into r6/cache/final.redscripts (regenerated,
size grew to 16,167,154 bytes). Test mod wraps PlayerPuppet.OnGameAttached to show an
on-screen message "redscript is LIVE on macOS".

Remaining: RUNTIME proof. Launch the game via launch_modded.sh (recompiles cleanly now),
load a save, and observe whether the on-screen message actually appears. This resolves the
open question of whether the macOS build natively loads r6/cache/final.redscripts at all
(no separate redscript loader was observed in the install).

## Todo
- [ ] Confirm no game instance is running as root (from the earlier fs_usage trace) — user must Cmd-Q it
- [ ] Launch game via launch_modded.sh (or plain binary)
- [ ] User loads a save / gets into the world
- [ ] Observe whether "redscript is LIVE on macOS" on-screen message appears (~15s duration)
- [ ] Record result as a FACTS.md entry (F-046 follow-up)
- [ ] Restore NightCityAlive: mv /tmp/NightCityAlive.disabled r6/scripts/NightCityAlive
- [ ] Remove/clean up r6/scripts/MacRedscriptTest test mod
- [ ] Decide whether to keep or revert the recompiled final.redscripts (backup at /tmp/final.redscripts.preMacTest)
- [ ] Update state/status.yaml (via doc-keeper) and commit
