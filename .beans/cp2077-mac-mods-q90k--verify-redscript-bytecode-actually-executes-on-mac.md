---
# cp2077-mac-mods-q90k
title: Verify redscript bytecode actually EXECUTES on macOS (fresh test mod)
status: todo
type: task
priority: high
created_at: 2026-07-02T21:42:39Z
updated_at: 2026-07-02T21:42:39Z
---

F-050 proved the game opens/mmaps r6/cache/final.redscripts at boot (loader runs natively; no hook needed). Still unproven: that the compiled bytecode is linked into the script VM and its @wrapMethod/@replaceMethod hooks run in gameplay. F-047's on-screen-message test showed no visible effect, but its stated cause (blob not loaded) is now known false — so the null is either a flawed test mod (wrong hook target / a toast API that does not render on this build) or a post-load validation/version gate silently rejecting the blob. Author a minimal, robustly-observable .reds (prefer a file/stdout-checkable signal like FTLog captured via launch_modded.sh stdout, plus a guaranteed-visible gameplay change), scc-compile, launch, observe. If it fires -> redscript works natively (re-sequence Phase 6 much earlier, big win). If not -> RE the post-open path (blob validate/version/VM-register) as the real gate. NOTE old MacRedscriptTest mod was cleaned up; needs a new one.
