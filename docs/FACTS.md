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

### F-011: TweakDB singleton access path — getter, global pointer, and initializer (resolves H-005)

- **Date:** 2026-05-29
- **Game version:** 2.3.1 (build 5314028)
- **Method:** Static analysis of `reference/ghidra/Cyberpunk2077.txt` (full disassembly listing), entered from the F-008 anchor `0x1073bbea8` (the `emptyRecords` static of `TweakDB::GetRecords<Vehicle_Record>`). All addresses below are **static** (Ghidra base `0x100000000`); runtime = static + slide (F-004), slide per-launch (F-007).
- **The singleton getter — `FUN_102b73c7c` @ static `0x102b73c7c`:**
  ```
  adrp x19,0x1080c9000 ; add x19,#0x2d0   ; x19 = &g_TweakDB  (= 0x1080c92d0)
  ldapr x8,[x19] ; cbz x8 -> init         ; acquire-load the global; if null, init
  ldapr x0,[x19] ; ret                    ; return *g_TweakDB in x0
  init: bl FUN_102b73b50 ; ldapr x0,[x19] ; ret
  ```
  Returns `game::data::TweakDB*` in x0. Called from thousands of sites (central accessor); confirmed it is the `this`-source for `GetRecords` (the caller at `0x10121c75c` does `bl FUN_102b73c7c` immediately before `bl FUN_10121f264`).
- **The global singleton pointer — static `0x1080c92d0`, in `__bss`** (Ghidra label `__bss:DAT_1080c92d0`; writable, zero-initialized). Holds the heap `TweakDB*`. **Reading `*(0x1080c92d0 + slide)` at runtime yields the live TweakDB instance — no code hook required to obtain it.**
- **The initializer — `FUN_102b73b50` @ static `0x102b73b50`:** allocates the instance (`mov w0,#0x168; bl red::memory::Pool…Allocate`), `bzero`s it, runs the constructor `FUN_102b73db8` @ static `0x102b73db8`, then `swpal` (atomic store) the pointer into `0x1080c92d0`. Note: this constructs an **empty** TweakDB — record/flat data is populated by a *later* load path (not in this function; see FOLLOW-UPS), so this is the lifecycle-create point, not the mods-apply point.
- **How to re-verify:**
  ```bash
  cd reference/ghidra
  grep -n -m1 '__text:102b73c7c' Cyberpunk2077.txt    # getter entry; body reads 0x1080c92d0
  grep -n -m1 '__text:102b73b50' Cyberpunk2077.txt    # initializer; 'mov w0,#0x168' + Allocate
  grep -n -m6 '1073bbea8' Cyberpunk2077.txt           # anchor xref into GetRecords (F-012)
  ```
- **Invalidates:** none (resolves H-005)

---

### F-012: `TweakDB::GetRecords<Vehicle_Record>` = `FUN_10121f264` @ static 0x10121f264; reveals records hash-map layout

- **Date:** 2026-05-29
- **Game version:** 2.3.1 (build 5314028)
- **Evidence:** `FUN_10121f264` references both the `emptyRecords` static `0x1073bbea8` (F-008 anchor 1) and its `__cxa_guard_acquire` guard `0x1073bbeb8`; the "no records" branch lazily constructs and returns that empty `red::DynArray`. This identifies it as `game::data::TweakDB::GetRecords<Vehicle_Record>() const` (returns `red::DynArray<THandle<TweakDBRecord>> const&`; `this` in x0). Its body performs an FNV-1a hash (basis `0x811c9dc5`, prime `0x01000193`) of the lookup key and indexes a hash table on `this`:
  - `+0x88` — pointer to hash **bucket-index array** (uint32 entries; `ldr x10,[x19,#0x88]`, indexed `[x10, w9, UXTW #2]`)
  - `+0x90` — entry **count** / non-empty flag (uint32; `cbz` → empty-records path)
  - `+0x94` — **bucket count** (uint32; used as `udiv` modulus on the hash)
  - `+0x98` — pointer to **entries array** (records map storage)
  - `+0xa4` — **entry stride** (uint32; `mul w11,w11,w10` to index entries)
  - Entry shape (walked at `0x10121f328`): `+0x00` next-index, `+0x04` stored hash (uint32), `+0x08` key/TweakDBID (8 bytes), payload follows.
