#pragma once
#include <filesystem>
#include <memory>

namespace tweakxl_mac {
    // Opaque wrapper around psiberx's Red::TweakSource — we'll define an Op AST
    // adapter in P1.10. For now we only expose: parse → return a shared_ptr to a
    // handle, OR nullptr on parse error. Errors are logged (stderr + the mac log).
    //
    // The handle is kept opaque here so this public header does not leak the
    // psiberx/PEGTL/Core-stub dependency chain (Source.hpp requires the
    // TWEAKXL_MAC_OFFLINE build config). The applicator (P1.10) reaches the
    // underlying Red::TweakSource via parsers/TweakInternal.hpp.
    struct TweakSourceHandle;

    std::shared_ptr<TweakSourceHandle> ParseTweakFile(const std::filesystem::path& path);
}
