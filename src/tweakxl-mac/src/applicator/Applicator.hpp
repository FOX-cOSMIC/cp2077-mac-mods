#pragma once
//
// Applicator.hpp — P1.10a SCOPED applicator (scalar Assign only).
//
// Consumes the Op AST (model/Operation.hpp) and mutates the live TweakDB via
// Schema's primitives (runtime/TweakDB.hpp ReadFlat/WriteFlat). This first slice
// handles ONLY OpKind::Assign targeting scalar flats (Int32/Float/Bool); every
// other op kind and every non-scalar value is logged "not yet supported" and
// counted as skipped — arrays, records and the other op kinds land in P1.10b.
//
// Atomic per-mod (AGENTS.md constraint): a snapshot of every flat we touch is
// taken via ReadFlat *before* writing; if any op in the mod is rejected, all of
// this mod's writes are restored in reverse order. The game is never left in a
// partially-mutated state by a failed mod.
//
#include <cstdint>
#include "../model/Operation.hpp"

namespace red4ext_mac { struct TweakDB; }

namespace tweakxl_mac {

    struct ApplyResult {
        uint32_t applied   = 0;
        uint32_t skipped   = 0;  // non-Assign or non-scalar; not yet supported
        uint32_t rejected  = 0;  // type mismatch, lookup miss, etc.
        uint32_t mods_ok   = 0;  // mods fully applied
        uint32_t mods_rolled_back = 0;  // mods where atomic guarantee triggered rollback
    };

    // Apply a single mod file's operations atomically. On any rejection,
    // restore all writes from this mod (using snapshot taken before applying).
    // Returns counts. Game state never partially mutated by a failed mod.
    //
    // Counter semantics worth noting for the caller:
    //   * A rejected mod that managed ≥1 successful write before failing is
    //     rolled back and counted in mods_rolled_back (and NOT mods_ok).
    //   * A rejected mod that never wrote anything (e.g. the very first op
    //     missed) has nothing to roll back: it is counted in neither mods_ok
    //     nor mods_rolled_back — the rejection is captured solely by `rejected`.
    //   * A mod with zero rejections is counted in mods_ok (even if every op
    //     was skipped as not-yet-supported).
    ApplyResult ApplyMod(red4ext_mac::TweakDB* db, const ModFile& mod);

}
