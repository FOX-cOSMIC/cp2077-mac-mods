#include "YAML.hpp"

#include <yaml-cpp/yaml.h>

#include <array>
#include <cstdio>
#include <ctime>
#include <mutex>
#include <optional>
#include <string>

// P1.8 — YAML mod-file parser.
//
// A macOS port of psiberx's App::YamlReader (reference/.../Yaml/YamlReader.cpp),
// lowering YAML into the unified Op AST (model/Operation.hpp). Compatibility
// contract (windows-tweakxl-api.md): where this disagrees with the reference for
// a real mod, this is wrong.
//
// KEY STRUCTURAL DIFFERENCE FROM WINDOWS: the Windows reader resolves
// record-vs-flat, flat types, and array element types from live TweakDB RTTI
// *during parse*. The macOS port has no reflection at parse time, so this parser
// captures declared intent into the AST and DEFERS type/record resolution to the
// applicator (P1.10), which holds the live singleton + Schema's lookup. The
// consequences (documented inline where they bite):
//   * A bare mapping with no $base/$type is treated as an existing-record /
//     free-flat property edit — Assign ops on composed `Name.prop` names — since
//     we can't ask reflection whether the target is a record instance.
//   * Scalar type inference follows windows-tweakxl-api §6.1, but a value we
//     can't classify is emitted as String(raw) + a warning rather than dropped,
//     so P1.10 can re-coerce against the real flat type instead of losing data.
//   * Legacy `groups`/`flats` format is NOT converted — matching the reference,
//     where ConvertLegacyNodes() is commented out (code wins over the doc).
//   * $instances templates are not expanded (logged + skipped) — a P1.x stretch.

namespace {
using namespace tweakxl_mac;

constexpr char   kAttrSymbol = '$';
constexpr const char* kMacLogPath = "/tmp/red4ext-mac.log";

void LogFatal(const std::string& message) {
    std::fprintf(stderr, "[tweakxl/yaml] %s\n", message.c_str());
    static std::mutex logMutex;
    std::lock_guard<std::mutex> lock(logMutex);
    if (FILE* log = std::fopen(kMacLogPath, "a")) {
        std::time_t now = std::time(nullptr);
        char stamp[32];
        std::strftime(stamp, sizeof(stamp), "%Y-%m-%d %H:%M:%S", std::localtime(&now));
        std::fprintf(log, "%s [tweakxl/yaml] %s\n", stamp, message.c_str());
        std::fclose(log);
    }
}

// ── TweakDBID hashing — standard zlib/ISO-3309 CRC32 (spec §5) ────────────────
uint32_t Crc32(const void* data, size_t len) {
    static const std::array<uint32_t, 256> table = [] {
        std::array<uint32_t, 256> t{};
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t c = i;
            for (int k = 0; k < 8; ++k)
                c = (c & 1u) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            t[i] = c;
        }
        return t;
    }();
    uint32_t crc = 0xFFFFFFFFu;
    const auto* p = static_cast<const uint8_t*>(data);
    for (size_t i = 0; i < len; ++i)
        crc = table[(crc ^ p[i]) & 0xFFu] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFFu;
}

TweakDBID MakeID(const std::string& name) {
    TweakDBID id;
    id.nameHash = Crc32(name.data(), name.size());
    id.nameLength = static_cast<uint8_t>(name.size());
    return id;
}

std::string ComposeName(const std::string& a, const std::string& b) {
    return a + "." + b;
}

// ── Prefix-form detection (windows-tweakxl-api §5/§6) ─────────────────────────
// Each returns true and fills `inner` with the cleaned payload if the scalar
// matches that type's quoted/wrapped form.
bool between(const std::string& s, const char* pre, const char* suf, std::string& inner) {
    const std::string p = pre, q = suf;
    if (s.size() >= p.size() + q.size() && s.compare(0, p.size(), p) == 0 &&
        s.compare(s.size() - q.size(), q.size(), q) == 0) {
        inner = s.substr(p.size(), s.size() - p.size() - q.size());
        return true;
    }
    return false;
}

bool MatchCName(const std::string& s, std::string& inner) {
    return between(s, "n\"", "\"", inner) || between(s, "CName(\"", "\")", inner);
}
bool MatchTweakDBID(const std::string& s, std::string& inner) {
    return between(s, "t\"", "\"", inner) || between(s, "TweakDBID(\"", "\")", inner);
}
bool MatchLocKey(const std::string& s, std::string& inner) {
    if (between(s, "l\"", "\"", inner)) return true;
    if (between(s, "LocKey(", ")", inner)) return true;
    if (s.compare(0, 7, "LocKey#") == 0) { inner = s.substr(7); return true; }
    return false;
}
bool MatchResRef(const std::string& s, std::string& inner) {
    return between(s, "r\"", "\"", inner) || between(s, "ResRef(", ")", inner);
}

