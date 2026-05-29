#pragma once
#include <cstdint>
namespace red4ext_mac {
    // Returns true once the main Cyberpunk2077 image has been observed.
    bool IsMainImageLoaded();
    // 0 until main image loads; thereafter the live image base.
    uintptr_t GetImageBase();
    // 0 until main image loads; thereafter (image_base - 0x100000000).
    uintptr_t GetSlide();
    // Convert a static address (Ghidra-side) to runtime address. Returns 0
    // if main image hasn't loaded yet.
    uintptr_t StaticToRuntime(uintptr_t static_addr);
}
