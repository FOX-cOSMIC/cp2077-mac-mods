#pragma once
#include <filesystem>
#include <memory>
#include "../model/Operation.hpp"

namespace tweakxl_mac {
    // Parse a YAML mod file into a ModFile of Operations the applicator (P1.10)
    // can iterate. Returns:
    //   * a populated ModFile (ops may be empty; per-entry issues land in
    //     ModFile::warnings) on success, OR
    //   * nullptr if the file is structurally unusable — top-level is not a
    //     mapping, yaml-cpp throws, or the file can't be read. Fatal errors are
    //     logged (stderr + /tmp/red4ext-mac.log).
    //
    // $game / $dlc gates: the gate strings are preserved on the ModFile (gated=true)
    // but NOT evaluated here (v1.0 — P1.10/P1.11 may evaluate). A gated file still
    // returns its ops; the gate is advisory metadata.
    std::unique_ptr<ModFile> ParseYamlFile(const std::filesystem::path& path);
}