// Debug TweakDBID form <TDBID:HHHHHHHH:LL>. Fills id directly.
bool MatchDebugTweakDBID(const std::string& s, TweakDBID& id) {
    // "<TDBID:" + 8 hex + ":" + 2 hex + ">" == length 19
    if (s.size() != 19 || s.compare(0, 7, "<TDBID:") != 0 || s.back() != '>') return false;
    if (s[15] != ':') return false;
    try {
        id.nameHash = static_cast<uint32_t>(std::stoul(s.substr(7, 8), nullptr, 16));
        id.nameLength = static_cast<uint8_t>(std::stoul(s.substr(16, 2), nullptr, 16));
        return true;
    } catch (...) { return false; }
}

// ── Scalar inference (windows-tweakxl-api §6.1 probe order) ───────────────────
// Quoted scalars carry yaml-cpp tag "!" → String (verified empirically). Plain
// scalars probe int → float → bool → typed prefixes. A scalar that matches none
// is emitted as String(raw): Windows would call it "ambiguous" and drop it, but
// we keep the data and let the applicator re-coerce against the live flat type
// (callers warn when this fallback fires on an unquoted scalar).
OpValue InferScalar(const YAML::Node& node) {
    OpValue v;
    const std::string s = node.IsScalar() ? node.Scalar() : std::string();
    v.raw = s;

    // 1. explicitly-quoted string
    if (node.Tag() == "!") { v.type = ValueType::String; v.s = s; return v; }

    // 2/3/4. numeric / bool (int before float, per §6.1)
    int iv; float fv; bool bv;
    if (YAML::convert<int>::decode(node, iv))   { v.type = ValueType::Int32; v.i = iv; return v; }
    if (YAML::convert<float>::decode(node, fv)) { v.type = ValueType::Float; v.f = fv; return v; }
    if (YAML::convert<bool>::decode(node, bv))  { v.type = ValueType::Bool;  v.b = bv; return v; }

    // 5/6/7/8. typed prefixes
    std::string inner;
    if (MatchCName(s, inner))     { v.type = ValueType::CName;  v.s = inner; return v; }
    if (MatchTweakDBID(s, inner)) { v.type = ValueType::TweakDBID; v.s = inner; v.id = MakeID(inner); return v; }
    TweakDBID dbg;
    if (MatchDebugTweakDBID(s, dbg)) { v.type = ValueType::TweakDBID; v.id = dbg; v.s = s; return v; }
    if (s == "None")              { v.type = ValueType::TweakDBID; v.s = ""; return v; }   // zeroed id
    if (MatchLocKey(s, inner))    { v.type = ValueType::LocKey; v.s = inner; return v; }
    if (MatchResRef(s, inner))    { v.type = ValueType::ResRef; v.s = inner; return v; }

    // fallback (see header note): keep as String(raw)
    v.type = ValueType::String;
    v.s = s;
    return v;
}

// ── Struct inference (TryMakeValue Map order: Quat, Euler, Vec3, Vec2, Color) ──
std::optional<OpValue> InferStruct(const YAML::Node& node) {
    if (!node.IsMap()) return std::nullopt;
    auto has = [&](const char* k) { return node[k].IsDefined(); };
    auto comp = [&](std::initializer_list<const char*> keys, ValueType t) {
        OpValue v; v.type = t;
        for (const char* k : keys) v.components.push_back(node[k].as<float>(0.0f));
        return v;
    };
    if (has("i") && has("j") && has("k") && has("r"))
        return comp({"i", "j", "k", "r"}, ValueType::Quaternion);
    if (has("roll") && has("pitch") && has("yaw"))
        return comp({"roll", "pitch", "yaw"}, ValueType::EulerAngles);
    if (has("x") && has("y") && has("z"))
        return comp({"x", "y", "z"}, ValueType::Vector3);
    if (has("x") && has("y"))
        return comp({"x", "y"}, ValueType::Vector2);
    if (has("red") && has("green") && has("blue") && has("alpha"))
        return comp({"red", "green", "blue", "alpha"}, ValueType::Color);
    return std::nullopt;
}

