// Standalone exerciser for the mod scanner (P1.7).
//
//   scanner_test <directory>
//
// Scans <directory> recursively and prints one line per discovered mod:
//
//   <tier> <type> <path>
//
// where <tier> is 0/1/2 and <type> is YAML/TWEAK. Output order is the load
// order the importer would use. Used by reviewer + tester.

#include "ModScanner.hpp"

#include <cstdio>
#include <filesystem>

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <directory>\n", argv[0]);
        return 2;
    }

    const std::filesystem::path root = argv[1];
    const auto mods = tweakxl_mac::ScanModsDirectory(root);

    for (const auto& mod : mods) {
        const char* type = (mod.type == tweakxl_mac::ModFileType::Yaml) ? "YAML" : "TWEAK";
        std::printf("%d %s %s\n", mod.priority_tier, type, mod.path.string().c_str());
    }

    std::fprintf(stderr, "discovered %zu mod file(s) under %s\n", mods.size(), root.string().c_str());
    return 0;
}
