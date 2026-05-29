#pragma once
#include <filesystem>
#include <string>
#include <vector>

namespace tweakxl_mac {
    enum class ModFileType { Yaml, Tweak };

    struct DiscoveredMod {
        std::filesystem::path path;
        ModFileType type;
        int priority_tier;   // 0=first(_#$!), 1=normal, 2=last(^)
    };

    // Recursively walks the given directory. Returns mods sorted by
    // (priority_tier ASC, filesystem-iteration order) — matches the
    // Windows TweakXL behavior per windows-tweakxl-api.md §1.
    std::vector<DiscoveredMod> ScanModsDirectory(const std::filesystem::path& root);
}
