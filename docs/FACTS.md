# FACTS.md — Validated Truths Only

**Owner:** `researcher` agent (only researcher may add entries; only `doc-keeper` may invalidate).

This file contains only **validated, reproducible** facts about the project, the game, and the build environment. Hypotheses live in `HYPOTHESES.md`. Lessons learned about what doesn't work live in `FAILED_APPROACHES.md`.

---

## How to Add an Entry

```markdown
### F-NNN: <one-line claim>

- **Date:** YYYY-MM-DD
- **Game version:** <from Info.plist or N/A>
- **Evidence:** <commands run, log excerpts, screenshots, ghidra references>
- **How to re-verify:** <exact commands or steps a fresh agent can run>
- **Invalidates:** <prior F-NNN if any, else "none">
```

If you cannot fill all five fields, it's a hypothesis, not a fact.

---

## How to Invalidate an Entry

The `doc-keeper` may mark an entry invalid by appending:

```markdown
- **INVALIDATED YYYY-MM-DD by F-NNN** (or by failed approach FA-NNN): <reason>
```

**Never delete the entry.** Leave it visible with the invalidation note.

---

## Entries

### F-001: Current macOS Cyberpunk 2077 baseline (version, binary hash, entitlements)

- **Date:** 2026-05-29
- **Game version:** 2.3.1 (CFBundleShortVersionString), build 5314028 (CFBundleVersion), SDK build 24C94, built with Xcode 16C5032a
- **Evidence:**
  ```
  # plutil -p "$GAME_DIR/Contents/Info.plist" | grep -iE 'Bundle|Version|Build|MinimumSystem'
  "CFBundleShortVersionString" => "2.3.1"
  "CFBundleVersion" => "5314028"
  "BuildMachineOSBuild" => "24F74"
  "DTPlatformBuild" => "24C94"
  "DTXcodeBuild" => "16C5032a"
  "LSMinimumSystemVersion" => "15.5"

  # shasum -a 256 "$BIN"
  2a6ba5c2847759f7ed896b6bed7b36fe5a7990f75fe9f779383a62adb19d5a15

  # codesign --display --entitlements - "$BIN" 2>&1
  com.apple.security.cs.allow-dyld-environment-variables = true   ← PRESENT
  com.apple.security.cs.disable-library-validation = true         ← PRESENT
  allow-unsigned-executable-memory                                 ← MISSING
  allow-jit                                                        ← MISSING

  # codesign --verify --strict "$BIN" 2>&1
  (exit 0 — signature valid)
  ```
- **How to re-verify:**
  ```bash
  GAME_DIR="$HOME/Library/Application Support/Steam/steamapps/common/Cyberpunk 2077/Cyberpunk2077.app"
  BIN="$GAME_DIR/Contents/MacOS/Cyberpunk2077"
  plutil -p "$GAME_DIR/Contents/Info.plist" | grep -iE 'Bundle|Version|Build|MinimumSystem'
  shasum -a 256 "$BIN"
  codesign --display --entitlements - "$BIN" 2>&1
  codesign --verify --strict "$BIN" 2>&1
  ```
- **Invalidates:** none

**Notes:**
- `disable-library-validation` + `allow-dyld-environment-variables` are PRESENT in the stock binary. These are the two entitlements required for `DYLD_INSERT_LIBRARIES` to work. The binary does **not** need re-signing for DYLD injection to work.
- `allow-unsigned-executable-memory` and `allow-jit` are missing but are NOT required for mod dylib loading — they are needed only for JIT code generation.
- If the binary SHA256 changes in a future session, re-run the entitlement check before assuming anything about prior findings.

---

### F-002: TWEAKXL_MAC_OFFLINE flag enables .tweak parser compilation with stdlib-only deps

- **Date:** 2026-05-29
- **Game version:** N/A (static source analysis, not game-specific)
- **Evidence:**
  - `reference/windows-tweakxl/src/Red/TweakDB/Source/Source.hpp:6` — the only source-level branch on the flag:
    ```cpp
    #ifdef TWEAKXL_MAC_OFFLINE
    #include "mac/CoreStubs.hpp"   // stdlib-only Core::Vector/SharedPtr/WeakPtr stubs
    #else
    #include "Core/Stl.hpp"        // TiltedCore (Windows only)
    #endif
    ```
  - `reference/windows-tweakxl/src/mac/CoreStubs.hpp` — provides `Core::Vector<T>` (std::vector), `Core::SharedPtr<T>` (std::shared_ptr), `Core::WeakPtr<T>` (std::weak_ptr), `Core::MakeShared<T>` (std::make_shared). No RED4ext, no TiltedCore, no platform headers.
  - `reference/windows-tweakxl/src/Red/TweakDB/Source/Grammar.hpp` — uses only `<tao/pegtl.hpp>` (PEGTL, present in `vendor/pegtl/`).
  - `reference/windows-tweakxl/src/Red/TweakDB/Source/Parser.hpp` — uses PEGTL + `Core::SharedPtr` (stubbed by CoreStubs.hpp) + std::filesystem. No RED4ext dependency.
  - `reference/windows-tweakxl/src/Red/TweakDB/Source/Parser.cpp` — same. All `ParseAction<>` specializations use only in-scope PEGTL + TweakSource/TweakGroup/TweakFlat structs.
  - CMakeLists.txt comment confirms flag is defined but parser source is not yet linked into the tweakdb-patcher binary: "Parser/TweakDB integration will be re-enabled once mac stubs are complete."
  - yaml-cpp is included via `pch.hpp` (Windows precompiled header only). The `.tweak` format parser does NOT use yaml-cpp — that is confined to `src/App/Tweaks/Declarative/Yaml/`.
  - `grep -rn 'TWEAKXL_MAC_OFFLINE' reference/windows-tweakxl/` confirmed: flag appears in CMakeLists.txt (definition), build flags.make (definition), Source.hpp (usage), and mac-port-plan.md (documentation). No other usage sites.
