#pragma once
//
// Operation.hpp — the unified Op AST (P1.8, Option A).
//
// BOTH parsers target this AST: the YAML parser (P1.8) and the psiberx .tweak
// parser (P1.9) lower their input into a flat list of `Operation`s grouped in a
// `ModFile`. The applicator (P1.10) is the only consumer — it iterates the ops
// and calls Schema's ReadFlat/WriteFlat (runtime/TweakDB.hpp). The applicator
// never sees yaml-cpp or psiberx's TweakSource.
//
// WHY a dedicated AST (D-NNN candidate): the macOS port has NO TweakDB RTTI
// reflection at parse time, so the parser cannot do the record-vs-flat / element
// -type resolution the Windows YamlReader does inline. The AST is the clean seam
// where we capture *declared intent* (assign this name, append this value, clone
// from this base) and defer all live-DB resolution to P1.10, which holds the
// singleton + Schema's hash-map lookup. It also decouples us from psiberx's
// internal TweakSource types and gives one place to express Mac-specific
// concerns (e.g. the records-vs-flats hash-mode split Schema found, HashMap.hpp).
//
#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace tweakxl_mac {

// Packed 8-byte TweakDBID — mirrors Schema's runtime/HashMap.hpp::TweakDBID and
// binary-format §6 so the applicator can hand it to Lookup/ReadFlat/WriteFlat
// with at most a field-name copy. nameHash is CRC32(full dotted name) (zlib /
// ISO-3309 poly 0xEDB88320, spec §5). The applicator may recompute from
// `targetName` (authoritative) — this field is a parser-computed convenience
// that the unit tests assert against.
struct TweakDBID {
    uint32_t nameHash = 0;
    uint8_t  nameLength = 0;
    uint8_t  off[3] = {0, 0, 0};   // tdbOffset — 0 for name-derived IDs (spec §5)

    bool IsValid() const { return nameHash != 0 || nameLength != 0; }
    bool operator==(const TweakDBID& o) const {
        return nameHash == o.nameHash && nameLength == o.nameLength;
    }
};

enum class OpKind : uint8_t {
    Assign,         // replace flat value entirely (scalar or whole array)
    Append,         // !append / .tweak +=
    AppendOnce,     // !append-once
    AppendFrom,     // !append-from (value = source TweakDBID)
    Prepend,        // !prepend
    PrependOnce,    // !prepend-once
    PrependFrom,    // !prepend-from (value = source TweakDBID)
    Merge,          // !merge — alias of AppendFrom (kept distinct for diagnostics)
    Remove,         // !remove / .tweak -=
    RemoveAll,      // !remove-all (value unused)
    CreateRecord,   // $type given, no $base — value unused; typeName set
    CloneRecord,    // $base given — value unused; baseRecord (+ optional typeName) set
};

// The concrete value type the parser inferred (or that $type/.tweak declared).
// Without reflection this is a best-effort classification; the applicator
// reconciles it against the live flat's real type.
enum class ValueType : uint8_t {
    Unknown,
    Int32, Float, Bool, String, CName, TweakDBID,
    LocKey, ResRef, Quaternion, EulerAngles, Vector3, Vector2, Color,
    Array,
};

// Polymorphic literal carried from a mod file. Only the field(s) matching `type`
// are meaningful; the rest are default. `raw` always holds the original scalar
// text for diagnostics and as a fallback the applicator can re-coerce.
struct OpValue {
    ValueType type = ValueType::Unknown;

    int32_t     i = 0;          // Int32
    float       f = 0.0f;       // Float
    bool        b = false;      // Bool
    std::string s;              // String, and the cleaned inner text of
                                // CName / TweakDBID / LocKey / ResRef
    TweakDBID   id{};           // resolved id when type == TweakDBID

    // Struct components in canonical order:
    //   Quaternion {i,j,k,r} · EulerAngles {roll,pitch,yaw}
    //   Vector3 {x,y,z} · Vector2 {x,y} · Color {red,green,blue,alpha}
    std::vector<float> components;

    std::vector<OpValue> elements;   // type == Array

    std::string raw;            // original scalar text
};

struct Operation {
    OpKind      kind = OpKind::Assign;
    TweakDBID   target;             // hashed targetName
    std::string targetName;         // full dotted path (authoritative; diagnostics)
    OpValue     value;              // the literal (unused for RemoveAll/Create/Clone)

    // Record clone/create only:
    std::string baseRecord;         // $base / .tweak base group — empty if none
    std::string typeName;           // explicit $type / .tweak type — empty if none
};

struct ModFile {
    std::string path;
    std::string gameVersionGate;    // $game string, preserved (not evaluated in v1.0)
    std::string dlcGate;            // $dlc string, preserved (not evaluated in v1.0)
    bool        gated = false;      // a gate string was present (P1.10 may evaluate)
    std::vector<Operation>   ops;
    std::vector<std::string> warnings;   // non-fatal issues encountered while parsing
};

// ── Small name helpers (used by tests + the .tweak lowering in P1.10) ─────────
inline const char* OpKindName(OpKind k) {
    switch (k) {
        case OpKind::Assign:       return "Assign";
        case OpKind::Append:       return "Append";
        case OpKind::AppendOnce:   return "AppendOnce";
        case OpKind::AppendFrom:   return "AppendFrom";
        case OpKind::Prepend:      return "Prepend";
        case OpKind::PrependOnce:  return "PrependOnce";
        case OpKind::PrependFrom:  return "PrependFrom";
        case OpKind::Merge:        return "Merge";
        case OpKind::Remove:       return "Remove";
        case OpKind::RemoveAll:    return "RemoveAll";
        case OpKind::CreateRecord: return "CreateRecord";
        case OpKind::CloneRecord:  return "CloneRecord";
    }
    return "?";
}

inline const char* ValueTypeName(ValueType t) {
    switch (t) {
        case ValueType::Unknown:     return "Unknown";
        case ValueType::Int32:       return "Int32";
        case ValueType::Float:       return "Float";
        case ValueType::Bool:        return "Bool";
        case ValueType::String:      return "String";
        case ValueType::CName:       return "CName";
        case ValueType::TweakDBID:   return "TweakDBID";
        case ValueType::LocKey:      return "LocKey";
        case ValueType::ResRef:      return "ResRef";
        case ValueType::Quaternion:  return "Quaternion";
        case ValueType::EulerAngles: return "EulerAngles";
        case ValueType::Vector3:     return "Vector3";
        case ValueType::Vector2:     return "Vector2";
        case ValueType::Color:       return "Color";
        case ValueType::Array:       return "Array";
    }
    return "?";
}

} // namespace tweakxl_mac