// Map a $type / .tweak type name to a ValueType (scalar names only; arrays and
// unknown names are handled by the caller). Accepts both "Int32" (YAML $type)
// and "int" (.tweak) spellings.
ValueType TypeNameToValueType(const std::string& name) {
    if (name == "Int32" || name == "int")          return ValueType::Int32;
    if (name == "Float" || name == "float")         return ValueType::Float;
    if (name == "Bool"  || name == "bool")          return ValueType::Bool;
    if (name == "String" || name == "string")       return ValueType::String;
    if (name == "CName")                            return ValueType::CName;
    if (name == "TweakDBID")                        return ValueType::TweakDBID;
    if (name == "LocKey")                           return ValueType::LocKey;
    if (name == "ResRef")                           return ValueType::ResRef;
    if (name == "Quaternion")                       return ValueType::Quaternion;
    if (name == "EulerAngles")                      return ValueType::EulerAngles;
    if (name == "Vector3")                          return ValueType::Vector3;
    if (name == "Vector2")                          return ValueType::Vector2;
    if (name == "Color")                            return ValueType::Color;
    return ValueType::Unknown;
}

// Mutation tag → OpKind. Returns false for an unrecognized tag.
bool TagToOpKind(const std::string& tag, OpKind& out) {
    if (tag == "!append")        { out = OpKind::Append;      return true; }
    if (tag == "!append-once")   { out = OpKind::AppendOnce;  return true; }
    if (tag == "!append-from")   { out = OpKind::AppendFrom;  return true; }
    if (tag == "!prepend")       { out = OpKind::Prepend;     return true; }
    if (tag == "!prepend-once")  { out = OpKind::PrependOnce; return true; }
    if (tag == "!prepend-from")  { out = OpKind::PrependFrom; return true; }
    if (tag == "!merge")         { out = OpKind::Merge;       return true; }
    if (tag == "!remove")        { out = OpKind::Remove;      return true; }
    if (tag == "!remove-all")    { out = OpKind::RemoveAll;   return true; }
    return false;
}

bool IsFromKind(OpKind k) {
    return k == OpKind::AppendFrom || k == OpKind::PrependFrom || k == OpKind::Merge;
}

// Resolve a node as a TweakDBID source (for !append-from / !merge / !prepend-from).
OpValue MakeSourceID(const YAML::Node& node) {
    OpValue v; v.type = ValueType::TweakDBID;
    if (node.IsScalar()) {
        const std::string s = node.Scalar();
        std::string inner;
        if (MatchTweakDBID(s, inner)) { v.s = inner; v.id = MakeID(inner); }
        else if (s == "None")          { v.s = ""; }
        else                           { v.s = s; v.id = MakeID(s); }   // plain path
    }
    return v;
}

// Build a plain-array OpValue (each element inferred leniently via InferScalar).
OpValue BuildArrayValue(const YAML::Node& node) {
    OpValue v; v.type = ValueType::Array;
    for (const auto& item : node) {
        if (item.IsMap()) {
            if (auto st = InferStruct(item)) { v.elements.push_back(*st); continue; }
        }
        v.elements.push_back(InferScalar(item));
    }
    return v;
}

// Forward decls for mutual recursion.
void HandleProperties(const std::string& recordName, const YAML::Node& node, ModFile& mod);

// Returns true if the sequence carried mutation tags (ops emitted). False means
// "plain array" — the caller should emit a single Assign with BuildArrayValue.
// Faithful to YamlReader::HandleMutations.
bool HandleSequence(const std::string& name, const YAML::Node& node, ModFile& mod) {
    if (!node.IsSequence()) return false;

    bool isMutation = false, isAssignment = false;
    std::vector<Operation> muts;

    for (std::size_t i = 0; i < node.size(); ++i) {
        const auto item = node[i];
        const std::string tag = item.Tag();
        if (tag.size() <= 1) { isAssignment = true; continue; }   // untagged item

        OpKind kind;
        if (!TagToOpKind(tag, kind)) {
            mod.warnings.push_back(name + "[" + std::to_string(i) + "]: Invalid action " + tag + ".");
            continue;
        }

        Operation op;
        op.kind = kind;
        op.targetName = name;
        op.target = MakeID(name);
        if (kind == OpKind::RemoveAll) {
            // no value
        } else if (IsFromKind(kind)) {
            op.value = MakeSourceID(item);
        } else {
            op.value = InferScalar(item);
        }
        muts.push_back(std::move(op));
        isMutation = true;
    }

    if (isMutation && isAssignment) {
        mod.warnings.push_back(name + ": Mixed definition of array replacement and mutations. "
                                      "Only mutations will take effect.");
    }
    if (isMutation) {
        for (auto& op : muts) mod.ops.push_back(std::move(op));
        return true;
    }
    return false;
}

void EmitAssign(const std::string& name, OpValue value, ModFile& mod) {
    Operation op;
    op.kind = OpKind::Assign;
    op.targetName = name;
    op.target = MakeID(name);
    op.value = std::move(value);
    mod.ops.push_back(std::move(op));
}