- **How to re-verify:**
  ```bash
  cd /Users/lucas_1/Programming/cp2077-mac-mods
  grep -rn 'TWEAKXL_MAC_OFFLINE' reference/windows-tweakxl/
  grep '#include' reference/windows-tweakxl/src/Red/TweakDB/Source/Parser.cpp
  grep '#include' reference/windows-tweakxl/src/Red/TweakDB/Source/Grammar.hpp
  cat reference/windows-tweakxl/src/mac/CoreStubs.hpp
  ```
- **Invalidates:** none

**What the flag enables (summary):**
- Substitutes `Core/Stl.hpp` (TiltedCore) with `CoreStubs.hpp` (stdlib-only aliases).
- Makes the `.tweak` parser layer (`Source/`, `Grammar/`, `Parser/`) compilable with only: C++20 stdlib + PEGTL.
- Does NOT stub or modify `Buffer`, `Manager`, `Reflection`, or any `App/` layer — those remain Windows-only.
- Does NOT affect YAML tweak parsing — that is a separate layer needing yaml-cpp.

**Reuse feasibility for mod-loader:**
Mod-loader can compile `Red::TweakParser` directly from psiberx's source by:
1. Defining `TWEAKXL_MAC_OFFLINE=1`
2. Adding `vendor/pegtl` to include path
3. Compiling `Source/Parser.cpp` with C++20

No additional stubs or changes are needed for the `.tweak` parser layer itself. The CMakeLists.txt linkage step (connecting parser source to the binary target) is the only remaining integration work, and it is explicitly noted as next in the mac-port-plan.

---

### F-003: DYLD_INSERT_LIBRARIES injection works on stock build 2.3.1 (empirical)

- **Date:** 2026-05-29
- **Game version:** 2.3.1 (build 5314028), binary SHA256 `2a6ba5c2...` per F-001
- **Evidence:** The H-001 probe dylib was injected via `DYLD_INSERT_LIBRARIES` into the stock (non-re-signed) binary. Its `__attribute__((constructor))` ran and executed code **before `main()`** — it opened `/tmp/h001-probe.log`, registered a dyld add-image callback, and that callback fired synchronously for the already-loaded main image. Log excerpt:
  ```
  [H001] 1780036022.925472000  dylib loaded            (= 2026-05-29 08:27:02 local)
  [H001] log = /tmp/h001-probe.log
  [H001] --- Cyberpunk2077 image detected ---
  [H001]   path    = .../Cyberpunk2077.app/Contents/MacOS/Cyberpunk2077
  [H001]   base    = 000000010285c000
  ...
  [H001] dyld image callback registered
  ```
  This empirically confirms F-001's static conclusion: the stock binary's `disable-library-validation` + `allow-dyld-environment-variables` entitlements are sufficient for DYLD injection. No re-signing was required.
- **How to re-verify:**
  ```bash
  cd /Users/lucas_1/Programming/cp2077-mac-mods
  tools/run-h001-probe.sh    # builds libh001_probe.dylib, injects via DYLD_INSERT_LIBRARIES, tails /tmp/h001-probe.log
  # Success = "[H001] dylib loaded" + at least one "image detected" block in the log.
  ```
- **Invalidates:** none

**⚠ Provenance caveat (verified, not assumed):** The `/tmp/h001-probe.log` analyzed for this fact was produced by an **earlier build** of the probe, not the current `src/red4ext-mac/src/probes/h001_probe.cpp`. Evidence: the log's first line timestamps the run at 08:27:02, but the current `libh001_probe.dylib` was rebuilt at 08:27:18 (16s later); the log emits `--- Cyberpunk2077 image detected ---` while the current source/dylib emit `========== Cyberpunk2077 binary detected ==========`; the log contains no symtab scan or `__DATA` dump, both of which the current source performs; the log shows 4 dlsym candidates while the current source has 5. **The injection-works conclusion (F-003) is build-independent and holds.** But the current, richer probe (symtab scan + `__DATA`/`__DATA_CONST` dispatch-table dump — the actual H-001 test) has **not yet been run** and produced no data in this log.

