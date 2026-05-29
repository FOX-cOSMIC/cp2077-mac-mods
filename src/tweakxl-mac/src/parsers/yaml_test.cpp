// Standalone unit tests for the YAML parser (P1.8). On-host, no game needed.
//
// Writes synthetic YAML to a temp dir and asserts op counts / kinds / values.
// Exit 0 = all pass; non-zero = first failure (with a message).

#include "YAML.hpp"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

using namespace tweakxl_mac;

namespace {
int g_failures = 0;

#define CHECK(cond, msg)                                                        \
    do {                                                                        \
        if (!(cond)) { std::printf("  FAIL: %s\n", (msg)); ++g_failures; }      \
        else         { std::printf("  ok:   %s\n", (msg)); }                    \
    } while (0)

std::filesystem::path WriteTemp(const std::string& name, const std::string& body) {
    auto p = std::filesystem::temp_directory_path() / ("pw_yaml_" + name);
    std::ofstream(p) << body;
    return p;
}

// Count ops of a given kind.
int CountKind(const ModFile& m, OpKind k) {
    int n = 0;
    for (const auto& op : m.ops) if (op.kind == k) ++n;
    return n;
}
const Operation* FindByName(const ModFile& m, const std::string& name) {
    for (const auto& op : m.ops) if (op.targetName == name) return &op;
    return nullptr;
}
} // namespace