// Coerce a value node to a declared scalar/struct type (for $type + $value, and
// for .tweak-style typed flats reused later). Returns nullopt if the node shape
// is incompatible with the declared type.
std::optional<OpValue> MakeTypedValue(const YAML::Node& node, ValueType vt, const std::string& typeName) {
    OpValue v; v.type = vt;
    switch (vt) {
        case ValueType::Int32: { int x;   if (YAML::convert<int>::decode(node, x))   { v.i = x; v.raw = node.Scalar(); return v; } return std::nullopt; }
        case ValueType::Float: { float x; if (YAML::convert<float>::decode(node, x)) { v.f = x; v.raw = node.Scalar(); return v; } return std::nullopt; }
        case ValueType::Bool:  { bool x;  if (YAML::convert<bool>::decode(node, x))  { v.b = x; v.raw = node.Scalar(); return v; } return std::nullopt; }
        case ValueType::String: case ValueType::CName: case ValueType::LocKey:
        case ValueType::ResRef: {
            if (!node.IsScalar()) return std::nullopt;
            v.s = node.Scalar(); v.raw = v.s; return v;
        }
        case ValueType::TweakDBID: {
            if (!node.IsScalar()) return std::nullopt;
            return MakeSourceID(node);
        }
        case ValueType::Quaternion: case ValueType::EulerAngles:
        case ValueType::Vector3: case ValueType::Vector2: case ValueType::Color: {
            if (auto st = InferStruct(node); st && st->type == vt) return st;
            return std::nullopt;
        }
        default:
            (void)typeName;
            return std::nullopt;
    }
}

// Emit a clone/create record op. Returns the kind chosen for diagnostics.
void EmitRecordOp(OpKind kind, const std::string& name, const std::string& base,
                  const std::string& typeName, ModFile& mod) {
    Operation op;
    op.kind = kind;
    op.targetName = name;
    op.target = MakeID(name);
    op.baseRecord = base;
    op.typeName = typeName;
    mod.ops.push_back(std::move(op));
}

// Resolve a $base / $type attribute's scalar string (handles t"..." TweakDBID form).
std::string AttrString(const YAML::Node& attr) {
    if (!attr.IsScalar()) return std::string();
    const std::string s = attr.Scalar();
    std::string inner;
    if (MatchTweakDBID(s, inner)) return inner;
    return s;
}

// Iterate a record block's properties (non-$ keys), composing `recordName.key`.
void HandleProperties(const std::string& recordName, const YAML::Node& node, ModFile& mod) {
    if (!node.IsMap()) return;
    for (const auto& it : node) {
        if (!it.first.IsScalar()) continue;
        const std::string key = it.first.Scalar();
        if (key.empty() || key[0] == kAttrSymbol) continue;   // skip attributes

        const std::string propName = ComposeName(recordName, key);
        const YAML::Node val = it.second;

        switch (val.Type()) {
        case YAML::NodeType::Scalar: {
            OpValue v = InferScalar(val);
            if (v.type == ValueType::String && val.Tag() != "!")
                mod.warnings.push_back(propName + ": inferred String for unquoted scalar; "
                                                  "declare $type for a precise type.");
            EmitAssign(propName, std::move(v), mod);
            break;
        }
        case YAML::NodeType::Sequence: {
            if (!HandleSequence(propName, val, mod))
                EmitAssign(propName, BuildArrayValue(val), mod);
            break;
        }
        case YAML::NodeType::Map: {
            if (auto st = InferStruct(val)) { EmitAssign(propName, *st, mod); break; }
            // Inline record (nested $base/$type) or nested record edit.
            const auto baseAttr = val["$base"];
            const auto typeAttr = val["$type"];
            const auto valueAttr = val["$value"];
            if (baseAttr.IsDefined()) {
                EmitRecordOp(OpKind::CloneRecord, propName, AttrString(baseAttr),
                             typeAttr.IsDefined() ? AttrString(typeAttr) : "", mod);
                mod.warnings.push_back(propName + ": inline record cloned as a named sub-record; "
                                                  "Windows auto-synthesizes inline FK names (needs reflection).");
                HandleProperties(propName, val, mod);
            } else if (typeAttr.IsDefined() && valueAttr.IsDefined()) {
                const std::string tn = AttrString(typeAttr);
                ValueType vt = TypeNameToValueType(tn);
                if (auto tv = MakeTypedValue(valueAttr, vt, tn)) EmitAssign(propName, *tv, mod);
                else mod.warnings.push_back(propName + ": Invalid value, expected " + tn + ".");
            } else if (typeAttr.IsDefined()) {
                EmitRecordOp(OpKind::CreateRecord, propName, "", AttrString(typeAttr), mod);
                HandleProperties(propName, val, mod);
            } else {
                HandleProperties(propName, val, mod);   // nested record edit
            }
            break;
        }
        default:
            mod.warnings.push_back(propName + ": Bad format. Unexpected data.");
            break;
        }
    }
}

