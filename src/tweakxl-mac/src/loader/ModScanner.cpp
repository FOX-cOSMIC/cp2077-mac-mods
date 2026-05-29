#include "ModScanner.hpp"

#include <algorithm>
#include <cctype>
#include <string>

// Mod-file discovery — a faithful macOS port of
// reference/windows-tweakxl/src/App/Tweaks/Declarative/TweakImporter.cpp::ImportTweaks().
//
// Compatibility contract (windows-tweakxl-api.md §1): for any real-world mods
// directory, the set of discovered files and their load order must match the
// Windows importer. Where this code and the Windows source disagree, this code
// is wrong.

namespace {
    // Windows: TweakImporter::IsFirstPriority / IsLastPriority test the FIRST
    // character of the *filename* (not the full path) against these marker sets.
    constexpr const char* kFirstPriorityMarkers = "_#$!";
    constexpr const char* kLastPriorityMarkers = "^";

    // 0 = first (loaded before all others), 1 = normal, 2 = last.
    int PriorityTier(const std::filesystem::path& path) {
        const std::string filename = path.filename().string();
        if (filename.empty()) {
            return 1; // defensive; regular files always have a non-empty name
        }
        const char first = filename.front();
        if (std::string(kFirstPriorityMarkers).find(first) != std::string::npos) {
            return 0;
        }
        if (std::string(kLastPriorityMarkers).find(first) != std::string::npos) {
            return 2;
        }
        return 1;
    }

    // Extension classification. Windows uses a case-sensitive `ext == L".yaml"`
    // comparison, but since the Windows filesystem is case-insensitive, real mods
    // ship with mixed-case extensions. Per windows-tweakxl-api.md §8.4 we normalize
    // the extension to lowercase before comparing so `.YAML`/`.Tweak` are recognized
    // on macOS's case-sensitive filesystem. This is a documented compatibility
    // widening, not a divergence — it accepts a strict superset of Windows.
    bool ClassifyExtension(const std::filesystem::path& path, tweakxl_mac::ModFileType& outType) {
        std::string ext = path.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (ext == ".yaml" || ext == ".yml") {
            outType = tweakxl_mac::ModFileType::Yaml;
            return true;
        }
        if (ext == ".tweak") {
            outType = tweakxl_mac::ModFileType::Tweak;
            return true;
        }
        return false; // any other extension is silently skipped
    }
}

std::vector<tweakxl_mac::DiscoveredMod>
tweakxl_mac::ScanModsDirectory(const std::filesystem::path& root) {
    std::vector<DiscoveredMod> discovered;

    std::error_code error;
    if (!std::filesystem::is_directory(root, error)) {
        return discovered; // not a directory / unreadable → empty, no throw
    }

    // Recursive walk with symlink following, matching the Windows iterator options.
    // We drive the iterator with the error_code increment overload so that an
    // unreadable entry mid-walk is silently skipped instead of throwing, per the
    // "silent skip on filesystem errors" requirement.
    std::error_code iterError;
    auto it = std::filesystem::recursive_directory_iterator(
        root, std::filesystem::directory_options::follow_directory_symlink, iterError);
    const auto end = std::filesystem::recursive_directory_iterator();

    for (; it != end; it.increment(iterError)) {
        if (iterError) {
            iterError.clear();
            continue; // skip the offending entry, keep walking
        }

        std::error_code entryError;
        if (!it->is_regular_file(entryError) || entryError) {
            continue;
        }

        const auto& path = it->path();
        ModFileType type;
        if (!ClassifyExtension(path, type)) {
            continue; // not a recognized mod extension
        }

        discovered.push_back(DiscoveredMod{path, type, PriorityTier(path)});
    }

    // Windows concatenates first-priority, then normal, then last-priority paths,
    // preserving filesystem-iteration order within each tier. A stable sort on the
    // tier reproduces this exactly: equal-tier elements keep their discovery order.
    std::stable_sort(discovered.begin(), discovered.end(),
                     [](const DiscoveredMod& a, const DiscoveredMod& b) {
                         return a.priority_tier < b.priority_tier;
                     });

    return discovered;
}
