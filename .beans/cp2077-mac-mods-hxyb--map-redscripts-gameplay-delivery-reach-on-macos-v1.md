---
# cp2077-mac-mods-hxyb
title: Map redscript's gameplay-delivery reach on macOS (v1.0 vehicle)
status: todo
type: feature
priority: high
created_at: 2026-07-03T08:21:59Z
updated_at: 2026-07-03T08:21:59Z
---

F-052 proved redscript executes natively and can call game systems (TransactionSystem.GiveItem moved real eddies). This is a WORKING path into gameplay that routes around the TweakDB->gameplay propagation wall (F-043/FA-016) that has blocked the v1.0 "visible mods" goal. Goal: chart exactly what visible gameplay redscript can deliver on the stock macOS install, and evaluate redscript (not the walled TweakDB-flat path) as the primary v1.0 delivery vehicle.

## Test plan (each = a minimal .reds, scc-compile, launch, observe; priority order)
- [ ] Capability B — @replaceMethod alters game LOGIC (not just add items). Prove we can change a computed/returned value that affects behavior (e.g. a damage/price/gate method). Distinguishes "change game logic" from F-052's "call a system".
- [ ] Capability C — stat change via StatsSystem/StatModifier (e.g. carry capacity or a visible stat) takes effect. Directly relevant: this is what many TweakXL mods do that the TweakDB path can't deliver (F-039/F-040).
- [ ] Capability D — read + conditionally act (query game state in a hook, branch). Confirms non-trivial logic runs, not just fire-and-forget.
- [ ] Capability E — persistence/interaction with a real Nexus redscript mod (pick a pure-.reds Nexus mod, verify it works unchanged) -> the actual v1.0 compatibility criterion for scripting.
- [ ] Decide: is redscript the right v1.0 gameplay-delivery vehicle? Record as a DECISION (doc-keeper) + re-scope if yes.

## Notes
- Execution requires the user to launch + observe in-game (redscript effects have no log sink without RED4ext, per q90k). Author + scc-compile is autonomous; observation is not.
- Cleanup discipline (F-047 lesson): remove test mod + recompile clean final.redscripts after each capability is recorded.
- Scope caveat (F-052): @addField/@addMethod struct extension and custom RED4ext/Codeware natives are NOT covered here — those still need the RED4ext-equivalent (separate track).
