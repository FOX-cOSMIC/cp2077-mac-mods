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

// One restored-write's worth of state: the flat id + its pre-edit typed value.
// F-031: snapshot via ReadScalarFlat (typed), rollback via EditScalarFlatInPlace.
struct UndoEntry {
    red4ext_mac::TweakDBID id;
    red4ext_mac::FlatValue oldValue;
};

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

        // ── Snapshot for atomic rollback (F-031: typed read off the +0x40 path)
        auto snap = red4ext_mac::ReadScalarFlat(db, id);
        if (!snap.has_value()) {
            ++r.rejected;
            rejectedThisMod = true;
            log_line("[applicator] reject op: flat not found/non-scalar (id=0x%08x len=%u name=%s)",
                     id.nameHash, id.nameLength, op.targetName.c_str());
            continue;  // keep scanning so all skips/rejects are accounted for;
                       // the whole mod is rolled back atomically at the end.
        }

        // ── Commit the write: edit the flat's FlatValue in the +0x148 buffer ──
        // NOTE: for record-cached values (e.g. stats) the change reaches gameplay
        // only after UpdateRecord re-materializes the record (F-030) — that step
        // is the next applicator addition; the flat itself is correctly mutated.
        if (!red4ext_mac::EditScalarFlatInPlace(db, id, newVal)) {
            ++r.rejected;
            rejectedThisMod = true;
            log_line("[applicator] reject op: EditScalarFlatInPlace failed (id=0x%08x name=%s)",
                     id.nameHash, op.targetName.c_str());
            continue;
        }

        ++r.applied;
        undo.push_back(UndoEntry{id, *snap});
    }

    // ── Atomic finalize: any rejection rolls back this mod's applied writes ──
    if (rejectedThisMod) {
        for (auto it = undo.rbegin(); it != undo.rend(); ++it) {
            if (!red4ext_mac::EditScalarFlatInPlace(db, it->id, it->oldValue)) {
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
