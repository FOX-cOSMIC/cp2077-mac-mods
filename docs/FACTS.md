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
- **REFINED 2026-05-29 by F-021:** the +0x88 map this function reads is the **records-BY-TYPE index** (key = a record-type id; value = the DynArray of records of that type — exactly what `GetRecords<T>` returns), NOT a record-by-name→record lookup. Looking up an individual record by its own TweakDBID (`Items.Preset_Nova_Default`, 0xb1e27e8e) MISSES this map (runtime-proven, P1.12 probe). The sub-layout above is still correct for the +0x88 map; only the gloss "the records map" (implying by-id record lookup) was imprecise. See F-021/F-022.

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

### F-020: On-disk tweakdb.bin stores record names but NOT flat property names (flats keyed by TweakDBID hash)

- **Date:** 2026-05-29
- **Game version:** 2.3.1 (build 5314028); file `r6/cache/tweakdb.bin`, 42,028,033 bytes, mtime 2026-05-29
- **Evidence:** `strings -8 tweakdb.bin`:
  - 12,951 two-part dotted IDs (record names) — e.g. `Items.Preset_Nova_Default`, `Attacks.MissileProjectile`, `Character.Player_Puppet_Base`, `Vehicle.RotationLimiter`.
  - Only **11** three-part dotted strings, and those are vehicle sub-names (`quadra.type66.avenger`), not flats.
  - **Zero** `Constants.*` strings; no `Player.maxHealth`-style flat paths.
  - So full flat names (`Record.property`) are **not** present as ASCII — flats are stored/keyed by TweakDBID hash, and the `.property` component is reconstructed at runtime from record-type RTTI (+ TweakXL's `ExtraFlats` for non-reflected flats).
- **Consequence:** you cannot enumerate moddable flat names by scanning the binary. To target a flat: (1) confirm the **record** exists via `strings -8 tweakdb.bin | grep -xF '<Record>'`; (2) get a valid **property** from the record's type block in `reference/windows-tweakxl/data/ExtraFlats.yaml` (keys are record TYPE names, verified by `MetadataImporter::ImportExtraFlats` → `IsRecordType()`) or from RTTI; (3) the runtime TweakDB resolves `Record.property` by hash. Whether a specific record↔type binding holds is only confirmable at runtime (apply → `[applicator] mod applied:` vs `reject op: flat not found`).
- **How to re-verify:**
  ```bash
  BIN="$HOME/Library/Application Support/Steam/steamapps/common/Cyberpunk 2077/r6/cache/tweakdb.bin"
  strings -8 "$BIN" | grep -cE '^[A-Za-z][A-Za-z0-9_]+\.[A-Za-z][A-Za-z0-9_]+$'   # ~12951 record names
  strings -8 "$BIN" | grep -cE '^[A-Za-z][A-Za-z0-9_]+\.[A-Za-z0-9_]+\.[A-Za-z][A-Za-z0-9_]+$'  # 11 (not flats)
  strings -8 "$BIN" | grep -c 'Constants'   # 0
  ```
- **Invalidates:** none

---

### F-021: The +0x88 map is the records-BY-TYPE index (GetRecords<T>), NOT a record-by-name store; ~843 = live record-type count

- **Date:** 2026-05-29
- **Game version:** 2.3.1 (build 5314028)
- **Why this was opened:** Schema's P1.12 candidate probe found `Items.Preset_Nova_Default` (CRC32 `0xb1e27e8e`, on-disk 22×) **MISSES** the +0x88 map, and the +0x88 map holds only **843** entries — far below CP2077's ~12,951 record names. F-012 had glossed +0x88 as "the records map," implying by-id record lookup; that gloss is wrong.
- **Evidence:**
  - F-012: the function reading +0x88 is `GetRecords<Vehicle_Record>()` — a **type-parameterized** accessor that returns *all records of a type*. Its lookup key is a record-**type** id (the FNV-1a'd output of `FUN_1026cb278`, the per-type id), not a record's own name-hash. So +0x88 is keyed by type, not by record name.
  - Binary RTTI census: `nm Cyberpunk2077 | grep -oE 'N4game4data[0-9]+[A-Za-z0-9_]+_RecordE' | sort -u | wc -l` → **967** distinct `game::data::*_Record` types (1341 across all namespaces). The live count **843** is a clean subset (record types that have ≥1 instantiated record), matching "records-by-type index" — and nothing like 12,951.
  - Runtime (Schema, P1.12 log): the +0x88 self-test entry has key `{nameHash=0x081122e0, len=1}` and is queried with hash mode **fnv1a-8B** (FNV-1a over the 8-byte key) — consistent with a synthetic short type id, not a `CRC32(fullname)+strlen` record-name TweakDBID.
  - The +0x28 heap sub-object (`FUN_102b7b084`, 0x158 bytes) is a reflection/type registry — it FNV-1a-registers a *fixed static table* of type-name strings (`0x1070d5ff0`, e.g. `"alignas"` @ `0x106d2c9a2`), not a 13K record store. So records-by-name are not there either.
- **Consequence:** to fetch an individual record by its TweakDBID you do **not** use +0x88. Where the by-id record store lives is not yet confirmed (leading hypothesis: the +0x58 map is the *combined* flat+record id map — see H-009; 193,354 ≈ ~180K flats + ~13K records). 
- **How to re-verify:**
  ```bash
  BIN="$HOME/Library/Application Support/Steam/steamapps/common/Cyberpunk 2077/Cyberpunk2077.app/Contents/MacOS/Cyberpunk2077"
  nm "$BIN" | grep -oE 'N4game4data[0-9]+[A-Za-z0-9_]+_RecordE' | sort -u | wc -l   # 967 record types
  # + P1.12 probe log: 'records.count=843' and 'Items.Preset_Nova_Default ... -> MISS'
  ```
- **Invalidates:** refines F-012 (the +0x88 sub-layout stands; the "by-id record lookup" interpretation is corrected). Does not invalidate F-013/F-015/F-017.

---

### F-022: Live TweakDB map census (P1.12 runtime) — flats=+0x58 (193,354), records-by-type=+0x88 (843), queries=+0x108 (10); H-008 resolved

- **Date:** 2026-05-29
- **Game version:** 2.3.1 (build 5314028); measured at title screen (no save), TweakDB @ runtime 0x3222404a0
- **Evidence (Schema P1.12 probe log, `docs/probes/logs/red4ext-mac-2026-05-29-candidate-probe.log`):**
  - **+0x58 (hashMapA) = 193,354 entries** — the **flats** map. **H-008 RESOLVED: flats = +0x58** (mapA.count=193354 ≫ mapC=10 → `verdict=flats-is-A`). Keyed by **nameHash-direct** = `CRC32(full flat name)` (line 25: `stored=name=0xce8348b9`); compact-key lookup self-test HIT. Entry stride `0x20`; flat **value buffer** at +0x148, base 0x7164e8000, size 4,291,664 bytes.
  - **+0x88 (hashMapB) = 843 entries** — records-BY-TYPE index (F-021). Hash mode fnv1a-8B.
  - **+0x108 (hashMapC) = 10 entries** — queries (mapC{@08=10,@0c=13}).
  - Flat read/write **works**: id `0xce8348b9` (Int32) read=2 → write 3 → verify ok → restored (lines 26–31). The mutation path (P1.10a) is runtime-proven on a real flat.
- **Note:** 193,354 ≈ (~180K flats + ~13K records). Whether +0x58 is flats-only or the *combined* flat+record id map is the open question H-009 — decisive because if records are in +0x58, record-targeting mods work too.
- **How to re-verify:** re-run the P1.12 probe (`TWEAKXL_CANDIDATES_FILE=... DYLD_INSERT_LIBRARIES=libtweakxl.dylib`), read the `[tweakdb] H-008 ...` and `[flat-layout]` lines.
- **Invalidates:** none (resolves H-008)

---

### F-023: macOS TweakDB FlatValue indirection is at entry+0x10 (NOT +0x18 — P1.6 verdict was wrong)

- **Date:** 2026-05-29
- **Game version:** 2.3.1 (build 5314028); measured live via P1.13 walker
- **Evidence (P1.13 walker, deep-walk of all 193,354 entries):**
  - **entry+0x10** dereferences to a heap address whose first 8 bytes are a **real C++ vtable pointer** in the game's __TEXT segment (range `0x107090xxx…0x107110xxx` at the run's slide — slid by ~0x6000000 from the base). Histogram of `*(entry+0x10)`'s deref yields **exactly 843 distinct vtables** — matching the records-by-type count (F-021), i.e. **one C++ class per record type**.
  - **entry+0x18** dereferences to a different heap region whose first 8 bytes are a **tagged 64-bit header** of form `(N<<32)|2` (`0x200000002`, `0x600000002`, `0xa00000002`, …) — NOT a vtable, just an allocator/refcount block. 191,127 of 193,354 (98.8%) entries share the same dominant tag `0x200000002`. That's why the P1.6 layout test was misled: BOTH `+0x10` and `+0x18` looked like plausible pointers (6/6 tally each), and the code picked `+0x18` arbitrarily.
  - P1.13 walker source: `src/red4ext-mac/src/runtime/TweakDB.cpp::DoDumpFlatsSample`, wired in Loader.cpp after `VerifyCandidateFlats`. Env: `TWEAKXL_DUMP_FLATS_MAX=200000`.
- **Implication:** every accessor that reads/writes a flat must follow `entry → +0x10 → FlatValue`. The current P1.6/P1.10 R/W path uses `+0x18` and IS WRONG — the existing "write of `0xce8348b9`" (F-022) succeeded only by coincidence of a 4-byte slot in the +0x18-pointed header being writable; it did not modify a flat value. **All flat R/W code needs a fix-up to use +0x10.**
- **How to re-verify:** `cmake --build build && TWEAKXL_DUMP_FLATS_MAX=200000 DYLD_INSERT_LIBRARIES=…/libtweakxl.dylib "$GAME"` and grep `\[flat-dump\] FREQ-ranked` in `/tmp/red4ext-mac.log`. Every FREQ-ranked top vft is in `0x107xxxxxx` (binary __TEXT range, slide-adjusted).
- **Invalidates:** the `flatValue-ptr-at-entry @ +0x18` selection in `FlatLayout` / P1.6 verdict. Layout was previously declared `confirmed = true`; downgrade.

---

### F-028: End-to-end live mutation of a typed gamedataStat_Record field — framework proven

- **Date:** 2026-05-29
- **Game version:** 2.3.1
- **Evidence (single log line, machine-parseable):**
  ```
  [cinema] 'BaseStats.AccumulatedDoTDecayRate' vft=0x10936a198 p10=0x31ae40d00 +0x54
           before=0.0961745 after=10 wrote=1 verify=OK
  ```
  - The `before` value (0.0961745) is the bit-exact float that F-027 read at the same offset in a separate process — confirming the address was located, not coincidence.
  - `wrote=1` means `mach_vm_write` returned `KERN_SUCCESS` against the live record-class instance memory.
  - `verify=OK` means a subsequent `mach_vm_read_overwrite` of the same 4 bytes returned the freshly-written `10.0` — the change persisted.
- **What this proves end-to-end:** the macOS framework can (1) inject via `DYLD_INSERT_LIBRARIES`, (2) capture the slide (P1.1), (3) read the TweakDB singleton (P1.2), (4) detect DB-ready via apply-trigger polling (P1.3), (5) hash-lookup a named record in the +0x58 map (P1.5/P1.14b), (6) follow the F-023 corrected indirection at entry+0x10 to the typed record, (7) write a per-class field at the F-027-identified offset, and (8) read it back. The full Read→Write→Verify path is live. **Modding works in principle.**
- **What this does NOT yet prove:** that the in-game gameplay system reads `BaseStats.AccumulatedDoTDecayRate` from this memory location (vs an unrelated copy or cached value). A visible in-game test (load save → trigger a poison/burning DoT → time the decay vs vanilla) is needed to confirm the gameplay path. That is the next step.
- **Caveats / Honest gaps:**
  - This is one offset on one record class. Other record types (`Vehicle`, `Item`, `Attack`, `Character`) still need per-class probing for their value slot offsets (F-027 caveat).
  - The full TweakXL `.tweak` mod-application pipeline (parse YAML → resolve flat name → mutate) still needs the per-class value-slot table to be exhaustive. P1.18+ would build that table by probing one representative record per vftable.
- **How to re-verify:** rebuild then
  ```
  DYLD_INSERT_LIBRARIES=…/libtweakxl.dylib \
    TWEAKXL_CINEMA_NAME=BaseStats.AccumulatedDoTDecayRate \
    TWEAKXL_CINEMA_VALUE=10.0 \
    "$GAME_DIR/Cyberpunk2077.app/Contents/MacOS/Cyberpunk2077"
  ```
  Then `grep '\[cinema\]' /tmp/red4ext-mac.log` — line should match the form above with `wrote=1 verify=OK`.
- **Invalidates:** none. Confirms F-023 + F-027 in vivo.

---

### F-029: TweakDB accessors located — GetRecord (+0x58 by-id) and GetFlat (+0x40 sorted flats → +0x148 buffer); confirms macOS layout == Windows (H-010)

- **Date:** 2026-05-30
- **Game version:** 2.3.1; via Ghidra consumer-trace (read-only headless, static base 0x100000000)
- **The two accessors:**
  - **`TweakDB::GetRecord(TweakDBID)` = `FUN_102b745d0`** (1004 callers). Live hash lookup on the **+0x58** map (no caching): `db+0x58`=bucket-index array, `+0x60`=count, `+0x64`=bucketCount, `+0x68`=entries base, `+0x74`=stride; `idx=(id&0xff_ffff_ffff)%bucketCount` → bucket chain → match `entry+0x04==id_lo32 && (entry+0x08&0xff_ffff_ffff)==id40` → returns `*(entry+0x10)`=record instance (refcount++). Caller reads inline fields (Stat_Record value @ p10+0x54, F-027).
  - **`TweakDB::GetFlat(TweakDBID)` = `FUN_102b76708`** (29 callers, all typed `GetFlatValue<T>` wrappers). Masks id to 40 bits, **binary-searches a SORTED key array off `db+0x40`** (helper `FUN_102b139b8`, compares key[0]=uint32 then key[1]=len), returns `bufferBase + packedOffset` — a pointer **into the +0x148 flat-value buffer**.
- **This locates the real `flats` array at +0x40** (a SortedUniqueArray, not a hashmap — why hashmap-centric probes missed it) and **confirms H-010**: macOS TweakDB struct == Windows RED4ext.SDK `TweakDB.hpp`. flats@+0x40, recordsByID@+0x58, recordsByType@+0x88, defaultValues@+0x108, flatDataBuffer@+0x148, unk160@+0x160.
- **Invalidates / corrects:** the map *labels* in F-020/F-022 — **+0x58 is `recordsByID` (not "flats"), +0x108 is `defaultValues` (not "queries")**. The 63,699 record-name hits (F-026) and the read/write proofs remain valid; only the names were wrong. Resolves H-010.
- **How to re-verify:** Ghidra `re-16-getrecord.py` (GetRecord deref + callers) and `re-3-flatread.py` (getter-callers scanning +0x58/+0x148), saved in `/tmp`.

---

### F-030: Gameplay reads a per-entity StatsContainer CACHE, seeded once at entity init — NOT live TweakDB; this is the root cause of F-028's no-op

- **Date:** 2026-05-30
- **Game version:** 2.3.1; via Ghidra consumer-trace
- **Evidence:** scripted stat reads (`StatsDataSystem.GetStatValue/...ByType/Bare/HasStatData` = natives `FUN_103832688/1038327ac/10386c7ec/10386c990`) all funnel to **`FUN_103a7b8e8`** (live stat getter, 23 callers) which reads a **per-entity StatsContainer**: `container+0x50`=sorted stat-type-id array, `+0x5c`=count, `+0x60`=0x10-byte entry array with the value float at **entry+0xc** → binary-search `+0x50`, return `*(entry+0xc)`. It makes **zero** calls to GetFlat/GetRecord/the singleton and never reads `Stat_Record+0x54`. The container is seeded once at entity stat-init/recompute (`FUN_103a99a60/103a9b03c/103a9b73c` gather modifiers via GetRecord; `FUN_103a7d4bc` folds base+modifiers → writes `container+0x60[i]+0xc`).
- **Root cause of the failed cinema poke (F-028 / unleash):** gameplay queries read the cache, while our writes hit the record/source memory AFTER the one-time copy. F-028's `verify=OK` was real (the record byte changed) but inert (the consumer reads the cache).
- **Fix (what/where/when):**
  1. **Preferred (TweakXL-style):** apply at load — after DB-load orchestrator `FUN_102b75744` (F-018) completes, before entity stat-init — so the container seeds from modded values. Write the float at `*(GetRecord(id)+0x10)+0x54` (or, properly, the flat's FlatValue in the +0x148 buffer per F-029/H-010).
  2. **Immediate on a live save:** also patch the per-entity cache at `StatsContainer+0x60[entry]+0xc` (overwritten on next recompute).
  3. Forcing a recompute after a TweakDB write is the general mechanism.
- **How to re-verify:** Ghidra `re-13-base.py` (stats-TU readers of record+0x54) and the `FUN_103a7b8e8` decomp.
- **Invalidates:** none. Explains F-028's caveat ("does NOT prove gameplay reads this location") — it does NOT; gameplay reads the cache.

---

### F-031: TweakDB WRITE-side primitives located — CreateTDBRecord, flats-array layout, FlatValue layout (macOS-specific), buffer fields

- **Date:** 2026-05-30
- **Game version:** 2.3.1; via Ghidra write-side trace (read-only headless, static base 0x100000000), all decompile-confirmed unless noted
- **`CreateTDBRecord` = `FUN_1026b8db8`** — `void(TweakDB* db, uint32 baseMurmur3, TweakDBID id)`. Builds a record from current flats and inserts it via `FUN_102b74408` into recordsByID(+0x58) + recordsByType(+0x88). This is the function `UpdateRecord` calls (Windows AddressHash 0x3201127A equiv). The 4 per-class factory builders are `FUN_1027052ac/102726504/10274f578/102798288`. Load-time record-build loop = `FUN_102b16a48`; base-hash builder (murmur3 seed `0x5eedba5e`) = `FUN_1026b8ce8`.
- **Record object fields:** `recordID` (TweakDBID, 8B) @ **record+0x40**; `GetTweakBaseHash()` = virtual at vtable slot **+0x110** (uint32). (recordID@+0x40 corroborates F-027's storedKey echo.)
- **Flats array @ db+0x40 = `SortedUniqueArray<TweakDBID>`** (decompiled from `FUN_102b139b8` LowerBound + `FUN_102b76708` GetFlat): `entries` @ +0x00, **`capacity` (uint32) @ +0x08, `size` (uint32) @ +0x0C**, `flags` (int32, bit0=NotSorted) @ +0x10. Entry stride 8 (TweakDBID). Binary-search key = `id & 0xFF_FFFF_FFFF` (low 40 bits = {uint32 nameHash; uint8 nameLen}); tdbOffset is the high 24 bits (big-endian, bytes [5][6][7]). Insert = LowerBound→memmove→write, grow via generic realloc `FUN_1000286e8(arr,newCap,8,8,moveFn)`; sort via `FUN_102b467bc`.
- **flatDataBuffer fields** (set by `FUN_102b15ec0`, the flats-section parser, via `PoolGMPL_TDB_Data::AllocateAligned`): `staticFlatDataBuffer` @ db+0x00, `flatDataBuffer` @ db+0x148, `flatDataBufferCapacity` (uint32) @ db+0x150, `flatDataBufferEnd` @ db+0x158. **CRITICAL: allocated exactly to flat-data size — capacity==used, ZERO slack, game never upsizes at runtime.** Appending past `end` is unsafe; to add a FlatValue you must allocate a new larger buffer and repoint +0x00/+0x148/+0x150/+0x158 (+ fix defaultValues offsets), Windows-`SetFlatDataBuffer`-style. Editing an EXISTING flat in place is safe.
- **FlatValue layout (macOS — DIFFERS from Windows):** scalar FlatValue = **0x10 bytes, `[vtable @ +0x00][data @ +0x08]`** (Windows: 0x20, data@+0x10). Confirmed via ctor `FUN_102b1422c`. Per-type vtables (static): **Float `0x106e925f0`, Int32 `0x106e926f8`, Bool `0x106e92d78`**, TweakDBID `0x106e9b448`, Vector3 `0x106e94090`, etc. Scalars of a type share one vtable (so can also clone from an existing same-type FlatValue). Data sizes: Float/Int32/Color=4, Bool=1, TweakDBID/Vector2/LocKey=8, Vector3/EulerAngles=12.
- **`unk160` @ db+0x160:** ⚠️ inferred — no game writer found; treat as the runtime-write gate (Windows `EnsureRuntimeAccess` clears it). Write 0 before runtime flat writes.
- **Recipe (edit existing Float flat + refresh record):** clear `*(u8*)(db+0x160)=0`; resolve flat in +0x40 (binary search) → `fv = flatDataBuffer + tdbOffset`; overwrite `*(float*)(fv+0x08)=v` (in-place; interning caveat); then `UpdateRecord`: `rec=GetRecord(db,recordId)` (`FUN_102b745d0`), `CreateTDBRecord(fakeDB, *(u32*)(rec+baseHashVtbl), *(TweakDBID*)(rec+0x40))`, take rebuilt instance from fake recordsByType, `nativeType->Assign(rec,new)`. Apply at LOAD (after `FUN_102b75744`, before entity stat-init, F-030).
- **How to re-verify:** Ghidra scripts `tools/ghidra/rw-*.py`.
- **Invalidates:** none. Corrects the assumption (from Windows) that scalar FlatValue data is at +0x10 — on macOS it is **+0x08**. Completes F-029 (write side).

---

### F-036: WRITE PATH verified by the GAME's OWN GetFlat — but BaseStats.Health.min/max are NOT the visible HP (correcting earlier over-claims)

- **Date:** 2026-05-31; in-vivo (`docs/probes/logs/red4ext-mac-2026-05-31-verify-game.log`), `VerifyGameSeesEdit`.
- **Ground truth (the game's own accessor, not our inference):** after editing a flat, calling the game's `GetFlat` (`FUN_102b76708`) returns OUR value — for BOTH editors:
  - `BaseStats.Health.max` (in-place): game GetFlat `-1e7 -> 4242` ✓
  - `BaseStats.Health.min` (interning-safe repoint): game GetFlat `0 -> 4242` ✓
  - So `EditScalarFlatInPlace` AND `EditScalarFlatSafe` (alloc+repoint) BOTH genuinely reach the game's flat store. The write side is REAL.
- **CORRECTION of earlier over-claims:** `records-updated=1/1` only means `UpdateRecord` *ran*, NOT that a meaningful value changed. And `BaseStats.Health.max` read **-1e7** before our edit — it is an internal range sentinel, **NOT the player's max HP**. So editing `BaseStats.Health.min/max` changes nothing visible; the "min==max forces the stat" theory was wrong. The only in-game effect ever observed (the RAM/carry/spawn "chaos") was interning COLLATERAL, not targeted flats working.
- **What this means:** the engine (resolve → write → game-visible) is verified. The unsolved part is purely CONTENT: the player's HP/RAM are driven by `gamedataConstantStatModifier` records (base-value modifiers), whose exact names we don't have. Picking flats by guessing property names is the wrong method.
- **Reliable method going forward:** use the verified game-`GetFlat` reader to READ a flat's current value (ground truth), and a canonical TweakDB flat-name dump to know which records/flats exist — find the flat whose live value is the real stat (e.g. base HP, base RAM), then edit THAT. No guessing, no save needed to verify the write.
- **How to re-verify:** `TWEAKXL_VERIFY_GAME=1`; grep `[verify-game] ... GAME-SEES-EDIT=YES`.
- **Invalidates:** the implicit claim (F-034 wiring) that applying a flat mod produces a gameplay effect — it produces a *flat* change the game's GetFlat sees, but whether that drives a visible stat depends on picking the right flat.

---

### F-032: Correct flat path VALIDATED in-vivo — named resolve, real values, scalar edit round-trip (no save)

- **Date:** 2026-05-30; build 2.3.1; title-screen test (`TestFlatWritePath`, evidence `docs/probes/logs/red4ext-mac-2026-05-30-flatwrite.log`)
- **Flats array @ +0x40 holds 3,306,462 flats** (vs 193,354 in +0x58=recordsByID); `flags=0 sorted`; binary-search resolve self-round-trips (HIT).
- **Named resolution works:** `16/20` high-confidence BaseStats flats HIT with sensible values — `BaseStats.AccumulatedDoTDecayRate.max`=999.0 (Float), `BaseStats.Health.enumName` data="Heal…" (CName "Health"). The 4 misses are all `.value` (BaseStats exposes no `.value` flat; min/max/enumName/flags do exist).
- **THE root-cause proof:** the candidate file that scored **0/98 against +0x58** now scores **25/98 against +0x40** — same names, correct map. Confirms every prior "flat" miss was the wrong-map bug (F-029), not absent flats.
- **`EditScalarFlatInPlace` PASS:** Float flat (orig 0.1) → write 1337.0 → read-back 1337.0 → restore 0.1, all verified via `SafeWrite` at `flatDataBuffer+tdbOffset+0x08`. The scalar write primitive works (interning caveat still applies for production).
- **How to re-verify:** launch with the dylib; grep `[flat-name]` / `[flat-edit-test]` (expect `... PASS`).
- **Invalidates:** none. Validates F-029/F-031 end-to-end (read+resolve+write). Remaining: UpdateRecord (propagate to record cache), interning-safe allocation, applicator rewire.

---

### F-033: Runtime baseHash accessor = record vtable +0x118 (GetTweakBaseHash) — unblocks UpdateRecord

- **Date:** 2026-05-30; Ghidra decompile (read-only, static base 0x100000000), cross-validated.
- **GetTweakBaseHash is the virtual at record `vtable + 0x118`** (NOT +0x110 — that slot is a shared empty `{return;}` stub `FUN_102b73b20`). The +0x118 fn returns a 32-bit per-type constant = the value `CreateTDBRecord (FUN_1026b8db8)` switches on. Call form: `baseHash = ((uint32_t(*)(void*))(*(void**)(*(void**)rec + 0x118)))(rec)`.
- **Cross-validation (decompile-confirmed):** for every record type, the +0x118 constant == the factory branch's dispatch CONST, and `baseHash & 0x1f` == the factory case. Airtight.
- **`gamedataStat_Record`:** static vtable **`0x10704f2e8`**, baseHash **`0x1885c3e0`**, factory case 0 (`FUN_1027052ac` → alloc 0x60, ctor `FUN_102b73ac0`, wire `.statType`→rec+0x48, `.value`→rec+0x54 — the +0x54 matching F-027 confirms identity). NOTE: F-026's `0x107d4a198` was a LIVE/slid address; the STATIC Stat vtable is `0x10704f2e8` (same class, different base → at runtime use `StaticToRuntime(0x10704f2e8)`).
- **Last factory (case 0x1f):** `FUN_102afe918`. Partial vtable→baseHash table in `tools/ghidra/bh-*.out`.
- **How to re-verify:** `tools/ghidra/bh-3.py` (full-vtable constant scanner), `bh-4.py` (vtable→baseHash builder).
- **Invalidates:** the +0x110 inference (H-011 had it as a candidate). Resolves H-011's baseHash blocker.

---

### F-034: UpdateRecord COMPLETE — factory rebuild + RTTI Assign (vtable+0x50); wired into the applicator

- **Date:** 2026-05-30; Ghidra decompile + in-vivo (build 2.3.1). Evidence: `docs/probes/logs/red4ext-mac-2026-05-30-updaterec*.log`.
- **Mechanism (both halves validated):** to propagate a flat edit to a live record:
  1. `realRec = recordsByID[recordId]` (+0x58 Lookup → `*(entry+0x10)`).
  2. `baseHash = realRec->vtable[+0x118]()` (F-033).
  3. `newRec = factory[baseHash&0x1f](baseHash, *(u64*)(realRec+0x40))` — the per-class factory (F-031 table) reads the GLOBAL (edited) flats and returns a freshly-materialized record. **In-vivo: no crash; newRec faithfully reproduces realRec (matches all of [0x18,0x60) incl. +0x54) except a +0x28 allocation counter.**
  4. `nativeType = realRec->vtable[+0x08]()` (GetNativeType, slot 1); `nativeType->Assign(realRec, newRec)` where **Assign = type-vtable +0x50** = `FUN_102197fe4` — decompile-confirmed RTTI property-copy loop (iterates `type+0x118` props, count `type+0x124`, copies `src+off→dst+off` per reflected property). **In-vivo: runs without crash.**
- **Important semantics:** Assign copies only **RTTI-reflected properties** (the flat-backed mod targets) — NOT internal/computed fields. (A manual sentinel poke at `+0x54` was NOT copied because +0x54 is an internal field on this record type, not a reflected property; this is correct behavior, not a defect.)
- **Wired into the applicator:** `ApplyMod` now collects each edited flat's owning record (name minus last `.prop`) and calls `red4ext_mac::UpdateRecord(db, recordId)` after the edits. **In-vivo E2E: YAML `BaseStats.AccumulatedDoTDecayRate.max:1234` → `applied=1 ... records-updated=1/1`, no crash.**
- **Caveats / remaining:** temp `newRec` is leaked (one per call — proper release is a follow-up); interning-safe flat allocation still pending (F-031); the final **visible** in-game confirmation needs a save load + a flat that backs a reflected/used property.
- **How to re-verify:** `TWEAKXL_TEST_UPDATEREC=1` (build validation) / `+TWEAKXL_TEST_ASSIGN=1` (Assign), or drop a YAML mod and grep `[applicator] ... records-updated`.
- **Invalidates:** none. Completes H-011 (resolve to fact). Assign confirmed at type-vtable +0x50; GetNativeType at record-vtable +0x08.

---

### F-027: BaseStats / Stat_Record Float value lives at p10+0x54 — first identified cinema mutation target

- **Date:** 2026-05-29
- **Game version:** 2.3.1; measured live via `MapNamesToVftables` + `DumpNamedRecord` probe
- **Evidence:** named-record dump of 3 different `BaseStats.*` records at title screen showed an identical 128-byte layout:
  ```
  p10[+0x00]  void*       vtable           = 0x10b94e198 (gamedataStat_Record)
  p10[+0x08]  void*       self_ptr
  p10[+0x10]  void*       refcount_block
  p10[+0x28]  uint32_t    bufOff (small)
  p10[+0x40]  TweakDBID   storedKey echo   (low32 = recordHash, byte4 = nameLen)
  p10[+0x48]  uint64_t    secondary tag    (looks like CName + 0x08000033-ish type)
  p10[+0x50]  uint32_t    pad / zero
  p10[+0x54]  float       VALUE            ← the scalar payload
  ```
  Per-record `+0x54` floats:
  - `BaseStats.ADSSpeedPercentBonus`:                  `0x43d654b4` = **428.665**
  - `BaseStats.AccumulatedDoTDecayRate`:               `0x3dc4f722` = **0.0961** ✓ plausible decay rate
  - `BaseStats.AccumulatedDoTDecayStartDelay`:         `0xca62dd63` = -3.71e+06 (anomaly — bytes there may not be the value for this particular stat, or a default-not-loaded variant)
- **Cinema-demo path now concrete:**
  1. Pick a Stat with a known plausible default (e.g. `AccumulatedDoTDecayRate` = 0.0961).
  2. Look up its TweakDBID in +0x58.
  3. Read `p10+0x10 → record` and write a new float at `record+0x54` via `mach_vm_write`.
  4. Launch the game and observe DoT-related gameplay (poison/burn duration) — a visible change at 10.0 vs 0.0961 is the cinema.
- **Caveats:**
  - The +0x54 offset is **specific to `gamedataStat_Record`** (vft `0x10b94e198`). Other record classes (`Vehicle_Record`, `Item_Record`, `Attack_Record`) have different layouts; their value offsets need per-class probing. Dump output for `Attack`, `Vehicle`, `Item` samples is captured in `/tmp/red4ext-mac.log`.
  - The `0xca62dd63` outlier for one BaseStats record suggests possibly an alternative encoding for some stat sub-types, OR a field that holds a different kind of value (max instead of base, e.g.). Probe more BaseStats samples before declaring +0x54 universal for the class.
- **How to re-verify:** rebuild, run with `DYLD_INSERT_LIBRARIES=…/libtweakxl.dylib`, grep `\[record-dump\] === 'BaseStats\.`. The 3 sample dumps in `/tmp/red4ext-mac.log` should reproduce.
- **Invalidates:** none. Closes the per-class field-offset half of F-025's gap, for ONE class (`gamedataStat_Record`).

---

### F-026: vftable → record-class identification solved (269 named-record vtables mapped) — closes F-025's "vtable→type" gap

- **Date:** 2026-05-29
- **Game version:** 2.3.1; measured via P1.14b name→vft probe at title screen
- **Evidence:** ran `MapNamesToVftables` against `reference/tweakxl-data/record_names.txt` (63,711 canonical record names extracted from psiberx TweakXL's `InheritanceMap.yaml`). Results:
  - **63,699 of 63,711 (99.98%) resolved** in the live +0x58 map. 12 misses are all stale records: `Items.CapacityBooster_Regina*` (removed in a CP2077 update) and `Character.TygerClawsBikerTattooTest_ma`. Acceptable noise.
  - **269 distinct vtables** in the named-record set (vs 843 total from the full P1.13 deep walk — the 574 unaccounted vtables are auto-generated `_inline` records not present in InheritanceMap).
  - Each vtable cluster's sample record names share a **dominant prefix that identifies the C++ record class**. Top clusters (sample → class name):

  | vtable | freq | class identified by name prefix |
  |--------|------|---------------------------------|
  | `0x107d1a5b8` | 6891 | inline attack/modifier data |
  | `0x107d1e820` | 6668 | `gamedataCharacter_Record` (all "Character.*") |
  | `0x107d8fc98` | 4835 | `gamedataAmmo_Record` (all "Ammo.*") |
  | `0x107d4c1a0` | 3494 | `gamedataAIAction_Record` (Action* / ActionSamples.*) |
  | `0x107d5b418` | 1997 | `gamedataClothing_Record` (Items.* clothing) |
  | `0x107d58fa0` | 1822 | `gamedataItem_Record` (Items.* weapons/cyberware) |
  | `0x107d4a788` | 1499 | `gamedataVendor_Record` (all "Vendors.*") |
  | `0x107d177a8` | 1377 | `gamedataVehicle_Record` (all "Vehicle.*") |
  | `0x107d4a198` | 1269 | `gamedataStat_Record` ⚡ (all "BaseStats.*") |
  | `0x107d31568` | 1081 | `gamedataAIActionTarget_Record` |
  | `0x107d66b88` | 609 | `gamedataAttack_Record` (all "Attacks.*") |

- **Implication:** combined with `reference/tweakxl-data/ExtraFlats.yaml` (60 record-type → property-list mappings authored by psiberx), we now have a **typed flat candidate generator**: pick a named record, look up its vtable, infer the class, and the ExtraFlats entry for that class lists the canonical property names (with flat-types like `Float`, `String`, `Int32`). Cinema target inference becomes deterministic per type rather than blind guessing.
- **Still pending:** the per-class FIELD OFFSET within the C++ record instance — i.e. for `gamedataStat_Record`, at what byte offset within the 0x88-byte record does the `value` Float live? Solving this needs either a per-class probe (dump 128 bytes for a known-value record, locate the float by eye) or Ghidra RTTI walk.
- **How to re-verify:** rebuild + run with `TWEAKXL_VFT_MAP_OUT=/tmp/vft_map.tsv DYLD_INSERT_LIBRARIES=…/libtweakxl.dylib`; grep `\[name-vft\] resolved` (expect "63699/63711") and `\[name-vft\] vft=` for the cluster table. Output TSV is at `/tmp/vft_map.tsv`.
- **Invalidates:** none. Builds on F-023, F-025; closes the type-name half of F-025's gap.

---

### F-025: macOS +0x58 entries point to TYPED RECORD instances, not FlatValue objects — values are inline record-class members

- **Date:** 2026-05-29
- **Game version:** 2.3.1; measured via P1.13 walker 256-byte deep dump of entry[0]
- **Evidence:** dumping 256 bytes at `p10 = *(entry+0x10)` shows the SAME 0x48-byte header (vtable, self, refcnt, …, bufOff, storedKey) **repeating every 0x88-0x90 bytes** through the allocation. The second header at `p10+0x90` has its own valid vtable (`0x10bd99520`, also __TEXT-range), self-ptr (close to original p10), refcount-ptr, distinct bufOff (`0x036305`), and a different storedKey (`0x34000034e4c`). The 64 bytes BETWEEN consecutive headers (`p10+0x48..p10+0x87`) hold mixed-pattern bytes including values that look like CName hashes (`0x50404031`), small ints, and zero-padded fields. **Conclusion: p10 is not a `FlatValue<T>` — it's the start of a slab of TYPED record instances**, where each record's class is identified by its vtable and its property values are stored inline as C++ member fields between header and next allocation.
- **Implication:**
  - **193,354 entries in +0x58 ≈ 193,354 records** (matches CP2077's ~50K records + ~140K flats-as-records-with-one-prop, or each map entry per-record-per-property). Not a flat-only map.
  - **843 unique vftables = 843 typed record subclasses** (`gamedataItem_Record`, `gamedataAttack_Record`, etc. — matching the 843 records-by-type count in +0x88 / F-021).
  - **Value access requires per-class reflection.** Each typed record places its properties (damage, range, multiplier, …) at class-specific offsets. There is no uniform "value slot" — only Ghidra-level RTTI walk OR an in-process RTTI dump can map (vtable → property name → field offset).
  - **F-022's "write of `0xce8348b9` succeeded"** mutated a refcount byte in the +0x18 allocator pool (per F-023) — neither the flat value nor a meaningful record field.
  - **The on-disk flat data buffer at +0x148 (4.1MB) IS real and IS used for some scalar storage** (we successfully decoded `"LocKey#8"` and `"9298"` ASCII from buffer offsets) — but `p10+0x28` is not a clean buffer offset for arbitrary flats; the buffer-scan reverse search found NO vftable cluster with disproportionate float hits (uniform 0.0-0.2% rate across all clusters).
- **Cinema-demo readiness:** **blocked** until one of:
  1. Per-vftable Ghidra RTTI walk identifies a typed record class with a known scalar field (e.g. `gamedataAttack_Record::damage @ +0x4c`) — direct mutation feasible.
  2. WolvenKit canonical TweakDB name dump (P1.14) + per-class probe — named flat lookup gives a known record/property, then probe its layout in-process.
  3. RTTI dump at runtime — enumerate vtable[N] for type-info string per class.
- **How to re-verify:** P1.13 walker with `TWEAKXL_DUMP_FLATS_MAX=200000`. Grep `\[flat-dump\] === deep p10 dump` for the 256-byte hex; look for the second header at `+0x90`.
- **Invalidates:** F-024's `FlatValue object layout` framing — supersede the "this is a FlatValue<T>" interpretation with "this is a Record* into a typed slab." Keep F-024 visible for the layout fields (vtable/self/refcnt/bufOff/storedKey ARE real and are the record's common prefix); revise the interpretation.

---

### F-024: FlatValue object layout (macOS) — 9-word header with vtable, self-ptr, refcount ref, buffer offset, back-key

- **Date:** 2026-05-29
- **Game version:** 2.3.1; measured at title screen via P1.13 walker
- **Evidence:** for every sampled entry's `p10 = *(entry+0x10)`, the 9 8-byte words at `p10+0x00..p10+0x40` consistently decode as:

  ```
  +0x00  void*       vtable             — 843 distinct values, points into __TEXT (binary RTTI)
  +0x08  void*       self_ptr           — equals p10 (TweakRecord base-class "this" back-ref)
  +0x10  void*       refcount_ptr       — points to a 16-byte heap block holding (N<<32)|2 headers
                                          (the same allocator pool as the entry+0x18 indirection)
  +0x18  uint64_t    0                  — zero in every observed entry
  +0x20  uint64_t    0                  — zero in every observed entry
  +0x28  uint32_t    bufferOffset       — byte offset into TweakDB.flatDataBuffer (+0x148, 4.1MB).
                                          Range observed: 0x362f0..~0x5fa6c+ across 193k entries.
                                          ALIASED: distinct entries with the same value share an offset
                                          (interning), so |distinct offsets| ≪ |entries|.
  +0x30  uint64_t    0
  +0x38  uint64_t    0
  +0x40  TweakDBID   storedKey echo     — same 8 bytes as entry+0x08 (back-reference for type-erased ops)
  ```
- **Buffer content confirmed typed-by-vft:** reading `bufferBase + bufferOffset` yields real ASCII for the string-typed vfts (e.g. `0x382379654b636f4c` decodes byte-wise to `"LocKey#8"` — Cyberpunk's localization key format). The buffer holds **typed scalar bytes packed contiguously** (strings null-terminated; Floats/Ints 4-byte aligned).
- **Per-cluster typing still pending:** which vft = `FlatValueFloat`, which = `FlatValueInt32`, etc., is not yet decoded. The float-hit-rate predicate (4-aligned bufOff + value in `[0.01, 1e6]`) was noise-level (~4% across all clusters), indicating either (a) heavy interning of `0.0` floats, or (b) Floats are a small cluster diluted by alignment-quirks. **F-025 / a follow-up RTTI walk needed.**
- **How to re-verify:** P1.13 walker output. Look for `[flat-dump] e[i] hash=… p10=… vft=0x107xxxxxx +8=<self> +10=<refcnt> +28=<bufOff> +40=<storedKey>` lines — the pattern repeats across all samples.
- **Invalidates:** none. Supersedes the prior assumption that `+0x18` carried the FlatValue (corrected in F-023).

---
