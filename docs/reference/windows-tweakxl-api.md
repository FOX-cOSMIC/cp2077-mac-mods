# Windows TweakXL API — Compatibility Reference

**Scope:** The mod file formats and runtime behaviors that our `mod-loader` agent must replicate exactly. If our parser disagrees with the Windows source for any real-world mod, **our parser is wrong**.

**Source of truth:** `reference/windows-tweakxl/` — psiberx's Windows TweakXL repo (Cyberpunk 2077 2.3, commit as archived 2025-11-17). When this doc and the source code disagree, the code wins. All section references cite source file paths relative to `reference/windows-tweakxl/`.

**Maintained by:** `doc-keeper` (Ledger)
**Related:** `docs/reference/tweakdb-binary-format.md`, FA-002, T-002

---

## Compatibility Contract

> **If our parser disagrees with the Windows source for a real-world mod file, our parser is wrong.**

This is the project's #1 compatibility constraint (see AGENTS.md "Mission"). Modders should not have to change `.yaml`, `.tweak`, or any other mod file to use their mods on macOS. We do not add new syntax or new features; we implement the spec that already exists.

---

## 1. Mod File Discovery

**Source:** `src/App/Tweaks/Declarative/TweakImporter.cpp` — `TweakImporter::ImportTweaks()`

### Scan roots

The importer is invoked with a list of root paths. In the game context, this is:
```
<game>/r6/tweaks/
```

All scan paths are traversed **recursively** with `std::filesystem::recursive_directory_iterator` and `follow_directory_symlink` enabled. Subdirectory depth is unlimited; directory structure is informational only.

### Recognized file extensions

Only regular files with these extensions are processed:

| Extension | Parser |
|-----------|--------|
| `.yaml` | `YamlReader` |
| `.yml` | `YamlReader` |
| `.tweak` | `RedReader` |

All other files are silently skipped (no error, no warning).

### Load order (priority tiers)

Files are sorted into three priority groups based on the **first character of the filename** (not the path):

| Tier | First-character triggers | Load order |
|------|--------------------------|------------|
| First | `_`, `#`, `$`, `!` | Loaded before all others |
| Last | `^` | Loaded after all others |
| Normal | anything else | Loaded between first and last |

Within each tier, the order is the filesystem iteration order (platform-dependent; effectively directory-walk order). Mods that need to run before other mods should prefix their filename with `_` or `#`.

**Source:** `TweakImporter::IsFirstPriority()` and `IsLastPriority()`:
```cpp
const std::string s_firstPriorityMarkers = "_#$!";
const std::string s_lastPriorityMarkers  = "^";
```

### Error handling during scanning

A file that fails to load (`reader->Load()` returns false) or throws an exception is logged as an error and **skipped** — the importer continues processing remaining files. An exception during the `Apply()` phase (committing to TweakDB) is also caught and logged; it does not abort subsequent files.

---

## 2. YAML Format

**Source:** `src/App/Tweaks/Declarative/Yaml/YamlReader.cpp`, `YamlReader.hpp`, `YamlReader.Values.cpp`, `YamlReader.Template.cpp`, `YamlReader.Legacy.cpp`

### 2.1 Top-level structure

A YAML mod file must be a **mapping** (YAML Map node) at the top level. Any other structure (scalar, sequence, null) is rejected with: `"Bad format. Unexpected data at the top level."` The top-level keys are TweakDB IDs (flat names or record names). Top-level keys beginning with `$` are **attribute keys** and are skipped during iteration — they configure the file's behavior, not individual records.

```yaml
# Minimal example — assign a flat value
Player.maxHealth: 300

# Full-qualified form with type override
NPCs.gangDensityFactor:
  $type: Float
  $value: 2.5

# Record creation via clone
Items.MyCustomGun:
  $base: Items.Preset_Ajax_Default
  damageMin: 80
  damageMax: 120
```

### 2.2 `$`-prefixed attribute keys (complete list)

All keys whose first character is `$` (`AttrSymbol = '$'`) are interpreted as attributes, not as TweakDB paths. The following are all attribute keys recognized in the source:

| Key | Scope | Source | Behavior |
|-----|-------|--------|----------|
| `$type` | top-level entry, record property | `YamlReader.cpp` | Declares the TweakDB type name for the entry (flat type or record class name) |
| `$value` | top-level entry | `YamlReader.cpp` | Explicit value node; used alongside `$type` for flat assignments with type override |
| `$base` | top-level entry, inline record | `YamlReader.cpp` | Clone source — creates a new record by copying an existing one. Value is a TweakDB path string. |
| `$props` | top-level entry, record block | `YamlReader.cpp` | Property mode override. Only recognized value: `"AutoFlats"`. In Auto mode, unknown record properties are treated as free flats (not an error). Default: Strict (unknown properties are errors). |
| `$game` | top-level file | `YamlReader.cpp` | Game version condition (semver range string, e.g. `">=2.1.0"`). If the condition is not satisfied, the entire file is skipped. |
| `$dlc` | top-level file | `YamlReader.cpp` | DLC condition. Only recognized value: `"EP1"` (Phantom Liberty). If the DLC is not installed, the entire file is skipped. |
| `$instances` | array item (templates) | `YamlReader.Template.cpp` | Template instantiation list. See §2.5. |

All other `$`-prefixed keys are silently skipped (no error).

**Important:** `$game` and `$dlc` are top-level file conditions — they appear as top-level keys in the YAML map and gate the entire file. They are not per-entry conditions.

### 2.3 Flat assignment

A top-level scalar or sequence assigns a flat value directly:

```yaml
# Scalar — inferred or type-known
Player.maxHealth: 300
Vehicle.nitro.boostDuration: 5.0

# Sequence — assigns an array flat
Items.Preset_Ajax_Default.tags:
  - Tier4
  - Iconic
```

A top-level mapping with neither `$base` nor `$type` (and no record instance in TweakDB for that key) attempts flat inference from the node content.

### 2.4 Record block

A mapping value for an existing record ID (or a new record with `$base` or `$type`) is treated as a record block. Keys inside the block are property names (short names, without the record prefix):

```yaml
Items.MyWeapon:
  $base: Items.Preset_Ajax_Default    # clone from this record
  $type: gamedataWeapon_Record        # optional if $base is given
  $props: AutoFlats                   # optional: treat unknown props as free flats
  damageMin: 80
  damageMax: 120
  buyPrice:                           # inline record
    $type: gamedataBuyPrice_Record
    basePrice: 1500
```

### 2.5 Array mutation tags (YAML tags syntax)

Instead of replacing an array flat entirely, individual elements can be appended, prepended, or removed using **YAML tags** on sequence items. Tags are the `!tagname` prefix on YAML sequence elements.

All tag operations are triggered inside a sequence value on a flat that is an array type:

```yaml
Items.Preset_Ajax_Default.tags:
  - !append Tier5
  - !remove Tier4
  - !append-once Legendary    # append only if not already present
  - !prepend Priority
```

**Complete list of recognized mutation tags** (source: `YamlReader.cpp`, constants computed as `FNV1a64` hashes):

| Tag | Operation | Notes |
|-----|-----------|-------|
| `!append` | `AppendElement` | Append one element (may duplicate) |
| `!append-once` | `AppendElement(unique=true)` | Append only if not already in array |
| `!append-from` | `AppendFrom(sourceId)` | Append all elements from another flat (value is a TweakDBID path) |
| `!prepend` | `PrependElement` | Prepend one element (may duplicate) |
| `!prepend-once` | `PrependElement(unique=true)` | Prepend only if not already in array |
| `!prepend-from` | `PrependFrom(sourceId)` | Prepend all elements from another flat |
| `!merge` | `AppendFrom(sourceId)` | Alias for `!append-from` (same behavior) |
| `!remove` | `RemoveElement` | Remove matching element(s) from array |
| `!remove-all` | `RemoveAllElements` | Clear the entire array |

**Mixed mutations and assignments:** If a sequence mixes tagged (mutation) and untagged (assignment) items, the untagged items are silently ignored and only mutations take effect. A warning is logged: `"Mixed definition of array replacement and mutations. Only mutations will take effect."` This behavior must be matched.

**Unrecognized tags** produce an error log: `"Invalid action <tag>."` The element is skipped; other elements in the same sequence continue to be processed.

### 2.6 Template system (`$instances`)

A top-level entry (or an array item) can carry a `$instances` key containing a list of variable mappings. The importer expands the entry once per instance, substituting `$(var)` or `${var}` placeholders in string values and keys.

```yaml
# Top-level template: generates Items.Gun_A, Items.Gun_B
Items.$(name):
  $instances:
    - name: Gun_A
      dmg: 50
    - name: Gun_B
      dmg: 80
  $base: Items.Preset_Ajax_Default
  damageMin: $(dmg)
```