// Top-level map entry decision tree (mirrors YamlReader::HandleTopNode, minus
// the reflection-gated record-instance/flat-instance branches we can't run).
void HandleMapEntry(const std::string& name, const YAML::Node& node, ModFile& mod) {
    const auto baseAttr  = node["$base"];
    const auto typeAttr  = node["$type"];
    const auto valueAttr = node["$value"];

    if (node["$instances"].IsDefined()) {
        mod.warnings.push_back(name + ": $instances templates are not supported yet; entry skipped.");
        return;
    }

    if (baseAttr.IsDefined()) {
        EmitRecordOp(OpKind::CloneRecord, name, AttrString(baseAttr),
                     typeAttr.IsDefined() ? AttrString(typeAttr) : "", mod);
        HandleProperties(name, node, mod);
        return;
    }
    if (typeAttr.IsDefined() && valueAttr.IsDefined()) {
        const std::string tn = AttrString(typeAttr);
        ValueType vt = TypeNameToValueType(tn);
        if (auto tv = MakeTypedValue(valueAttr, vt, tn)) EmitAssign(name, *tv, mod);
        else mod.warnings.push_back(name + ": Invalid value type " + tn + ".");
        return;
    }
    if (typeAttr.IsDefined()) {
        EmitRecordOp(OpKind::CreateRecord, name, "", AttrString(typeAttr), mod);
        HandleProperties(name, node, mod);
        return;
    }
    if (auto st = InferStruct(node)) { EmitAssign(name, *st, mod); return; }

    // Bare mapping: existing-record / free-flat property edit (see header note).
    HandleProperties(name, node, mod);
}

void HandleTopNode(const std::string& name, const YAML::Node& node, ModFile& mod) {
    if (name.empty()) {
        mod.warnings.push_back("Bad format. The top element must have a name.");
        return;
    }
    if (name[0] == kAttrSymbol) return;   // top-level attribute key, handled elsewhere

    switch (node.Type()) {
    case YAML::NodeType::Scalar: {
        OpValue v = InferScalar(node);
        if (v.type == ValueType::String && node.Tag() != "!")
            mod.warnings.push_back(name + ": inferred String for unquoted scalar; "
                                          "declare $type for a precise type.");
        EmitAssign(name, std::move(v), mod);
        break;
    }
    case YAML::NodeType::Sequence:
        if (!HandleSequence(name, node, mod))
            EmitAssign(name, BuildArrayValue(node), mod);
        break;
    case YAML::NodeType::Map:
        HandleMapEntry(name, node, mod);
        break;
    default:
        mod.warnings.push_back(name + ": Bad format. Unexpected data.");
        break;
    }
}

} // namespace

std::unique_ptr<ModFile> tweakxl_mac::ParseYamlFile(const std::filesystem::path& path) {
    YAML::Node root;
    try {
        root = YAML::LoadFile(path.string());
    } catch (const std::exception& ex) {
        LogFatal("Failed to load \"" + path.string() + "\": " + ex.what());
        return nullptr;
    }

    if (!root.IsDefined() || root.IsNull()) {
        LogFatal("Empty or undefined YAML: \"" + path.string() + "\".");
        return nullptr;
    }
    if (!root.IsMap()) {
        LogFatal("\"" + path.string() + "\": Bad format. Unexpected data at the top level.");
        return nullptr;
    }

    auto mod = std::make_unique<ModFile>();
    mod->path = path.string();

    // $game / $dlc gates — preserved, not evaluated (v1.0).
    if (const auto g = root["$game"]; g.IsDefined() && g.IsScalar()) {
        mod->gameVersionGate = g.Scalar();
        mod->gated = true;
    }
    if (const auto d = root["$dlc"]; d.IsDefined() && d.IsScalar()) {
        mod->dlcGate = d.Scalar();
        mod->gated = true;
    }

    try {
        for (const auto& it : root) {
            if (!it.first.IsScalar()) continue;
            HandleTopNode(it.first.Scalar(), it.second, *mod);
        }
    } catch (const std::exception& ex) {
        LogFatal("\"" + path.string() + "\": parse error: " + ex.what());
        return nullptr;
    }

    return mod;
}
