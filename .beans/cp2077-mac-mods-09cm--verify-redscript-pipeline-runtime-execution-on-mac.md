---
# cp2077-mac-mods-09cm
title: Verify redscript pipeline runtime execution on macOS
status: completed
type: task
priority: normal
created_at: 2026-07-01T23:13:03Z
updated_at: 2026-07-01T23:59:12Z
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


## Summary of Changes

**Result: NEGATIVE.** redscript compiles cleanly on macOS (F-046, confirmed again) but the game
**never loads `r6/cache/final.redscripts` at runtime**. Documented as **F-047**.

Evidence:
- `lsof` on the running process immediately post-launch: no handle to `r6/cache/final.redscripts`,
  `r6/scripts`, or anything redscript-related.
- Rapid `lsof` polling (0.5s interval) across two full startup -> main-menu -> save-load windows
  (~76s each): zero opens of any redscript-related path at any point, including after the user
  loaded into the world. The test mod's on-screen message never appeared.
- `strings` on the macOS binary DOES contain `RedScriptsHost`, `final.redscripts`, etc. -- the
  loader code exists but is dead/gated/never reached on this build, unlike F-045 (archive mods)
  where no comparable native hook exists at all.

Docs updated: docs/FACTS.md (F-047 added), docs/ROADMAP.md (Phase 6 re-sequencing note revised --
redscript moves alongside Phase 3, no longer "cheapest win"), docs/FAILED_APPROACHES.md (FA-020:
process lesson -- don't stage user-owned files in /tmp, it's not durable across reboots).

**Side effect / data loss:** the `NightCityAlive` mod, moved to `/tmp/NightCityAlive.disabled` in
an earlier session to unblock a compile test, was lost when `/tmp` was cleared (likely a reboot).
Not recoverable from this machine -- user will re-download it themselves. r6/scripts/ is now empty
(no third-party mods installed); r6/cache/final.redscripts was recompiled clean with no mods present.

Cleanup done: quit the game, removed the MacRedscriptTest test mod, recompiled clean.