Variable substitution uses `$()` or `${}` delimiters. Unknown variables (not in the instance data) pass through as-is (no error). The `$instances` key is removed before the template is applied.

**Source:** `YamlReader.Template.cpp` — `ProcessTemplates()` and `FormatString()`.

### 2.7 Legacy format (deprecated, but still parsed)

An older top-level format using `groups` and `flats` map keys (not `$`-prefixed) is still supported via `ConvertLegacyNodes()` in `YamlReader.Legacy.cpp`. This is not used by current mods; document for completeness:

```yaml
# Legacy format (do not generate new mods in this style)
groups:
  SomeRecord:
    type: gamedataSomeType_Record
    members:
      propA:
        type: Int32
        value: 42
flats:
  Some.Flat:
    type: Float
    value: 1.5
```

The importer converts these to the modern format before processing. We must handle legacy files for maximum compatibility.

---

## 3. `.tweak` Format

**Source:** `src/Red/TweakDB/Source/Grammar.hpp`, `src/Red/TweakDB/Source/Parser.cpp`, `src/Red/TweakDB/Source/Source.hpp`

### 3.1 Overview

`.tweak` files use a custom textual format parsed with PEGTL (Parsing Expression Grammar Template Library). The grammar is defined in `Grammar.hpp` as a set of PEGTL rule types. Our parser must produce an equivalent parse tree.

### 3.2 Top-level structure

A `.tweak` file has two structural forms:

**With package declaration:**
```
package <PackageName>
[using <Package1>, <Package2>, ...]

<group-stmt | flat-stmt>*
```

**Without package declaration:**
```
<group-stmt>*
```

Package names can be identifiers (alphanumeric + underscore) or hex literals (`0x` + 9–10 hex digits).

Special reserved packages:
- `RTDB` — schema package. Editing schema packages produces an error: `"Schema package editing is not supported."`
- `Query` — query package. Produces: `"Query package editing is not supported."`

### 3.3 Comments

```
// single-line comment (to end of line)
/* block comment (may span lines) */
```

### 3.4 Group statement (record definition)

```
[tags] <GroupName> [: <BaseGroup>] {
    <flat-stmt>*
}
```

- `GroupName`: identifier or dotted path (e.g. `MyGun`, `Items.MyGun`)
- `BaseGroup`: optional inheritance — a group path (`Package.Group`) or group id. Used to clone/extend a record.
- Tags: zero or more `[TagName]` before the group name. Currently only `[EP1]` is recognized (gates on Phantom Liberty DLC).

Example:
```tweak
package MyMod

[EP1] Items.MyGun : Items.Preset_Ajax_Default {
    damageMin = 80;
    damageMax = 120;
}
```

### 3.5 Flat statement

Inside a group or at package level (when a package is declared):

```
[tags] [<TypeExpr> ] <FlatName> <Op> <Value> ;
```

- `TypeExpr`: optional type annotation. Required when declaring a new flat. Can be omitted for flats inherited from a base record.
- `FlatName`: unqualified name within the group (the full TweakDBID will be `Package.GroupName.FlatName`)
- `Op`: `=` (assign), `+=` (append), `-=` (remove)
- `Value`: scalar, struct literal, array literal, or inline record

### 3.6 Type expressions

| Grammar token | Type name | Notes |
|---|---|---|
| `int` | `Int32` | |
| `float` | `Float` | |
| `bool` | `Bool` | |
| `string` | `String` | |
| `CName` | `CName` | |
| `ResRef` | `ResRef` | |
| `LocKey` | `LocKey` | |
| `Quaternion` | `Quaternion` | |
| `EulerAngles` | `EulerAngles` | |
| `Vector3` | `Vector3` | |
| `Vector2` | `Vector2` | |
| `Color` | `Color` | |
| `fk<TypeName>` | foreign key | `TypeName` is a record class name |
| `<TypeExpr>[]` | array | Appends `[]` to any base type: e.g. `int[]`, `fk<Weapon_Record>[]` |

### 3.7 Value literals

**Scalar:**
```tweak
bool_val   = true;          // or false
int_val    = 42;
neg_val    = -7;
float_val  = 3.14f;         // 'f' suffix is optional but conventional
string_val = "some text";   // double-quoted; no escape sequences documented
```

