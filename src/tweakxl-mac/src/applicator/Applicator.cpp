//
// Applicator.cpp — P1.10a SCOPED applicator (scalar Assign only).
//
// See Applicator.hpp for scope + atomicity contract. The narrow rules:
//   kind != Assign                      -> skipped (P1.10b)
//   Assign of a non-scalar value type   -> skipped (P1.10b)
//   Assign scalar, flat not found       -> rejected
//   Assign scalar, WriteFlat failed     -> rejected
//   Assign scalar, written              -> applied (+ pushed on the undo stack)
// End of mod: any rejection -> restore the undo stack in reverse (atomic).
//
#include "Applicator.hpp"

#include "runtime/TweakDB.hpp"   // red4ext_mac::TweakDB, ReadFlat, WriteFlat
#include "runtime/Values.hpp"    // FlatType, FlatValue, IsScalarFlatType
#include "runtime/HashMap.hpp"   // red4ext_mac::TweakDBID

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <variant>
#include <vector>

namespace tweakxl_mac {
namespace {

// On-host the applicator logs to stderr (visible in the unit-test run). The
// in-game stack writes to /tmp/red4ext-mac.log via Schema's logger; P1.11 can
// route these through the same sink if a unified log is wanted (see FOLLOW-UPS).
void log_line(const char* fmt, ...) {
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    std::fprintf(stderr, "%s\n", buf);
}

// Our parser's 8-byte TweakDBID and Schema's are byte-identical (Operation.hpp
// mirrors HashMap.hpp); a direct copy is the documented seam (no adapter layer).
red4ext_mac::TweakDBID ToRuntimeId(const TweakDBID& src) {
    red4ext_mac::TweakDBID dst;
    static_assert(sizeof(dst) == sizeof(src),
                  "tweakxl_mac::TweakDBID and red4ext_mac::TweakDBID must share a byte layout");
    std::memcpy(&dst, &src, sizeof(dst));
    return dst;
}

// Lower an OpValue's inferred scalar type into a runtime FlatValue. Returns
// false for any non-scalar / unsupported value type (deferred to P1.10b).
bool ToScalarFlatValue(const OpValue& v, red4ext_mac::FlatValue* out) {
    switch (v.type) {
        case ValueType::Int32:
            out->type  = red4ext_mac::FlatType::Int32;
            out->value = static_cast<int32_t>(v.i);
            return true;
        case ValueType::Float:
            out->type  = red4ext_mac::FlatType::Float;
            out->value = v.f;
            return true;
        case ValueType::Bool:
            out->type  = red4ext_mac::FlatType::Bool;
            out->value = v.b;
            return true;
        default:
            return false;
    }
}

// Rebuild a typed FlatValue from a snapshot's raw 4 bytes for rollback.
// ReadFlat returns FlatType::Unknown carrying the raw uint32 (the vft->FlatType
// map is a pending Schema follow-up); WriteFlat REFUSES Unknown, so we re-clothe
// the bytes in the scalar type we originally wrote — the type is authoritative
// here because we only ever wrote that flat as that type.
red4ext_mac::FlatValue RestoreValue(red4ext_mac::FlatType type, uint32_t raw) {
    red4ext_mac::FlatValue fv;
    fv.type = type;
    switch (type) {
        case red4ext_mac::FlatType::Int32: {
            int32_t i;
            std::memcpy(&i, &raw, sizeof(i));
            fv.value = i;
            break;
        }
        case red4ext_mac::FlatType::Float: {
            float f;
            std::memcpy(&f, &raw, sizeof(f));
            fv.value = f;
            break;
        }
        case red4ext_mac::FlatType::Bool:
            fv.value = (raw & 0xFFu) != 0u;
            break;
        default:
            break;  // unreachable: we only push scalar types onto the undo stack
    }
    return fv;
}

// One restored-write's worth of state: where, what type, and the old raw bytes.
struct UndoEntry {
    red4ext_mac::TweakDBID id;
    red4ext_mac::FlatType  type;
    uint32_t               oldRaw;
};

// Extract the raw 4 bytes from a snapshot FlatValue. ReadFlat reports scalars as
// FlatType::Unknown carrying a uint32 today, but tolerate an int32 alternative
// too in case the vft->type map lands later and ReadFlat starts typing reads.
uint32_t SnapshotRaw(const red4ext_mac::FlatValue& fv) {
    if (std::holds_alternative<uint32_t>(fv.value))
        return std::get<uint32_t>(fv.value);
    if (std::holds_alternative<int32_t>(fv.value))
        return static_cast<uint32_t>(std::get<int32_t>(fv.value));
    if (std::holds_alternative<float>(fv.value)) {
        float f = std::get<float>(fv.value);
        uint32_t raw;
        std::memcpy(&raw, &f, sizeof(raw));
        return raw;
    }
    if (std::holds_alternative<bool>(fv.value))
        return std::get<bool>(fv.value) ? 1u : 0u;
    return 0u;
}

} // namespace

ApplyResult ApplyMod(red4ext_mac::TweakDB* db, const ModFile& mod) {
    ApplyResult r;
    std::vector<UndoEntry> undo;
    bool rejectedThisMod = false;

    for (const auto& op : mod.ops) {
        // ── Deferred op kinds (P1.10b): everything but Assign ────────────────
        if (op.kind != OpKind::Assign) {
            ++r.skipped;
            log_line("[applicator] skip op: kind=%s not yet supported (P1.10b) [%s]",
                     OpKindName(op.kind), op.targetName.c_str());
            continue;
        }

        // ── Deferred value types (P1.10b): non-scalar Assign ─────────────────
        red4ext_mac::FlatValue newVal;
        if (!ToScalarFlatValue(op.value, &newVal)) {
            ++r.skipped;
            log_line("[applicator] skip op: value type %s not scalar (P1.10b) [%s]",
                     ValueTypeName(op.value.type), op.targetName.c_str());
            continue;
        }

        const red4ext_mac::TweakDBID id = ToRuntimeId(op.target);

        // ── Snapshot for atomic rollback (the spec's snapshot mechanism) ─────
        auto snap = red4ext_mac::ReadFlat(db, id);
        if (!snap.has_value()) {
            ++r.rejected;
            rejectedThisMod = true;
            log_line("[applicator] reject op: flat not found (id=0x%08x len=%u name=%s)",
                     id.nameHash, id.nameLength, op.targetName.c_str());
            continue;  // keep scanning so all skips/rejects are accounted for;
                       // the whole mod is rolled back atomically at the end.
        }
        const uint32_t oldRaw = SnapshotRaw(*snap);

        // ── Commit the write ─────────────────────────────────────────────────
        if (!red4ext_mac::WriteFlat(db, id, newVal)) {
            ++r.rejected;
            rejectedThisMod = true;
            log_line("[applicator] reject op: WriteFlat failed (id=0x%08x name=%s)",
                     id.nameHash, op.targetName.c_str());
            continue;
        }

        ++r.applied;
        undo.push_back(UndoEntry{id, newVal.type, oldRaw});
    }

    // ── Atomic finalize: any rejection rolls back this mod's applied writes ──
    if (rejectedThisMod) {
        for (auto it = undo.rbegin(); it != undo.rend(); ++it) {
            const red4ext_mac::FlatValue restore = RestoreValue(it->type, it->oldRaw);
            if (!red4ext_mac::WriteFlat(db, it->id, restore)) {
                log_line("[applicator] WARNING: rollback write failed (id=0x%08x) "
                         "-- state may be inconsistent", it->id.nameHash);
            }
        }
        if (!undo.empty()) {
            ++r.mods_rolled_back;
            log_line("[applicator] mod rolled back: %s (restored %zu write(s), rejected=%u)",
                     mod.path.c_str(), undo.size(), r.rejected);
        } else {
            log_line("[applicator] mod rejected: %s (rejected=%u, no writes to roll back)",
                     mod.path.c_str(), r.rejected);
        }
    } else {
        ++r.mods_ok;
        log_line("[applicator] mod applied: %s (applied=%u skipped=%u)",
                 mod.path.c_str(), r.applied, r.skipped);
    }

    return r;
}

} // namespace tweakxl_mac
