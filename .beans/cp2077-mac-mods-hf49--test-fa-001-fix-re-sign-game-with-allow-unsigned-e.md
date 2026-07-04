---
# cp2077-mac-mods-hf49
title: 'Test FA-001 fix: re-sign game with allow-unsigned-executable-memory + allow-jit'
status: completed
type: task
priority: high
created_at: 2026-07-03T23:58:36Z
updated_at: 2026-07-04T00:30:37Z
---

H-012 (docs/HYPOTHESES.md): a community RED4ext macOS fork's docs claim CP2077's stock binary is missing com.apple.security.cs.allow-unsigned-executable-memory + com.apple.security.cs.allow-jit (already has allow-dyld-environment-variables + disable-library-validation, matching our baseline), and that adding them may unlock inline/JIT-based hooking -- a direct fix for FA-001 (inline __TEXT hooking blocked under Hardened Runtime).

This is UNVERIFIED by the source project and by us -- their own port's actual hooking (Frida-based) has no dynamic hook API and may not even need these entitlements (internal doc contradiction, see H-012 for full skepticism).

## Todo
- [x] Got explicit user sign-off ("go ahead, back up the game and test it")
- [x] Backed up via ditto to 6 isolated .app copies (SHA256-verified identical before any change); never touched the live Steam install
- [x] Re-signed backup copies only (never --deep); tested allow-unsigned-executable-memory+allow-jit AND (extended scope) disable-executable-page-protection + get-task-allow
- [x] Tested via h004_probe.cpp -- NO, fails identically across all 4 tested configurations (KERN_PROTECTION_FAILURE/EPERM)
- [x] Confirmed (with one caveat found + fixed): must drop CDPR's restricted provisioned entitlements before ad-hoc signing or the process gets SIGKILL'd at launch (documented as new guidance)
- [x] Recorded as F-053 (docs/FACTS.md); resolved H-004 + H-012 in docs/HYPOTHESES.md; corrected macos-codesigning.md and extended FA-001's scope note
- [x] N/A -- never modified the live install; all work on disposable copies

## Summary of Changes — COMPLETED (F-053)

User approved the test. Built h004_probe.cpp (new, safe read-only diagnostic: tests mprotect/mach_vm_protect on a __TEXT page, reverts immediately, never writes bytes). Tested against 6 isolated .app bundle copies (never the live Steam install), each ditto-verified identical to the live binary (SHA256 matches F-001) before any change.

RESULT: DECISIVE NEGATIVE across every tested configuration.
- copy0 (control, untouched): mprotect/mach_vm_protect FAIL (EPERM/KERN_PROTECTION_FAILURE) -- expected baseline, now empirically confirmed for the first time on build 2.3.1 (closes long-open T-005).
- copyA (CDPR restricted entitlements preserved + jit-pair, ad-hoc signed): SIGKILL at launch (exit 137) -- unrelated to __TEXT, caused by preserving Apple-provisioned restricted keys under ad-hoc signing. Real, actionable gotcha, now documented.
- copyC (minimal base, no restricted keys): launches clean, confirms the SIGKILL cause; mprotect/mach_vm_protect still FAIL (matches control).
- copyD (minimal + allow-unsigned-executable-memory + allow-jit -- the exact hypothesis from H-012/memaxo): FAILS identically. H-012 REFUTED.
- copyE (+ disable-executable-page-protection, this projects own prior uncited claim in macos-codesigning.md): FAILS identically. That claim REFUTED too -- corrected in the doc.
- copyF (+ get-task-allow): FAILS identically.

Conclusion: no tested ad-hoc-signing entitlement combination unlocks __TEXT write access on this build. Recorded as F-053 (docs/FACTS.md), resolved H-004 and H-012 in docs/HYPOTHESES.md, corrected the uncited claim in docs/reference/macos-codesigning.md, extended FA-001s scope note, and added the SIGKILL/restricted-entitlements gotcha as re-sign guidance. New reusable diagnostic tool: src/red4ext-mac/src/probes/h004_probe.cpp + tools/run-h004-probe.sh (safe to run anytime against the live install -- read-only, reverts immediately).

Remaining untested lever (not ruled out): a real, non-ad-hoc Apple Developer ID signature. Materially higher cost/complexity; not pursued this session.