**Struct (positional, comma-separated floats in parentheses):**
```tweak
vec3_flat   = (1.0, 0.0, -1.5);           // Vector3
vec2_flat   = (0.5, 0.5);                 // Vector2
quat_flat   = (0.0, 0.0, 0.0, 1.0);      // Quaternion (i, j, k, r)
euler_flat  = (0.0, 90.0, 0.0);          // EulerAngles (Roll, Pitch, Yaw)
color_flat  = (255, 128, 0, 255);         // Color (RGBA, via integers)
```

Up to 4 elements are parsed (`rep_max<2, ...>` beyond the first two, so 2+2=4 maximum). The grammar requires at least 2 elements.

**Array:**
```tweak
tags       = ["Tier4", "Iconic"];
some_ints  = [1, 2, 3];
records    = [OtherRecord.Name, (0.0, 1.0, 0.0)];
```

Arrays can contain scalars, struct literals, or inline records.

**Inline record:**
```tweak
icon = {
    atlasResourcePath = "some\\path.xbm";
} : UIIcon.IconName;           // optional base after closing brace
```

### 3.8 Operations

| Grammar token | `ETweakFlatOp` | Description |
|---|---|---|
| `=` | `Assign` | Replace flat value entirely |
| `+=` | `Append` | Append element(s) to array |
| `-=` | `Remove` | Remove matching element(s) from array |

Compound operations (`+=`, `-=`) are only valid on **array flats**. Applying them to a scalar flat produces: `"Compound operations are only supported for array types."` The flat is skipped.

---

## 4. Operations Table

| Operation | YAML syntax | `.tweak` syntax | Behavior | Notes |
|---|---|---|---|---|
| **Assign flat** | `Flat.Path: value` | `FlatName = value;` | Replace the flat value | Works for scalars and arrays |
| **Append to array** | `!append item` in sequence | `FlatName += [item];` | Add item at end of array | May create duplicates |
| **Append-once** | `!append-once item` | *(not in .tweak grammar)* | Add item only if not already present | YAML only |
| **Append from** | `!append-from OtherFlat` | *(not in .tweak grammar)* | Append all elements from another flat | YAML only |
| **Prepend** | `!prepend item` | *(not in .tweak grammar)* | Add item at start of array | YAML only |
| **Prepend-once** | `!prepend-once item` | *(not in .tweak grammar)* | Prepend only if not present | YAML only |
| **Prepend from** | `!prepend-from OtherFlat` | *(not in .tweak grammar)* | Prepend all from another flat | YAML only |
| **Merge (append-from alias)** | `!merge OtherFlat` | *(not in .tweak grammar)* | Same as `!append-from` | YAML only |
| **Remove from array** | `!remove item` | `FlatName -= [item];` | Remove matching element(s) | |
| **Remove all** | `!remove-all` (bare tag) | *(not in .tweak grammar)* | Clear entire array | YAML only |
| **Clone record** | `Record: { $base: Source }` | `RecordName : SourceRecord { }` | Create record as copy of source | Must be same type or compatible subtype |
| **Create record** | `Record: { $type: TypeName }` | `TypeName RecordName { }` | Create record with default values from type | |

---

## 5. TweakDBID Encoding

**Source:** Inferred from `YamlReader.Values.cpp` (CRC32 hash construction) and the broader project context.

```
nameHash   = CRC32(full_name, polynomial=0xEDB88320)   // ISO 3309 / zlib
nameLength = strlen(full_name)
```

**Polynomial:** ISO 3309 reflected polynomial `0xEDB88320` — the same as zlib's `crc32()`. This is sometimes called "CRC32b" to distinguish from the unreflected variant. The seed value is `0xFFFFFFFF` and the output is XOR'd with `0xFFFFFFFF` (standard zlib convention).

**Full name convention:** Always the full dotted path, e.g., `"Items.Preset_Ajax_Default.buyPrice"`. Not relative names. The separator is `"."` (one period).

**Compact ID** (used for runtime lookups): `{nameHash, nameLength, tdbOffset={0,0,0}}`. The `tdbOffset` is zero for IDs computed from a name string; it is filled in from the binary file when loading from disk.

**Debug format:** `<TDBID:HHHHHHHH:LL>` — 8-digit hex hash, 2-digit hex length. Produced by the game's debug formatting; parseable back to TweakDBID (source: `YamlReader.Values.cpp`, `ConvertValue<TweakDBID>` debug-format branch).

**Quoted format (YAML):** `t"Package.Item"` — the `t"..."` syntax forces TweakDBID parsing. Wrapped format: `TweakDBID("Package.Item")`.

**None/empty:** The string `"None"` produces a zeroed TweakDBID (source: `YamlReader.Values.cpp`).

