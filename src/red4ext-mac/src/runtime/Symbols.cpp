// Symbols.cpp — P1.1 slide-capture primitive.
//
// Captures the ASLR slide of the main Cyberpunk2077 image at dylib load so
// every other runtime primitive can convert Ghidra-static addresses
// (F-004/F-007) to live addresses. Logic mirrors the dyld-callback +
// slide-extraction in src/probes/h001_probe.cpp, reworked for production:
// thread-safe, basename-exact image matching (no over-match on the
// Frameworks/*.dylib that live inside Cyberpunk2077.app — T-018 / F-010).
//
// runtime_addr = static_addr + slide ;  slide = image_base - 0x100000000
// The slide is per-launch; never cache a runtime address across launches.

#include "Symbols.hpp"

#include <atomic>
#include <mutex>
#include <cstring>
#include <mach-o/dyld.h>
#include <mach-o/loader.h>

namespace red4ext_mac {
namespace {

// Canonical arm64 Mach-O static load base (F-004).
constexpr uintptr_t kStaticBase = 0x100000000ull;

// The image-load callback may fire on a non-main thread; keep state atomic.
std::atomic<bool>      g_loaded{false};
std::atomic<uintptr_t> g_image_base{0};
std::atomic<uintptr_t> g_slide{0};

std::once_flag g_init_once;

// basename(path) — returns pointer to the component after the final '/'.
const char* basename_ptr(const char* path) {
    const char* slash = std::strrchr(path, '/');
    return slash ? slash + 1 : path;
}

void OnImageLoaded(const struct mach_header* mh, intptr_t /*s*/) {
    if (!mh || mh->magic != MH_MAGIC_64) return;
    if (g_loaded.load(std::memory_order_acquire)) return; // already captured

    // Resolve this image's path by matching the header pointer against dyld's
    // image table (the callback's own args don't carry the path).
    const char* path = nullptr;
    for (uint32_t i = 0, n = _dyld_image_count(); i < n; ++i) {
        if (_dyld_get_image_header(i) == mh) {
            path = _dyld_get_image_name(i);
            break;
        }
    }
    if (!path) return;

    // Match the MAIN binary only — exact basename "Cyberpunk2077".
    // Substring matching over-matches the Frameworks/*.dylib bundled inside
    // Cyberpunk2077.app/Contents/Frameworks/ (T-018 / F-010), so compare the
    // basename for exact equality instead.
    if (std::strcmp(basename_ptr(path), "Cyberpunk2077") != 0) return;

    const uintptr_t base  = reinterpret_cast<uintptr_t>(mh);
    const uintptr_t slide = base - kStaticBase;

    // Publish base+slide before flipping the loaded flag (release/acquire
    // pairing in the getters guarantees readers see consistent values).
    g_image_base.store(base, std::memory_order_relaxed);
    g_slide.store(slide, std::memory_order_relaxed);
    g_loaded.store(true, std::memory_order_release);
}

} // namespace

// Registers the dyld add-image callback exactly once. Safe to call from
// multiple constructors / threads. When called, dyld synchronously invokes
// the callback for every already-loaded image (the main binary is already
// mapped by the time an injected dylib initializes), so on return the slide
// is captured if the main image is present.
void EnsureInit() {
    std::call_once(g_init_once, [] {
        _dyld_register_func_for_add_image(&OnImageLoaded);
    });
}

bool IsMainImageLoaded() {
    return g_loaded.load(std::memory_order_acquire);
}

uintptr_t GetImageBase() {
    if (!g_loaded.load(std::memory_order_acquire)) return 0;
    return g_image_base.load(std::memory_order_relaxed);
}

uintptr_t GetSlide() {
    if (!g_loaded.load(std::memory_order_acquire)) return 0;
    return g_slide.load(std::memory_order_relaxed);
}

uintptr_t StaticToRuntime(uintptr_t static_addr) {
    if (!g_loaded.load(std::memory_order_acquire)) return 0;
    return static_addr + g_slide.load(std::memory_order_relaxed);
}

} // namespace red4ext_mac

// Self-initialize even if the loader stub never runs (e.g. when another
// translation unit links Symbols directly). Idempotent with EnsureInit().
__attribute__((constructor))
static void symbols_autoinit() {
    red4ext_mac::EnsureInit();
}
