// slide_test.cpp — standalone exerciser for the P1.1 Symbols API.
//
// TEMPORARY test target (not part of the shipped dylib). Run two ways:
//   1. Standalone: ./slide_test — its own image isn't "Cyberpunk2077", so the
//      API reports "main image not loaded". This verifies the API links,
//      is callable, and degrades safely (all getters return 0).
//   2. Injected via DYLD_INSERT_LIBRARIES into a process whose main binary is
//      named "Cyberpunk2077" — getters then return the live base/slide.
//
// The real injection smoke test is tools/test-slide-capture.sh.

#include "Symbols.hpp"
#include "SingletonAccess.hpp"
#include <cstdint>
#include <cstdio>

int main() {
    // F-011: global TweakDB* singleton lives at static 0x1080c92d0.
    constexpr uintptr_t kSingletonStatic = 0x1080c92d0ull;

    const bool      loaded = red4ext_mac::IsMainImageLoaded();
    const uintptr_t base   = red4ext_mac::GetImageBase();
    const uintptr_t slide  = red4ext_mac::GetSlide();
    const uintptr_t rt     = red4ext_mac::StaticToRuntime(kSingletonStatic);

    std::printf("[slide_test] IsMainImageLoaded = %s\n", loaded ? "true" : "false");
    std::printf("[slide_test] GetImageBase      = 0x%lx\n", (unsigned long)base);
    std::printf("[slide_test] GetSlide          = 0x%lx\n", (unsigned long)slide);
    std::printf("[slide_test] StaticToRuntime(0x%lx) = 0x%lx\n",
                (unsigned long)kSingletonStatic, (unsigned long)rt);

    // P1.2: exercise the singleton accessor once. Null is valid (no game DB in
    // this process); the real in-game validation is tools/test-singleton-access.sh.
    void* db = (void*)red4ext_mac::GetTweakDB();
    std::printf("[singleton-test] tweakdb=%p%s\n", db,
                db ? "" : " (null — expected; no TweakDB in this process)");

    if (!loaded) {
        std::printf("[slide_test] main image not loaded — expected when run "
                    "standalone (binary is not 'Cyberpunk2077'). API OK.\n");
        return 0;
    }

    // Sanity: base - slide must equal the canonical static base (F-004).
    if (base - slide != 0x100000000ull) {
        std::printf("[slide_test] FAIL: base-slide=0x%lx, expected 0x100000000\n",
                    (unsigned long)(base - slide));
        return 1;
    }
    std::printf("[slide_test] PASS: base-slide == 0x100000000\n");
    return 0;
}