---

## 6. Type Coercion Table

**Source:** `YamlReader.Values.cpp` — `MakeValue()`, `TryMakeValue()`, `ConvertValue<T>()` specializations.

### 6.1 Scalar type inference (when no `$type` is given)

When the flat type is not known from TweakDB reflection, `TryMakeValue()` attempts strict conversions in order:

| Probe order | Condition | Inferred type |
|-------------|-----------|---------------|
| 1 | YAML-quoted string (tag `"!"`) | `String` |
| 2 | Integer-convertible scalar | `Int32` |
| 3 | Float-convertible scalar | `Float` |
| 4 | `true`/`false` | `Bool` |
| 5 | `n"..."` or `CName("...")` prefix | `CName` |
| 6 | `t"..."` or `TweakDBID("...")` or `<TDBID:...>` | `TweakDBID` |
| 7 | `l"..."` or `LocKey(...)` or `LocKey#...` prefix | `LocKey` |
| 8 | `r"..."` or `ResRef(...)` prefix | `ResRef` |
| Map with `{i,j,k,r}` | — | `Quaternion` |
| Map with `{roll,pitch,yaw}` | — | `EulerAngles` |
| Map with `{x,y,z}` | — | `Vector3` |
| Map with `{x,y}` | — | `Vector2` |
| Map with `{red,green,blue,alpha}` | — | `Color` |
| Sequence of above | — | `array:T` (type from first element) |

If inference fails for all types, the flat is logged as: `"Ambiguous definition. The value type cannot be determined."` and skipped.

### 6.2 Explicit coercion (when target type is known)

When TweakDB reflection provides the target type (non-strict mode, `aStrict=false`):

| YAML source value | Target type | Behavior |
|---|---|---|
| Any integer scalar | `Int32` | Direct conversion via `YAML::convert<int>` |
| Any integer scalar | `Float` | Promoted (integer ints become float) |
| Any float scalar | `Int32` | **Rejected** — float-to-int is not automatic |
| Any float scalar | `Float` | Direct conversion |
| `true` / `false` | `Bool` | Direct |
| Quoted YAML string (`"!"` tag) | `String` | Always accepted in strict mode |
| Any scalar (non-strict) | `String` | Accepted; if it looks like a `LocKey`, format as `LocKey#<key>` string instead |
| Any scalar | `CName` | Formats via `n"..."` if present, else `CName("...")`, else plain string → `CNamePool::Add` |
| `t"..."`, `TweakDBID("...")`, `<TDBID:...>` | `TweakDBID` | Parsed; plain path string also accepted in non-strict |
| `l"..."`, `LocKey(...)`, `LocKey#<key>` | `LocKey` | Parsed; plain integer → numeric LocKey; plain string → string LocKey |
| `r"..."`, `ResRef(...)` | `ResRef` | Parsed; plain integer or string path also accepted |
| YAML map `{i, j, k, r}` | `Quaternion` | Components default to 0.0 if absent (non-strict) |
| YAML map `{roll, pitch, yaw}` | `EulerAngles` | Components default to 0.0 if absent |
| YAML map `{x, y, z}` | `Vector3` | Components default to 0.0 if absent |
| YAML map `{x, y}` | `Vector2` | Components default to 0.0 if absent |
| YAML map `{red, green, blue, alpha}` | `Color` | Components are `uint8_t`, default to 0 if absent |
| Sequence | `array:T` | Each element coerced to element type; first incompatible element **aborts** the entire array (returns `nullptr`) |

**Type mismatch:** When a known type and a given value are incompatible (e.g., a float value for an `Int32` flat), the flat is logged as: `"Invalid value. Expected <TypeName>."` and skipped.

### 6.3 CName encoding note

`CName` values are stored as `uint64_t` FNV1a64 hashes of the string in the CName pool. The coercion creates a pool entry via `CNamePool::Add()`. We do not need to reproduce FNV1a64 at the parser level — the runtime provides the pool. However, for a macOS port without the Windows runtime, we need to implement `CNamePool::Add()` with the correct FNV1a64 variant. The Windows source uses `Red::FNV1a64` throughout (e.g., in `YamlReader.cpp` for operation tag matching).

---

## 7. Error Handling

**Source:** `YamlReader.cpp`, `RedReader.cpp`, `TweakImporter.cpp`

### 7.1 General principle

Errors are **logged and skipped** — they do not abort the entire import. The importer processes as many valid entries as possible from each file, and as many valid files as possible in a session.

