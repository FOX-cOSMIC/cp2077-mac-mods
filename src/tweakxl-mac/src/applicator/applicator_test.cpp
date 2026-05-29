// Standalone unit test for the P1.10a applicator. On-host, no game needed.
//
// We can't exercise a *successful* apply on host (that needs the live game's
// flat buffer + a confirmed FlatLayout, which only exists in-game — see
// VerifyFlatEntry). What we CAN validate without the game is the branch logic:
// which ops skip, which reject, and whether the atomic-rollback path fires.
//
// Trick: ReadFlat/WriteFlat refuse to touch memory until GetFlatLayout() is
// confirmed, and on host it never is. So against a deliberately-invalid (zeroed)
// TweakDB*, ReadFlat returns nullopt for every scalar Assign → that op rejects,
// and no real memory is read. That lets us assert the counters exactly.
//
// Exit 0 = all pass; non-zero = a failure (with a message).

#include "applicator/Applicator.hpp"
#include "model/Operation.hpp"
#include "runtime/TweakDB.hpp"   // sizeof(red4ext_mac::TweakDB) for the fake db

#include <cstdio>
#include <cstdlib>
#include <cstring>

using namespace tweakxl_mac;

namespace {
int g_failures = 0;

#define CHECK(cond, msg)                                                        \
    do {                                                                        \
        if (!(cond)) { std::printf("  FAIL: %s\n", (msg)); ++g_failures; }      \
        else         { std::printf("  ok:   %s\n", (msg)); }                    \
    } while (0)

// A scalar Assign op with a plausible (non-zero) TweakDBID.
Operation MakeAssign(const std::string& name, ValueType vt) {
    Operation op;
    op.kind = OpKind::Assign;
    op.targetName = name;
    op.target.nameHash  = 0xDEADBEEF;       // any non-zero hash; lookup will miss
    op.target.nameLength = static_cast<uint8_t>(name.size());
    op.value.type = vt;
    switch (vt) {
        case ValueType::Int32:  op.value.i = 300;   break;
        case ValueType::Float:  op.value.f = 5.0f;  break;
        case ValueType::Bool:   op.value.b = true;  break;
        case ValueType::String: op.value.s = "V";   break;
        default: break;
    }
    return op;
}

Operation MakeAppend(const std::string& name) {
    Operation op;
    op.kind = OpKind::Append;
    op.targetName = name;
    op.target.nameHash = 0x12345678;
    op.value.type = ValueType::Array;
    return op;
}

// A zero-initialized 0x168-byte TweakDB — structurally a TweakDB but never
// populated, so Schema's primitives reject it (and, with layout unconfirmed,
// never dereference into it). Heap-allocated so the address is a real pointer.
red4ext_mac::TweakDB* MakeFakeDb() {
    auto* db = static_cast<red4ext_mac::TweakDB*>(
        std::calloc(1, sizeof(red4ext_mac::TweakDB)));
    return db;
}
} // namespace

int main() {
    // ── 1. Branch logic: scalar reject + two skips, no rollback ──────────────
    // Ops: [Int32 Assign] (flat-not-found -> reject),
    //      [Append]        (kind != Assign -> skip, P1.10b),
    //      [String Assign] (non-scalar value -> skip, P1.10b).
    // The Int32 Assign rejects (ReadFlat nullopt) but never wrote anything, so
    // there is nothing to roll back: mods_rolled_back stays 0.
    std::printf("[1] scalar reject + 2 skips, empty rollback\n");
    {
        red4ext_mac::TweakDB* db = MakeFakeDb();
        CHECK(db != nullptr, "fake db allocated");

        ModFile mod;
        mod.path = "test_mixed.yaml";
        mod.ops.push_back(MakeAssign("Player.maxHealth", ValueType::Int32));
        mod.ops.push_back(MakeAppend("Player.tags"));
        mod.ops.push_back(MakeAssign("Player.name", ValueType::String));

        ApplyResult r = ApplyMod(db, mod);
        CHECK(r.applied  == 0, "applied == 0");
        CHECK(r.skipped  == 2, "skipped == 2 (Append + String Assign)");
        CHECK(r.rejected == 1, "rejected == 1 (Int32 Assign, flat not found)");
        CHECK(r.mods_rolled_back == 0, "mods_rolled_back == 0 (nothing was written)");
        CHECK(r.mods_ok  == 0, "mods_ok == 0 (mod had a rejection)");

        std::free(db);
    }

    // ── 2. All-skip mod counts as mods_ok (no rejection occurred) ────────────
    // A mod whose only op is a deferred kind has zero rejections, so it is a
    // clean (if no-op) apply: mods_ok == 1.
    std::printf("[2] all-skip mod -> mods_ok\n");
    {
        red4ext_mac::TweakDB* db = MakeFakeDb();

        ModFile mod;
        mod.path = "test_appendonly.yaml";
        mod.ops.push_back(MakeAppend("Player.tags"));

        ApplyResult r = ApplyMod(db, mod);
        CHECK(r.applied  == 0, "applied == 0");
        CHECK(r.skipped  == 1, "skipped == 1 (Append)");
        CHECK(r.rejected == 0, "rejected == 0");
        CHECK(r.mods_ok  == 1, "mods_ok == 1 (no rejection -> clean)");
        CHECK(r.mods_rolled_back == 0, "mods_rolled_back == 0");

        std::free(db);
    }

    // ── 3. Float + Bool Assigns are recognised as scalar (then reject on miss)
    // Confirms the scalar classifier accepts Float/Bool (they reach ReadFlat and
    // reject as not-found, NOT skip-as-non-scalar).
    std::printf("[3] Float/Bool classified scalar (reject, not skip)\n");
    {
        red4ext_mac::TweakDB* db = MakeFakeDb();

        ModFile mod;
        mod.path = "test_scalars.yaml";
        mod.ops.push_back(MakeAssign("Vehicle.boost", ValueType::Float));
        mod.ops.push_back(MakeAssign("Player.godMode", ValueType::Bool));

        ApplyResult r = ApplyMod(db, mod);
        CHECK(r.skipped  == 0, "skipped == 0 (both are scalar)");
        CHECK(r.rejected == 2, "rejected == 2 (both flats not found)");
        CHECK(r.mods_rolled_back == 0, "mods_rolled_back == 0 (nothing written)");

        std::free(db);
    }

    std::printf("\n%s (%d failure%s)\n",
                g_failures == 0 ? "ALL PASS" : "FAILURES",
                g_failures, g_failures == 1 ? "" : "s");
    return g_failures == 0 ? 0 : 1;
}