- **Callers (xrefs):** static `0x10121c760`, `0x101537164`, `0x1035ef734` — each obtains `this` from the singleton getter `FUN_102b73c7c` (F-011).
- **How to re-verify:** `awk 'NR>=5974950 && NR<=5975110' reference/ghidra/Cyberpunk2077.txt` (function body around the `0x1073bbea8`/`0x1073bbeb8` references).
- **Invalidates:** none

---

### F-013: macOS TweakDB struct is 0x168 bytes; records storage is a plain hash table (no function-pointer dispatch) — DISPROVES H-002

- **Date:** 2026-05-29
- **Game version:** 2.3.1 (build 5314028)
- **Evidence:** The singleton initializer `FUN_102b73b50` (F-011) allocates the TweakDB instance with `mov w0,#0x168` immediately before `bl red::memory::Pool…Allocate(ulonglong)`, then `bzero`s the `0x168`-byte block and constructs it in place. The constructed pointer is stored into the global `0x1080c92d0` and is the exact object `GetRecords` (F-012) dereferences — so the records hash-map fields at `+0x88…+0xa4` lie within this `0x168`-byte struct. Consistent.
- **Two corrections to prior assumptions:**
  1. **Struct size is `0x168` bytes (360), the SAME as the Windows layout — NOT `0x198`.** This directly disproves H-002 (which claimed macOS uses `0x198`, differing from Windows's `0x168`). The old session-20 "0x198" figure is wrong on build 2.3.1. → see FA-010, H-002 marked resolved-failed.
  2. **Records storage is a conventional hash table** (bucket-index array + entries array + stride, F-012), read by inline code — **not** a function-pointer dispatch table. This corroborates FA-003's "hash table, not sorted array" observation and weakly counter-indicates H-001 (function-pointer dispatch) for the *records* map. (H-001 concerns *flats* specifically and remains open — flats map not yet examined; see FOLLOW-UPS.)
- **How to re-verify:**
  ```bash
  cd reference/ghidra
  L=$(grep -n -m1 '__text:102b73b50' Cyberpunk2077.txt | cut -d: -f1)
  awk -v s=$((L+25)) -v e=$((L+40)) 'NR>=s&&NR<=e' Cyberpunk2077.txt   # shows 'mov w0,#0x168' + Allocate + swpal to 0x1080c92d0
  ```
- **Invalidates:** none added to FACTS; disproves hypothesis H-002 (see FA-010)

---

### F-014: Candidate hook/access targets for TweakDB runtime modification (for hook-engineer)

- **Date:** 2026-05-29
- **Game version:** 2.3.1 (build 5314028)
- **All addresses static (base 0x100000000); apply per-launch slide (F-004/F-007).**

  | # | Target | Static addr | What it is | Suggested approach |
  |---|--------|-------------|-----------|--------------------|
  | 1 | Global singleton pointer | `0x1080c92d0` (`__bss`) | Live `TweakDB*` handle | **Read-only, no hook.** Read `*(0x1080c92d0+slide)` to get the live instance, then operate on its hash maps. `__bss` is writable but the *pointer target* is what we use. FA-001-compliant (no `__TEXT` write). |
  | 2 | Singleton initializer | `FUN_102b73b50` @ `0x102b73b50` | Allocates+constructs the (empty) DB once | Lifecycle anchor only — DB is empty here; **not** the mods-apply point. |
  | 3 | `GetRecords<T>` | `FUN_10121f264` @ `0x10121f264` | Records hash-map query | Reference for struct layout (F-012); a read-path, not a write target. |

- **Hooking constraints (carry-forward):** Per FA-001, `__TEXT` is immutable — no inline patching. These functions are reached by **direct `bl`** (not GOT-indirected), so a GOT hook does **not** apply to intra-image direct calls. Viable FA-001-compliant strategies for hook-engineer to evaluate:
  - **(a) Direct data manipulation** via target #1: read the global, walk/modify the records/flats hash maps in the heap instance. Needs the flats-map layout + value-storage format first (next research step).
  - **(b) VTable hook** *iff* TweakDB has a vtable: the constructor `FUN_102b73db8` would write a vtable pointer at `this+0x00` — if so, the vtable lives in `__DATA_CONST`/`__DATA` and a virtual method (e.g. a load/reload entry) can be swapped without touching `__TEXT`. **Unverified** — requires inspecting `FUN_102b73db8` (not yet done).
  - **The "when to apply mods" signal is still unknown** — the data-load/reload path that populates the empty DB is not yet located (see FOLLOW-UPS). Mods must be applied *after* that load, not at target #2.
- **How to re-verify:** addresses cross-checked against F-011/F-012 evidence; re-run those greps.
- **Invalidates:** none

---

### F-015: Complete TweakDB struct field map (from constructor FUN_102b73db8) — 0x168 bytes, three parallel hash maps + flat-data-buffer region

- **Date:** 2026-05-29
- **Game version:** 2.3.1 (build 5314028)
- **Method:** Disassembled the constructor `FUN_102b73db8` @ static `0x102b73db8` (T-002d), addressing every `str`/`strh`/`strb`/`stur` to `this` (x19). Destructor `FUN_102b73fa8` @ `0x102b73fa8` cross-checked the container teardown/hash semantics. All static addresses (base 0x100000000; +slide for runtime, F-004/F-007).
- **Top-level field map (offset → field → notes):**

  | Offset | Field | Evidence / notes |
  |---|---|---|
  | `+0x00..0x1F` | zeroed | `stp q0,q0,[x0]`, q0=0 — **vtable slot is ZERO, no vtable (F-016)** |
  | `+0x20` | u16 = 0 | `strh wzr,[x0,#0x20]` (atomic/refcount field; dtor touches +0x20/+0x21) |
  | `+0x28` | ptr → heap **0x158-byte** sub-object | `mov w0,#0x158; Allocate; bzero; thunk_FUN_102b7b084(init); str x20,[x19,#0x28]` — largest sub-alloc; **flat-value pool / manager candidate** |
  | `+0x30` | ptr → heap **0xf8-byte** sub-object | `mov w0,#0xf8; Allocate; bzero; FUN_102b76e78(init); str x20,[x19,#0x30]` — secondary manager |
  | `+0x38` | u8 = 1 | `mov w8,#1; strb w8,[x19,#0x38]` — flag (owns/initialized) |
  | `+0x40` | embedded sub-object | `add x20,x19,#0x40; FUN_102b03e88; FUN_1000285e0` (lock/small container) |
  | `+0x4c` | 8 bytes = 0 | `stur d0,[x19,#0x4c]` |
  | **`+0x58`** | **hash map A** (bucket array; entries +0x68; sentinel +0x78=0xffffffff; allocator +0x80) | structurally identical to records — **flats-map candidate (inferred, see F-017)** |
  | **`+0x88`** | **hash map B = RECORDS** | F-012: bucket +0x88, count +0x90, bucketcount +0x94, entries +0x98, stride +0xa4; sentinel +0xa8; allocator +0xb0 |
  | `+0xb8` | embedded sub-object | `add x20,x19,#0xb8; FUN_102b05460; FUN_1000285e0` |
  | `+0xc4` | u32 = 0 | |
  | `+0xc8` | embedded **small hash map** | `FUN_1000285e0`; dtor frees entries of size 0x8/align 4 (HashMap<key,u32>-like); count +0xd4 |
  | `+0xe0` | embedded sub-object | `FUN_102b05ba8; FUN_1000285e0` |
  | `+0xec` | u32 = 0 | |
  | `+0xf0` | embedded container | `FUN_1000285e0`; +0xfc = 0 |
  | **`+0x108`** | **hash map C** (bucket array +0x108; count +0x114; entries +0x118; sentinel +0x128; allocator +0x130) | dtor resets buckets to 0xffffffff, frees entries size 0x20; **queries-map candidate (inferred)** |
  | `+0x138` | embedded container | `FUN_102b028b0; FUN_1000285e0`; count +0x144 |
  | `+0x148` | ptr = 0 | `str xzr,[x19,#0x148]` — **flat-data-buffer pointer** (mirrors Windows flatDataBuffer@0x148; filled at load time, not in ctor) |
  | `+0x150` | u32 = 0 | buffer size/end |
  | `+0x158` | 8 bytes = 0 | buffer capacity/ptr |
  | `+0x160` | u8 = 0 | |
  | `+0x164` | u32 = 0 | last field; struct ends at **0x168** |

- **Confirmed:** struct = 0x168 (F-013); records map at +0x88 (F-012); **three structurally-identical hash maps** at +0x58 / +0x88 / +0x108 (same bucket-index-array + entries + `0xffffffff` empty-bucket sentinel template).
- **How to re-verify:** `L=$(grep -n -m1 '__text:102b73db8' reference/ghidra/Cyberpunk2077.txt | cut -d: -f1); awk -v s=$L -v e=$((L+125)) 'NR>=s&&NR<=e' reference/ghidra/Cyberpunk2077.txt`
- **Invalidates:** none (extends F-012/F-013)

---

### F-016: TweakDB has NO vtable — VTable hooking is not viable (finalizes F-014b)

- **Date:** 2026-05-29
- **Game version:** 2.3.1 (build 5314028)
- **Evidence:** The constructor `FUN_102b73db8`'s **first** memory operation on `this` is `movi v0.2D,#0x0` then `stp q0,q0,[x0]` — it writes 32 zero bytes to `this+0x00..0x1F`. `this+0x00` (the C++ vtable slot for a polymorphic class) is therefore set to **0**, and no code pointer is written to `this+0x00` (or any other offset) anywhere in the constructor. The initializer `FUN_102b73b50` (F-011) calls this constructor directly with no derived-class wrapper. As constructed, the TweakDB object is non-polymorphic: no vtable pointer is installed.
- **Consequence for hook-engineer:** **F-014 approach (b) (VTable hook) is NOT viable** — there is no vtable to swap. This also retires the old VTABLE-slot line of investigation (H-003, FA-004, FA-005) for *this* object: TweakDB methods are not dispatched through a vtable. The remaining FA-001-compliant path is **direct data manipulation** via the global singleton pointer (F-014 approach (a)).
- **How to re-verify:** inspect the first ~3 instructions of `FUN_102b73db8` (lines after entry): `movi v0.2D,#0x0; stp q0,q0,[x0]`.
- **Invalidates:** none (finalizes F-014's approach (b) as non-viable)

---

### F-017: TweakDB flats use plain hash-table + data-buffer storage (NOT a function-pointer dispatch table) — resolves H-001 and H-006

- **Date:** 2026-05-29
- **Game version:** 2.3.1 (build 5314028)
- **Evidence (all from the constructor F-015 + records function F-012):**
  1. The struct contains **three structurally-identical hash maps** (+0x58, +0x88=records, +0x108), each a plain data hash table: a bucket-index array of `uint32` (empty = `0xffffffff`), an entries array, a bucket count, and an entry stride — indexed by an FNV-1a hash of the TweakDBID (F-012). No entry is a code pointer; lookup is inline arithmetic, not a call through a pointer table.
  2. A **flat-data-buffer region at +0x148** (pointer +0x148, size +0x150, capacity +0x158) holds the actual flat *values* — mirrors the Windows `flatDataBuffer@0x148`. Zeroed in the constructor; populated at load time.
  3. The constructor writes **no function pointers** into the struct (F-016): `this+0x00` is zeroed; every initialized field is a data value, an embedded data container, or an allocator pointer.
- **Resolves H-001 (DISPROVED):** the old theory that TweakDB storage is a "function-pointer dispatch table" is wrong on build 2.3.1. Flats (and records, and queries) are stored in conventional hash tables backed by a flat-value buffer — plain data, no code-pointer dispatch. The old session-62 "staticFlatDataBuffer contained code pointers" reading was mistaken (it likely dumped a region of embedded sub-object/allocator pointers or misattributed the slide). → see FA-011.
- **Resolves H-006 (CONFIRMED in substance):** flats ARE stored in a hash map parallel to the records map (the records template repeats; F-015), plus the +0x148 flat-value buffer. Storage scheme = **hash table + value buffer**, same family as records.
- **One detail still open (narrowed, NOT drift-resolved here):** *which* of the two non-records hash maps is specifically **flats** vs **queries** — block A (+0x58) vs block C (+0x108) — is **inferred** (records=+0x88 is the middle of three; RED's declaration order is flats, records, queries → flats=+0x58, queries=+0x108) and is consistent with the Windows analogy, but is **not** directly confirmed by a macOS `GetFlat` accessor disassembly. Per FA-006, do not treat the +0x58=flats assignment as fact until a `GetFlat`/flat-read function is xref'd to it. This is a one-step follow-up (T-002e).
- **How to re-verify:** F-015 / F-012 re-verify commands; confirm no `bl`-through-pointer in GetRecords and no code pointer written by the constructor.
- **Invalidates:** none in FACTS; disproves H-001 (see FA-011)

---

### F-018: TweakDB initial-load/populate path — orchestrator FUN_102b75744; load completion is the mods-apply point (resolves H-007)

- **Date:** 2026-05-29
- **Game version:** 2.3.1 (build 5314028)
- **Method:** From the `"tweakdb.bin"` string xref in the TweakDB TU (Cyberpunk2077.txt). All addresses static (base 0x100000000; +slide, F-004/F-007).
- **Call chain (confirmed by string + structure):**
  - **`FUN_102b75744` @ static `0x102b75744`** — TweakDB initial-load orchestrator. Calls the loader, times the load (`_mach_absolute_time`), then dispatches parse/populate (`bl FUN_102253964` @ `0x102b75908`, plus virtual `blr` calls into the loaded resource at `[obj]+0x20/+0x30/+0x50`). **Single caller: `0x10a42f7de`** (engine init/static-ctor chain). When this function returns, the TweakDB singleton (`0x1080c92d0`, F-011) is populated.
  - **`FUN_102b75b48` @ static `0x102b75b48`** — path builder + file loader: assembles `{root}/tweakdb.bin` (string `"tweakdb.bin"` @ cstring `0x106d2bd9b`; also a `dlc\` variant via `"dlc"`/`"dlc\\"`), then `bl FUN_1021c90a4` (resource open/read).
  - **`FUN_102253964`** — parse/insert routine that fills the flats/records/queries maps from the loaded buffer (heavy populate).
- **Apply-mods trigger (the H-007 answer):** the **completion of `FUN_102b75744`** is the earliest point at which the DB is fully populated and safe to mod. The constructor `FUN_102b73b50`/`FUN_102b73db8` (F-011/F-015) builds an EMPTY DB and must NOT be used as the trigger (confirmed: populate happens here, separately, later).
- **Recommended hook mechanism for hook-engineer (FA-001-compliant):** these load functions are reached by **direct `bl`** (not GOT-indirected) and TweakDB has **no vtable** (F-016), so neither GOT nor VTable hooking applies, and `__TEXT` is immutable (FA-001). Practical options, in order of robustness:
  1. **Poll the singleton** from the injected dylib: after launch, wait until `*(0x1080c92d0+slide) != null` and its flats-map entry count is non-zero/stable, then apply mods via direct data manipulation (F-014a). Simple, no hook.
  2. Investigate the **TweakDBReloader** path (symbols exist, F-008) — designed for hot-reload, it may expose a re-entrant, indirectly-dispatched trigger that is cleaner to intercept. Follow-up.
- **How to re-verify:**
  ```bash
  cd reference/ghidra
  grep -n -m1 's_tweakdb.bin' Cyberpunk2077.txt            # cstring + xref 0x102b75c44 (inside FUN_102b75b48)
  L=$(grep -n -m1 '__text:102b75744' Cyberpunk2077.txt|cut -d: -f1); awk -v s=$L -v e=$((L+60)) 'NR>=s&&NR<=e' Cyberpunk2077.txt
  ```
- **Invalidates:** none (resolves H-007)

---

### F-019: The +0x58 hash map is a confirmed peer of the records map (flats candidate); flats-vs-queries block label still pending one accessor xref

- **Date:** 2026-05-29
- **Game version:** 2.3.1 (build 5314028)
- **Evidence:** The +0x58 map's dedicated destructor `FUN_100ad1df0` (called from the TweakDB destructor for the +0x58 sub-object) walks a hash table with sub-layout **identical to the records map** (F-012): bucket-index array @ block+0x00, count @ +0x08, bucket-count @ +0x0c, entries @ +0x10, entry stride @ +0x1c; it iterates entries and releases a ref-counted payload at entry+0x18. So +0x58, +0x88 (records), and +0x108 are three structurally-identical TweakDBID-keyed hash maps (F-015), and +0x58/+0x88 hold ref-counted (handle/value) payloads with dedicated complex destructors, while +0x108 is torn down inline.
- **What is NOT yet proven:** which of +0x58 / +0x108 is specifically **flats** vs **queries**. Convergent structural evidence favors **+0x58 = flats, +0x108 = queries** (records confirmed = +0x88 middle; RED declaration order flats<records<queries; the flat-value buffer sits at +0x148 after all three maps; +0x58 mirrors records' heavyweight payload). **This is high-confidence inference, NOT a confirmed fact** — per FA-006, the Windows-order analogy must not be trusted blindly. See H-008 for the decisive test. Hook-engineer must obtain that confirmation before writing flat-write code against a hardcoded +0x58.
- **How to re-verify:** `L=$(grep -n -m1 '__text:100ad1df0' reference/ghidra/Cyberpunk2077.txt|cut -d: -f1); awk -v s=$L -v e=$((L+50)) 'NR>=s&&NR<=e' reference/ghidra/Cyberpunk2077.txt`
- **Invalidates:** none

---