---

### F-004: Cyberpunk2077 Mach-O arm64 static base is 0x100000000; runtime address = static + ASLR slide

- **Date:** 2026-05-29
- **Game version:** 2.3.1 (build 5314028)
- **Evidence:** Probe log reported `base = 0x10285c000`, `slide = 0x285c000` for this launch. `base - slide = 0x10285c000 - 0x285c000 = 0x100000000`, the canonical arm64 Mach-O static load address. Confirmed independently: `nm -gU` on the binary reports `_AlignedMalloc` at static `0x100fa6e08` (a static vmaddr in the `0x1` range), consistent with a `0x100000000` static base.
- **Conversion formulae:**
  - `runtime_addr = ghidra_static_addr + slide`
  - `ghidra_static_addr = runtime_addr - slide`
  - **The slide is per-launch (ASLR) and will differ on every run.** On *this* run slide = `0x285c000`. Do NOT reuse this value across launches.
  - To recompute slide on any new run: read the `base` value from that run's probe log, then `slide = base - 0x100000000`. (The probe also logs `slide` directly.)
- **How to re-verify:**
  ```bash
  # From any probe-log run:
  grep -E 'base|slide' /tmp/h001-probe.log
  # Confirm base - slide == 0x100000000 (python3 -c 'print(hex(0x10285c000 - 0x285c000))')
  ```
- **Invalidates:** none

---

### F-005: Main-binary __TEXT segment spans 0x6de0000 bytes (112,512 KB) above the load base on 2.3.1

- **Date:** 2026-05-29
- **Game version:** 2.3.1 (build 5314028)
- **Evidence:** Probe log: `__TEXT = [000000010285c000, 000000010963c000)  size=112512K`. Span = `0x10963c000 - 0x10285c000 = 0x6de0000` = 115,212,288 bytes = 112,512 KB.
- **Derived static __TEXT range:** `[0x100000000, 0x106de0000)` (i.e. `[static_base, static_base + 0x6de0000)`).
- **Use:** Upper bound for an "is this runtime address inside the main binary's `__TEXT`?" check: `text_lo <= addr < text_lo + 0x6de0000`, where `text_lo = base` for that run. This is how the probe flags qword values as `CODE` (function pointers) vs data.
- **How to re-verify:** `grep '__TEXT' /tmp/h001-probe.log`, or `otool -l "$BIN" | grep -A4 '__TEXT'` and read `vmsize`. Re-check after any game update — segment size will change.
- **Invalidates:** none

---

### F-006: Stock binary is NOT export-stripped — 67,928 dynamic symbols exported; no TweakDB singleton/flat accessor among them

- **Date:** 2026-05-29
- **Game version:** 2.3.1 (build 5314028)
- **Evidence:** `nm -gU "$BIN" | wc -l` → **67,928** externally-defined exported symbols (68,654 total incl. local). The export table contains 136 `Tweak`-related **function** (`T`) symbols, e.g. `game::data::TweakDBID` helpers, `red::memory::PoolStorageProxy<AI::PoolAI_TweakActions>` methods, `game::data::TweakDBRecord`/`TweakDBReloader`/`TweakDBErrorLog` `DynArray` instantiations, and `TweakDBResource` text-format resource-loader closures. **However, none of the exported symbols is a TweakDB singleton getter, `GetFlat`, or `SetFlat`** — those are internal (no exported dynamic-symbol name) or inlined. A bounded grep of the Ghidra decompilation (`reference/ghidra/Cyberpunk2077.c`, `.h`) for named `TweakDB::Get`/`GetFlat`/`SetFlat`/`Load` returned nothing (only unrelated `GetFlatTireIndex`, `GetFlattenedMessages`); the `.h` has only 15 `TweakDB` hits, all `StateContextParametersBase::*TweakDBID` state-machine accessors.
- **Why this matters:** This **corrects** the prior assumption (probe log / Conductor obs #5) that "the binary strips C++ exports." It does not. dlsym failed for a different reason — see FA-009. Finding the real TweakDB accessor requires xref analysis (from `game::data::TweakDBID` RTTI or string references), not a symbol lookup. See H-005.
- **How to re-verify:**
  ```bash
  BIN="$HOME/Library/Application Support/Steam/steamapps/common/Cyberpunk 2077/Cyberpunk2077.app/Contents/MacOS/Cyberpunk2077"
  nm -gU "$BIN" | wc -l                       # ~67928
  nm -gU "$BIN" | awk '$2=="T"' | grep -ic tweak   # 136
  nm -gU "$BIN" | awk '$2=="T"' | grep -iE 'GetFlat|SetFlat|TweakDB.*Get'   # (no singleton/flat accessor)
  ```
- **Invalidates:** none (corrects an informal assumption that was never promoted to a FACT)

---