int main() {
    // ── 1. Flat scalar assignments + type inference ──────────────────────────
    std::printf("[1] scalar assigns + inference\n");
    {
        auto p = WriteTemp("scalars.yaml",
            "Player.maxHealth: 300\n"
            "Vehicle.boost: 5.0\n"
            "Player.godMode: true\n"
            "Player.name: \"V\"\n");
        auto m = ParseYamlFile(p);
        CHECK(m != nullptr, "parses");
        if (m) {
            CHECK(m->ops.size() == 4, "4 ops");
            const auto* hp = FindByName(*m, "Player.maxHealth");
            CHECK(hp && hp->kind == OpKind::Assign && hp->value.type == ValueType::Int32 && hp->value.i == 300,
                  "maxHealth = Int32 300");
            const auto* bo = FindByName(*m, "Vehicle.boost");
            CHECK(bo && bo->value.type == ValueType::Float, "boost = Float");
            const auto* gm = FindByName(*m, "Player.godMode");
            CHECK(gm && gm->value.type == ValueType::Bool && gm->value.b, "godMode = Bool true");
            const auto* nm = FindByName(*m, "Player.name");
            CHECK(nm && nm->value.type == ValueType::String && nm->value.s == "V", "name = String \"V\" (quoted)");
            // TweakDBID hash is the standard CRC32 of the full name.
            CHECK(hp && hp->target.nameLength == 16, "maxHealth nameLength = 16");
        }
    }

    // ── 2. Record block with $base → CloneRecord + property assigns ──────────
    std::printf("[2] record clone ($base)\n");
    {
        auto p = WriteTemp("clone.yaml",
            "Items.MyGun:\n"
            "  $base: Items.Preset_Ajax_Default\n"
            "  damageMin: 80\n"
            "  damageMax: 120\n");
        auto m = ParseYamlFile(p);
        CHECK(m != nullptr, "parses");
        if (m) {
            CHECK(CountKind(*m, OpKind::CloneRecord) == 1, "1 CloneRecord");
            const auto* clone = FindByName(*m, "Items.MyGun");
            CHECK(clone && clone->kind == OpKind::CloneRecord && clone->baseRecord == "Items.Preset_Ajax_Default",
                  "clone base = Items.Preset_Ajax_Default");
            CHECK(FindByName(*m, "Items.MyGun.damageMin") != nullptr, "damageMin assign composed");
            CHECK(FindByName(*m, "Items.MyGun.damageMax") != nullptr, "damageMax assign composed");
            CHECK(m->ops.size() == 3, "3 ops total (clone + 2 props)");
        }
    }

    // ── 3. Array mutations (!append + !remove) + RemoveAll ───────────────────
    std::printf("[3] array mutation tags\n");
    {
        auto p = WriteTemp("mutations.yaml",
            "Items.Preset_Ajax_Default.tags:\n"
            "  - !append Tier5\n"
            "  - !append-once Iconic\n"
            "  - !remove Tier4\n");
        auto m = ParseYamlFile(p);
        CHECK(m != nullptr, "parses");
        if (m) {
            CHECK(m->ops.size() == 3, "3 mutation ops");
            CHECK(CountKind(*m, OpKind::Append) == 1, "1 Append");
            CHECK(CountKind(*m, OpKind::AppendOnce) == 1, "1 AppendOnce");
            CHECK(CountKind(*m, OpKind::Remove) == 1, "1 Remove");
            // Every mutation targets the same flat.
            bool sameTarget = true;
            for (const auto& op : m->ops) if (op.targetName != "Items.Preset_Ajax_Default.tags") sameTarget = false;
            CHECK(sameTarget, "all target the tags flat");
        }
    }

    // ── 4. remove-all + append-from + mixed warning ──────────────────────────
    std::printf("[4] remove-all / append-from / mixed\n");
    {
        auto p = WriteTemp("mut2.yaml",
            "A.list:\n"
            "  - !remove-all ~\n"
            "  - !append-from B.otherList\n"
            "Mixed.flat:\n"
            "  - !append X\n"
            "  - plainItemIgnored\n");
        auto m = ParseYamlFile(p);
        CHECK(m != nullptr, "parses");
        if (m) {
            CHECK(CountKind(*m, OpKind::RemoveAll) == 1, "1 RemoveAll");
            CHECK(CountKind(*m, OpKind::AppendFrom) == 1, "1 AppendFrom");
            const auto* af = FindByName(*m, "A.list");
            // AppendFrom op carries the source id.
            bool foundFrom = false;
            for (const auto& op : m->ops)
                if (op.kind == OpKind::AppendFrom && op.value.type == ValueType::TweakDBID
                    && op.value.s == "B.otherList") foundFrom = true;
            CHECK(foundFrom, "AppendFrom source = B.otherList");
            (void)af;
            bool mixedWarned = false;
            for (const auto& w : m->warnings)
                if (w.find("Mixed definition") != std::string::npos) mixedWarned = true;
            CHECK(mixedWarned, "mixed mutation/assignment warns");
        }
    }

    // ── 5. $type + $value flat; $game gate preserved ─────────────────────────
    std::printf("[5] $type/$value + $game gate\n");
    {
        auto p = WriteTemp("typed.yaml",
            "$game: \">=2.1.0\"\n"
            "NPCs.gangDensityFactor:\n"
            "  $type: Float\n"
            "  $value: 2.5\n");
        auto m = ParseYamlFile(p);
        CHECK(m != nullptr, "parses");
        if (m) {
            CHECK(m->gated && m->gameVersionGate == ">=2.1.0", "$game gate preserved");
            const auto* gd = FindByName(*m, "NPCs.gangDensityFactor");
            CHECK(gd && gd->kind == OpKind::Assign && gd->value.type == ValueType::Float && gd->value.f == 2.5f,
                  "gangDensityFactor = Float 2.5 via $type/$value");
            CHECK(m->ops.size() == 1, "1 op (gate is not an op)");
        }
    }

    // ── 6. Unknown tag → warning, element skipped, others survive ────────────
    std::printf("[6] unknown mutation tag\n");
    {
        auto p = WriteTemp("badtag.yaml",
            "A.list:\n"
            "  - !append Good\n"
            "  - !frobnicate Bad\n");
        auto m = ParseYamlFile(p);
        CHECK(m != nullptr, "parses");
        if (m) {
            CHECK(CountKind(*m, OpKind::Append) == 1, "good !append survives");
            bool warned = false;
            for (const auto& w : m->warnings)
                if (w.find("Invalid action") != std::string::npos) warned = true;
            CHECK(warned, "unknown tag warns");
        }
    }

    // ── 7. Malformed top-level (not a map) → nullptr ─────────────────────────
    std::printf("[7] malformed top-level\n");
    {
        auto p = WriteTemp("bad_toplevel.yaml", "- just\n- a\n- list\n");
        auto m = ParseYamlFile(p);
        CHECK(m == nullptr, "top-level sequence rejected (nullptr)");
    }

    // ── 8. Struct flat inference (Vector3) ───────────────────────────────────
    std::printf("[8] struct inference\n");
    {
        auto p = WriteTemp("struct.yaml",
            "Some.position:\n"
            "  x: 1.0\n"
            "  y: 2.0\n"
            "  z: 3.0\n");
        auto m = ParseYamlFile(p);
        CHECK(m != nullptr, "parses");
        if (m) {
            const auto* pos = FindByName(*m, "Some.position");
            CHECK(pos && pos->value.type == ValueType::Vector3 && pos->value.components.size() == 3
                  && pos->value.components[2] == 3.0f, "position = Vector3(1,2,3)");
        }
    }

    std::printf("\n%s (%d failure%s)\n", g_failures == 0 ? "PASS" : "FAIL",
                g_failures, g_failures == 1 ? "" : "s");
    return g_failures == 0 ? 0 : 1;
}
