# HYPOTHESES.md — Theories Being Tested

**Owner:** any agent may add; `doc-keeper` prunes stale entries.

Hypotheses go here while they're being tested. Each entry must resolve to either `FACTS.md` (validated) or `FAILED_APPROACHES.md` (disproved) within **14 days**. Stale hypotheses are demoted by `doc-keeper`.

---

## How to Add an Entry

```markdown
### H-NNN: <one-line claim>

- **Proposed:** YYYY-MM-DD by <agent>
- **Why we think this:** <reasoning, prior session evidence, RE clues>
- **How to test:** <experiment design>
- **Status:** open | in-progress | resolved-fact | resolved-failed
- **Owner:** <agent assigned to test>
```

---

## How to Resolve

When you have evidence:

- **Confirmed:** create the matching `F-NNN` in `FACTS.md`, mark this hypothesis `resolved-fact`, link both ways.
- **Disproved:** create the matching `FA-NNN` in `FAILED_APPROACHES.md`, mark this hypothesis `resolved-failed`, link both ways.

Resolved hypotheses can be deleted after 30 days (the source of truth is now in FACTS or FAILED).

---

## Open Hypotheses

*Seed hypotheses are below — these come from the old work and need re-validation on the current game build before they can be promoted to FACTS.*

### H-001: Game's TweakDB storage uses a function-pointer dispatch table, not raw data

- **Proposed:** 2026-05-28 (carried from old session 62e, dated 2025-12-14)
- **Why we think this:** Old dump showed `staticFlatDataBuffer` contained code pointers (e.g., `0x102f53b04`) rather than data values. Hook on entry[16] reportedly fired at startup.
- **How to test:** Re-dump the structure at the same offset on the current game build. Disassemble entries[0..20] and check if they're code (function prologues) or data (TweakDBID-shaped values).
- **Status:** open
- **Owner:** researcher

### H-002: macOS TweakDB struct is 0x198 bytes, not Windows's 0x168 bytes

- **Proposed:** 2026-05-28 (carried from old session 20)
- **Why we think this:** Old finding that Windows offsets (`0x148` for flatDataBuffer) didn't match macOS layout.
- **How to test:** Locate TweakDB constructor in current Ghidra output, measure struct size from allocation site.
- **Status:** open
- **Owner:** researcher

### H-003: VTABLE[5] of TweakDB is a reliable startup-fire hook

- **Proposed:** 2026-05-28 (carried from old session 14)
- **Why we think this:** Old session showed VTABLE[5] firing hundreds of times from startup, useful as init trigger.
- **How to test:** Hook VTABLE[5] on current build, log first-fire timestamp relative to game launch.
- **Status:** open
- **Owner:** researcher

### H-004: Inline `__TEXT` hooking remains blocked under current macOS / current game build

- **Proposed:** 2026-05-28 (carried from old session 5 — well-established but should be smoke-tested)
- **Why we think this:** macOS code signing enforced; old `mprotect()` attempts returned EPERM.
- **How to test:** Attempt `mprotect(addr & ~PAGE_MASK, 4096, PROT_READ|PROT_WRITE|PROT_EXEC)` on a `__TEXT` page; expect EPERM.
- **Status:** open
- **Owner:** researcher

### H-005: The TweakDB singleton getter / flat accessor exists as an unnamed internal function, locatable by xref (not by symbol or dlsym)

- **Proposed:** 2026-05-29 by Scope (researcher)
- **Why we think this:** F-006 established the binary is not export-stripped (67,928 exports) but exposes no TweakDB singleton getter / `GetFlat` / `SetFlat` symbol, and Ghidra auto-analysis named no such function (decompiled as `FUN_*`). The functions clearly exist (the game reads/writes flats at runtime); they just lack names. The reachable anchors are: (a) `game::data::TweakDBID` RTTI/type-object symbols which ARE exported (e.g. `__ZGVZ13GetTypeObjectIN4game4data9TweakDBIDE...`), and (b) string references in the decomp. An xref from these to their callers should surface the accessor and the singleton-init path.
- **How to test:** In Ghidra, take an exported `game::data::TweakDBID` type-object or a TweakDB-related string, follow xrefs to find the function(s) that construct/consult the TweakDB singleton. Record the static address of the most stable candidate (singleton getter or the init/load function). Cross-check by converting to runtime (`static + slide`, F-004) and reading the bytes via a probe (depends on T-002b validating the slide formula first).
- **Status:** open
- **Owner:** researcher
- **Depends on:** T-002b (slide-formula validation, F-004) being empirically confirmed first.
