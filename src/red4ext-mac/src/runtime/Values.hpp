#pragma once
//
// Values.hpp — P1.6 TweakDB flat value type system.
//
// FlatType enumerates the game's 26 flat types (13 scalar + 13 array,
// binary-format §9). FlatValue is a tagged value used by ReadFlat/WriteFlat
// (TweakDB.hpp). v1.0 carries the scalar types as live variant alternatives;
// String/CName/struct/array types are enumerated for completeness but carried
// as std::monostate until P1.6+ extends decoding (Read-only first, per spec).
//
// The mapping FlatType ↔ the live runtime type discriminator (the FlatValue
// object's vft pointer) is NOT yet known on macOS — see VerifyFlatEntry() in
// TweakDB.cpp. Until that map exists, ReadFlat reports Unknown but still hands
// back the raw scalar bytes; WriteFlat writes by the caller-declared scalar
// type (P1.10 knows the type from the mod source).
//
#include <cstdint>
#include <string>
#include <variant>

namespace red4ext_mac {

// Order mirrors binary-format §9 (scalars 1..13, then array variants).
enum class FlatType : uint8_t {
    Int32 = 1,
    Float,
    Bool,
    String,
    CName,
    TweakDBID,
    LocKey,
    ResRef,
    Quaternion,
    EulerAngles,
    Vector2,
    Vector3,
    Color,

    Int32Array,
    FloatArray,
    BoolArray,
    StringArray,
    CNameArray,
    TweakDBIDArray,
    LocKeyArray,
    ResRefArray,
    QuaternionArray,
    EulerAnglesArray,
    Vector2Array,
    Vector3Array,
    ColorArray,

    Unknown = 0xFF,
};

// A flat value. The active variant alternative must agree with `type` for the
// scalar types that v1.0 supports; everything else is std::monostate for now.
struct FlatValue {
    FlatType type = FlatType::Unknown;
    std::variant<
        std::monostate,   // Unknown / unsupported-yet types
        int32_t,          // Int32
        uint32_t,         // raw 4-byte scalar (also used for untyped reads)
        float,            // Float
        bool,             // Bool
        std::string       // String (Read-only path for now)
    > value{std::monostate{}};
};

// True for the scalar types P1.6 reads/writes in place (fixed-size, no heap).
inline bool IsScalarFlatType(FlatType t) {
    switch (t) {
        case FlatType::Int32:
        case FlatType::Float:
        case FlatType::Bool:
            return true;
        default:
            return false;
    }
}

// Byte width of a scalar value's in-buffer storage. 0 for non-scalars.
inline uint32_t ScalarFlatTypeSize(FlatType t) {
    switch (t) {
        case FlatType::Int32:
        case FlatType::Float:
            return 4;
        case FlatType::Bool:
            return 1;          // stored as 1 byte (binary-format §9)
        default:
            return 0;
    }
}

inline const char* FlatTypeName(FlatType t) {
    switch (t) {
        case FlatType::Int32:       return "Int32";
        case FlatType::Float:       return "Float";
        case FlatType::Bool:        return "Bool";
        case FlatType::String:      return "String";
        case FlatType::CName:       return "CName";
        case FlatType::TweakDBID:   return "TweakDBID";
        case FlatType::LocKey:      return "LocKey";
        case FlatType::ResRef:      return "ResRef";
        case FlatType::Quaternion:  return "Quaternion";
        case FlatType::EulerAngles: return "EulerAngles";
        case FlatType::Vector2:     return "Vector2";
        case FlatType::Vector3:     return "Vector3";
        case FlatType::Color:       return "Color";
        case FlatType::Unknown:     return "Unknown";
        default:                    return "Array/Other";
    }
}

} // namespace red4ext_mac