### 7.2 Error catalog (we must produce equivalent behavior)

| Condition | Source | Message / Behavior |
|---|---|---|
| Top level is not a map | `YamlReader::Read()` | `"Bad format. Unexpected data at the top level."` — entire file skipped |
| `$game` condition false | `CheckConditions()` | File silently skipped (no message) |
| `$dlc` condition false | `CheckConditions()` | File silently skipped (no message) |
| Top-level entry has empty name | `HandleTopNode()` | `"Bad format. The top element must have a name."` |
| Unknown `$base` record | `HandleTopNode()` | `"Cannot clone <src>, the record doesn't exist."` |
| Incompatible type for clone | | `"Cannot clone <src>, the record has incompatible type."` |
| Invalid `$type` for flat | | `"Invalid value type <typename>."` — flat skipped |
| Invalid `$type` for record | | `"Invalid record type <typename>."` — record skipped |
| Ambiguous flat (no type, no inference) | `HandleFlatNode()` | `"Ambiguous definition. The value type cannot be determined."` |
| Invalid flat value | | `"Invalid value. Expected <typename>."` |
| Mixed array mutations+assignments | `HandleMutations()` | `"Mixed definition of array replacement and mutations. Only mutations will take effect."` (warning) |
| Unknown array mutation tag | | `"Invalid action <tag>."` — element skipped |
| `+=`/`-=` on non-array flat | `RedReader::HandleFlat()` | `"Compound operations are only supported for array types."` — flat skipped |
| Unknown group base in `.tweak` | `RedReader::HandleGroup()` | `"Unknown base group <name>."` — group skipped |
| Incompatible record type in `.tweak` | | `"Record type <A> doesn't match previous definition <B>."` or `"...is not compatible with..."` |
| Abstract record type used | | `"Cannot create record, the record type <T> is abstract."` |
| Schema/query package in `.tweak` | `RedReader::Read()` | `"Schema package editing is not supported."` / `"Query package editing is not supported."` |
| Exception during file read | `TweakImporter::Read()` | Error logged; file skipped; other files continue |
| Exception during apply | `TweakImporter::ImportTweaks()` | Error logged; import may be incomplete |

### 7.3 Unknown record properties

In the default **Strict** mode (`PropertyMode::Strict`), a property name inside a record block that is not in the record's RTTI type info produces: `"Unknown property <name>."` and the property is skipped.

In **Auto** mode (set via `$props: AutoFlats`), unknown properties are treated as free flats (handled via `HandleFlatNode`) rather than as errors.

---

## 8. Compatibility Contract (Summary)

1. **Same file → same result.** Given the same mod file and the same TweakDB state, our loader must produce the same TweakDB mutations as the Windows loader.

2. **Error messages need not match exactly**, but the behavior must: a file that Windows rejects must be rejected on macOS; an entry that Windows skips must be skipped on macOS; an entry that Windows applies must be applied on macOS.

3. **Unknown/future YAML keys beginning with `$` are silently skipped.** This is the Windows behavior. Do not error on unknown attributes.

4. **File extensions are case-sensitive on macOS** (case-insensitive filesystem aside). Implement extension matching case-insensitively to match Windows behavior: `.YAML`, `.Yaml` etc. should all be recognized. (*Note: Windows TweakXL source uses `ext == L".yaml"` — case-sensitive. However, since Windows FS is case-insensitive, mods may ship with any case. We should normalize extension to lowercase before comparing.*)

5. **The source of truth is always the Windows source code.** This document is a distillation; when there is ambiguity, read `reference/windows-tweakxl/` directly.

---

## 9. Cross-References

| Reference | Relevance |
|---|---|
| `docs/reference/tweakdb-binary-format.md` | TweakDBID encoding, flat type system, on-disk format |
| FA-002 | Why offline disk-patching was abandoned — runtime TweakDB modification is required |
| `reference/windows-tweakxl/src/App/Tweaks/Declarative/Yaml/YamlReader.cpp` | YAML parser implementation |
| `reference/windows-tweakxl/src/Red/TweakDB/Source/Grammar.hpp` | `.tweak` PEGTL grammar |
| `reference/windows-tweakxl/src/Red/TweakDB/Source/Parser.cpp` | `.tweak` parser actions |
| `reference/windows-tweakxl/src/App/Tweaks/Declarative/TweakImporter.cpp` | File discovery, load order |
| AGENTS.md Mission | Compatibility is the #1 success criterion |
