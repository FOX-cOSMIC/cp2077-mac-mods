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

### F-007: ASLR slide differs per launch — empirically confirmed across two runs

- **Date:** 2026-05-29
- **Game version:** 2.3.1 (build 5314028)
- **Evidence:** Two probe runs of the same binary on the same day produced different load bases / slides:
  - Run 1 (08:27:02): `base = 0x10285c000`, `slide = 0x285c000` (F-003/F-004)
  - Run 2 (08:44:28, v2 probe): `base = 0x100b10000`, `slide = 0xb10000` (`docs/probes/logs/h001-probe-2026-05-29-v2.log:5-6`)
  - Both satisfy `base - slide = 0x100000000` (F-004's static base). Both report identical `__TEXT` size (112,512 KB), confirming it's the same binary at a different load address.
- **Consequence:** F-004's "the slide is per-launch; re-extract it every run" rule is now empirically validated, not merely theoretical. **Any static→runtime address conversion must use the slide from that specific run's probe log.** Never cache a runtime address across launches.
- **How to re-verify:** Run `tools/run-h001-probe.sh` twice (separate game launches); compare `base`/`slide` lines. They will differ; `base - slide` will be `0x100000000` both times.
- **Invalidates:** none (strengthens F-004)

---

### F-008: game::data::TweakDB class confirmed in binary; anchor addresses for xref hunt (symbols are DATA-only, no function entry points)

- **Date:** 2026-05-29
- **Game version:** 2.3.1 (build 5314028)
- **Evidence:** The v2 probe's LC_SYMTAB scan (first 20,000 of 68,655 symbols) surfaced ~62 `Tweak`-related symbols confirming the engine type `game::data::TweakDB` with templated `GetRecords<T>()` const member methods (Vehicle, Attitude, District, LifePath, Character, and 26+ other record types), plus RTTI for `TweakDBID`, `TweakDBType`, `TweakDBRecord`, `TweakDBInterface`, `TweakDBResource`. **Verified independently with `nm`.** All static addresses below confirmed present in the binary (static base `0x100000000`; add the per-run slide for runtime):

  | Static addr | Symbol | Use as xref anchor |
  |---|---|---|
  | `0x1073af788` | `__ZZ13GetTypeObjectIN4game4data9TweakDBIDEE...E8rttiType` (the `rttiType` pointer static) | RTTI type-object for `game::data::TweakDBID`; multi-referenced hub — many call sites read it |
  | `0x1073af790` | `__ZGVZ13GetTypeObject...TweakDBID...rttiType` (its init guard) | adjacent guard for the same static |
  | `0x1073bbea8` | `__ZZNK4game4data7TweakDB10GetRecordsINS0_14Vehicle_RecordEE...E12emptyRecords` | **single-xref anchor** → pins `TweakDB::GetRecords<Vehicle_Record>()` const member → reveals TweakDB struct layout |
  | `0x1073eef58` | `__ZGVZ17GetNativeTypeHashIN4game4data13TweakDBRecordEE...nativeTypeHash` | native type-hash for `TweakDBRecord` |
  | `0x1073ea7e0` | `__ZGVZ17GetNativeTypeHashIN4game4data16TweakDBInterfaceEE...nativeTypeHash` | native type-hash for `TweakDBInterface` |
  | `0x1073eeab0` | `__ZGVZN3red17FixedSizeFunction...RegisterGameTweakDBRTTIvE...` | guard for `RegisterGameTweakDBRTTI()` — RTTI registration entry anchor |

- **Critical nuance (corrects an over-optimistic reading of the symtab):** Every TweakDB-related symbol in the table is **DATA** (`D`) — an Itanium static-init guard (`__ZGVZ...`) or a function-local static (`__ZZ...emptyRecords` / `__ZZ...rttiType`). There are **ZERO** function (`T`/`t`) symbols for `game::data::TweakDB` — confirmed: `nm "$BIN" | awk '$2=="T"||$2=="t"' | grep -cE '_ZNK?4game4data7TweakDB[0-9]'` → **0**. The actual function entry points are inlined / internal-linkage and have no name; they are **not** hiding in the unscanned 48,655 symbols. Therefore the path to the functions is **xref from these DATA statics**, not "scan more symbols." A function-local static (e.g. `emptyRecords` at `0x1073bbea8`) is referenced by exactly one function — its owning method — making it the cleanest single-xref entry point.
- **How to re-verify:**
  ```bash
  BIN="$HOME/Library/Application Support/Steam/steamapps/common/Cyberpunk 2077/Cyberpunk2077.app/Contents/MacOS/Cyberpunk2077"
  nm "$BIN" | grep -E '1073af788|1073bbea8|1073eef58'         # the statics exist
  nm "$BIN" | awk '$2=="T"||$2=="t"' | grep -cE '_ZNK?4game4data7TweakDB[0-9]'   # → 0 functions
  nm "$BIN" | grep 'NK4game4data7TweakDB10GetRecords' | grep -v ZGV | wc -l       # → 31 GetRecords data statics
  ```
- **Invalidates:** none (supersedes nothing; refines F-006's "no accessor symbol" with positive anchor addresses)

---

### F-009: Authoritative symbol counts — 68,655 LC_SYMTAB entries total; 67,928 externally-defined

- **Date:** 2026-05-29
- **Game version:** 2.3.1 (build 5314028)
- **Evidence:** The two numbers in play measure different things and are both correct:
  - **68,655** = total `LC_SYMTAB.nsyms` (every symbol-table entry: defined + undefined + local), as reported by the probe (`...v2.log:11`). `nm "$BIN" | wc -l` ≈ 68,654 (nm filters one debug/null entry).
  - **67,928** = externally-defined symbols only, `nm -gU "$BIN" | wc -l` (F-006).
  - Difference (~727) = undefined imports + entries `nm -gU` excludes. No contradiction.
- **For "is the binary stripped?" the authoritative figure is the export count (67,928, F-006): the binary is not stripped.** For "how many symbols must a full-symtab scan walk," the figure is 68,655.
- **How to re-verify:** `nm "$BIN" | wc -l` (≈68,654 total); `nm -gU "$BIN" | wc -l` (67,928 exported).
- **Invalidates:** none (reconciles F-006's count with the probe's reported total)

---

### F-010: __DATA begins with Objective-C runtime metadata, not game data; probe's "CODE" flag false-positives on cstring pointers

- **Date:** 2026-05-29
- **Game version:** 2.3.1 (build 5314028)
- **Evidence (v2 log):**
  - `__DATA` dump from its base (`vmaddr=0x107394000`, slid `0x107ea4000`): the first 64 qwords are Objective-C selector/method metadata — readable ASCII at the pointed-to bytes spells `isEqual:`, `performSelector:`, `isKindOfClass:`, `respondsToSelector:`, `retain`/`release`/`autorelease`, and ObjC type-encodings (`@16@0:8`, `B24@0:8@16`). This is the `__objc_*` region at the head of `__DATA`. No TweakDB structures in this window (`...v2.log:76-141`).
  - `__DATA_CONST` dump from its base (`vmaddr=0x106de0000`, slid `0x1078f0000`): pointers into `0x18xxxxxxxx`/`0x19xxxxxxxx` (consistent with the dyld shared cache / system frameworks) and into `0x10b8dxxxx` (other in-process mappings); 0 flagged as main-`__TEXT` code. This is the GOT/bind-pointer head of `__DATA_CONST`, not game data (`...v2.log:142-207`).
  - **Probe heuristic caveat:** the 41 entries flagged `CODE` in the `__DATA` dump are **false positives** — their values fall inside the `__TEXT` address range but point to C-string/selector data (in `__TEXT,__cstring`/`__objc_methname`), not function prologues (the dumped bytes are ASCII, not ARM64 instructions). The probe's "value ∈ `__TEXT` ⇒ CODE" test does not distinguish code from read-only strings living in `__TEXT`.
- **Guidance:** A v3 probe must NOT dump from segment base offset 0 to find TweakDB data — that window is ObjC/GOT boilerplate. Target by address (TweakDB statics from F-008 + slide) instead. And treat the `CODE` flag as "points into `__TEXT`," not "is a function" — disambiguate by checking for an ARM64 prologue (e.g. `stp`/`sub sp`) vs printable ASCII.
- **How to re-verify:** re-run `tools/run-h001-probe.sh`; inspect the `--- __DATA dump ---` and `--- __DATA_CONST dump ---` sections of `/tmp/h001-probe.log`.
- **Invalidates:** none

---
