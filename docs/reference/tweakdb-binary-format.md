# TweakDB Binary Format (`tweakdb.bin`) — Canonical Reference

**Scope:** On-disk format of `tweakdb.bin` (the game's flat-value database). In-memory representation is a separate concern (see §9 — macOS divergences are significant and under active research).

**Maintained by:** `doc-keeper` (Ledger)
**Canonical source:** WolvenKit C# parser — [github.com/WolvenKit/WolvenKit](https://github.com/WolvenKit/WolvenKit) (`WolvenKit.RED4.Types/TweakDB/`). When this doc and WolvenKit disagree, WolvenKit wins.
**Game path:** `<game>/r6/cache/tweakdb.bin`
**Related:** H-001, H-002, T-002, T-003, FA-002, FA-003

---

## 1. Magic Number and Byte Order

```
Magic bytes (little-endian): 47 DB B1 0B
As uint32_t LE:              0x0BB1DB47
```

All multi-byte fields in the file are **little-endian**. The magic constant `0x0BB1DB47` can be remembered as "DB47" → "TweakDB". When reading on any platform, read as a LE `uint32` and compare to `0x0BB1DB47`.

---

## 2. Header (32 bytes)

The file begins with a 32-byte header. All offsets are from the start of the file.

| Offset | Size | Type | Field | Notes |
|--------|------|------|-------|-------|
| 0x00 | 4 | u32 | `magic` | Must equal `0x0BB1DB47` |
| 0x04 | 4 | u32 | `blob_version` | File format version. Known good: 5 (game v2.x). |
| 0x08 | 4 | u32 | `parser_version` | Parser/generator version. Informational. |
| 0x0C | 4 | u32 | `records_offset` | Byte offset to the Records section |
| 0x10 | 4 | u32 | `flats_offset` | Byte offset to the Flats section |
| 0x14 | 4 | u32 | `queries_offset` | Byte offset to the Queries section |
| 0x18 | 4 | u32 | `groups_offset` | Byte offset to the Group Tags section |
| 0x1C | 4 | u32 | *(unknown)* | Possibly a CRC32 or entry count. **TBD — verify against WolvenKit source.** |

**Version note:** If `blob_version` does not match what the game binary expects, the game will refuse to load the file or produce garbage. Never write a modified `tweakdb.bin` back to disk with a mismatched version — this is why offline patching was abandoned (see FA-002).

---

## 3. Overall Layout

```
┌─────────────────────┐ 0x00
│   Header (32 bytes) │
├─────────────────────┤ header.records_offset
│   Records Section   │
├─────────────────────┤ header.flats_offset
│   Flats Section     │
├─────────────────────┤ header.queries_offset
│   Queries Section   │
├─────────────────────┤ header.groups_offset
│   Group Tags Section│
├─────────────────────┤ (inferred end of structured data)
│   String Pool       │ (exact offset TBD — embedded or appended)
└─────────────────────┘
```

The sections do not overlap. The order above is typical but the header offsets are authoritative — do not assume a fixed order.

---

## 4. Records Section

Each record entry represents a game data record (e.g., an item, NPC, weapon). Records are identified by their `TweakDBID` (see §6).

### Section header

```
u32  count     // number of record entries
```

### Record entry (12 bytes)

| Offset | Size | Type | Field | Notes |
|--------|------|------|-------|-------|
| 0x00 | 8 | TweakDBID | `id` | Packed 64-bit identifier (see §6) |
| 0x08 | 4 | u32 | `type_hash` | CRC32 (or CName hash) of the record's type name |

The full record body (its property flats) is found in the Flats section, keyed by `id` + property name suffix.

---

## 5. Flats Section

Flats are scalar and struct values. Each flat is a key-value pair: a `TweakDBID` (the flat's full dotted path name, e.g. `Items.Preset_Ajax_Default.buyPrice`) and a typed value.

### Section header

```
u32  count     // number of flat key entries
```

### Flat key entry (8 bytes)

| Offset | Size | Type | Field | Notes |
|--------|------|------|-------|-------|
| 0x00 | 4 | u32 | `name_hash` | CRC32 of the flat's full name string |
| 0x04 | 1 | u8  | `name_length` | `strlen(name)` — used with hash for disambiguation |
| 0x05 | 3 | u24 | `value_offset` | 24-bit little-endian offset into the flat data buffer |

**On Windows:** `value_offset` is a byte offset into a contiguous `flatDataBuffer` array, typed by the flat's type. Accessing a flat means: `flatDataBuffer + value_offset`, cast to the flat's C++ type.

**On macOS:** The meaning of `value_offset` at runtime is **unknown/hypothesized**. H-001 proposes that what Windows calls `flatDataBuffer` is actually a function-pointer dispatch table on macOS, making `value_offset` an index into that table rather than a byte offset. See §9.

Flat keys are stored sorted by `name_hash` for binary-search lookup.

### Value storage

Values are stored in type-separated arrays (one array per type), not interleaved. The `value_offset` points within the array for that flat's type. The Flats section contains a type directory mapping each of the 26 type slots (13 scalar + 13 array, see §8) to its value array.

---

## 6. TweakDBID Encoding

`TweakDBID` is the primary key type. It identifies a record or flat by a hash of its full dotted path name. The packed representation is **8 bytes**:

```c
struct TweakDBID {
    uint32_t nameHash;      // CRC32(full_name, ISO_3309_poly)
    uint8_t  nameLength;    // strlen(full_name), capped at 255
    uint8_t  tdbOffset[3];  // Windows: 24-bit LE offset into flatDataBuffer
                            // macOS:   meaning TBD (see H-001)
};
// Total: 8 bytes, little-endian
```

**Hash algorithm:** Standard ISO 3309 / zlib CRC32 polynomial (`0xEDB88320` reflected). The same polynomial used by `zlib`'s `crc32()`. Case-sensitive: `"Player.maxHealth"` and `"player.maxhealth"` produce different IDs.

**Name length field:** Allows the runtime to distinguish two different names that happen to share the same CRC32 (birthday collision). In practice, 8-byte TweakDBIDs with only the hash and length are "compact" IDs sufficient for most lookups.

**Computing a TweakDBID from a name string:**
```c
uint32_t crc   = crc32(0, (uint8_t*)name, strlen(name));  // zlib crc32
uint8_t  len   = (uint8_t)strlen(name);
TweakDBID id   = { crc, len, {0,0,0} };  // tdbOffset zeroed for compact form
```

**Debug string format** (appears in game logs): `<TDBID:HHHHHHHH:LL>` where `HHHHHHHH` is the 8-digit hex hash and `LL` is the 2-digit hex length. The YAML reader in Windows TweakXL parses this format back to a TweakDBID (see `YamlReader.Values.cpp`).

---

## 7. Queries Section

Queries describe named sets of record IDs (e.g., "all NPC records of type gamedataHuman_Record"). Used by the game's query system for batch lookups.

### Section header

```
u32  count     // number of query entries
```

### Query entry

Each entry contains:
- A `TweakDBID` for the query name
- A list of `TweakDBID`s matching the query criteria

Full layout: **TBD — verify against WolvenKit.** The section is read-only for our purposes; we do not need to write queries in v1.0.

---

## 8. Group Tags Section

Group tags associate records with named groups (tags) for organizational and gameplay logic purposes.

### Section header

```
u32  count     // number of group tag entries
```

Each entry maps a `TweakDBID` (record) to a `TweakDBID` (tag name). Layout: **TBD — verify against WolvenKit.** Not required for v1.0.

---

## 8.1 String Pool / Name Table

Flat and record names as strings are stored in a pooled/interned form. The exact layout (whether a length-prefixed array, a null-terminated list, or an offset table) is:

**TBD — verify against WolvenKit source (`TweakDBStringPool` or equivalent class).**

The string pool is used for:
- Debug representations (`nameLength` field can look up the original string)
- Any runtime feature that needs the human-readable name of a flat

For our parser implementation, we only need the hash + length for TweakDB lookups; the string pool is optional unless we need full name reconstruction.

---

## 9. Flat Type Registry (26 Types)

The game's type system defines 13 scalar types and their array variants. All 26 are valid flat types in `tweakdb.bin`.

| # | Scalar type | Array type | C++ / RED4ext type | Notes |
|---|-------------|------------|--------------------|-------|
| 1 | `Int32` | `array:Int32` | `int32_t` | Signed 32-bit integer |
| 2 | `Float` | `array:Float` | `float` | IEEE 754 single-precision |
| 3 | `Bool` | `array:Bool` | `bool` (1 byte) | |
| 4 | `String` | `array:String` | `Red::CString` | Length-prefixed UTF-8 |
| 5 | `CName` | `array:CName` | `Red::CName` | FNV1a64 hash of a string (8 bytes) |
| 6 | `TweakDBID` | `array:TweakDBID` | `Red::TweakDBID` | 8 bytes (see §6) |
| 7 | `LocKey` | `array:LocKey` | `gamedataLocKeyWrapper` | 8-byte localization key wrapper |
| 8 | `ResRef` | `array:ResRef` | `ResourceAsyncReference<>` | Resource path hash (8 bytes) |
| 9 | `Quaternion` | `array:Quaternion` | `Red::Quaternion` | `{i, j, k, r}` × f32 (16 bytes) |
| 10 | `EulerAngles` | `array:EulerAngles` | `Red::EulerAngles` | `{Roll, Pitch, Yaw}` × f32 (12 bytes) |
| 11 | `Vector2` | `array:Vector2` | `Red::Vector2` | `{X, Y}` × f32 (8 bytes) |
| 12 | `Vector3` | `array:Vector3` | `Red::Vector3` | `{X, Y, Z}` × f32 (12 bytes) |
| 13 | `Color` | `array:Color` | `Red::Color` | `{Red, Green, Blue, Alpha}` × u8 (4 bytes) |

The type keyword used in `.tweak` files differs slightly from the internal type name: `int` (not `Int32`), `float`, `bool`, `string`, `CName`, `ResRef`, `LocKey`, and the struct types as named above. See `docs/reference/windows-tweakxl-api.md` §3 for grammar keywords.

The type registry is **compiled into the game binary** — we do not extend it. Our mod-loader maps mod values to existing type slots.

---

## 10. Windows vs. macOS — Known Divergences

### 10.1 On-disk format

The `tweakdb.bin` file format is **the same on Windows and macOS** — it is a serialization format loaded by a cross-platform reader. No platform-specific differences have been found in the file structure. (If evidence to the contrary surfaces, this section must be updated with a FA entry.)

### 10.2 In-memory representation — UNKNOWN / HYPOTHESIZED

This is the critical unknown for our project. The game loads `tweakdb.bin` into a C++ runtime structure. Windows and macOS use the same source file but compile differently, and old session findings show the layouts diverge significantly.

**FA-003:** Direct memory writes using Windows layout (binary search on `flats` sorted array at offset `0x40`) failed on macOS. That offset contained NULL. The old sessions concluded the macOS struct is structurally different.

**H-001 (open):** The Windows `flatDataBuffer` pointer may be a function-pointer dispatch table on macOS. Prior sessions showed code pointers at that location, not data. T-002 will validate this on the current build.

**H-002 (open):** macOS TweakDB runtime struct may be 0x198 bytes (vs. Windows 0x168). T-003 will find the constructor in Ghidra and measure.

**Practical implication:** The on-disk `value_offset` field in flat keys encodes something useful on Windows (a byte offset into `flatDataBuffer`). On macOS, the same field's meaning at runtime is unknown until H-001 is resolved. We cannot implement runtime flat-value access until T-002 / T-003 complete.

**Do not use Windows offsets on macOS.** See FA-003.

---

## 11. Cross-References

| Reference | Relevance |
|---|---|
| FA-002 | Why offline `tweakdb.bin` patching was abandoned |
| FA-003 | Why Windows flat-buffer offsets don't work on macOS |
| H-001 / T-002 | TweakDB dispatch table hypothesis — must resolve before runtime access |
| H-002 / T-003 | macOS TweakDB struct size hypothesis |
| `docs/reference/windows-tweakxl-api.md` | Type coercion + TweakDBID hashing (mod-side perspective) |
| WolvenKit GitHub | Canonical C# on-disk parser — ground truth for §2–§8 |
