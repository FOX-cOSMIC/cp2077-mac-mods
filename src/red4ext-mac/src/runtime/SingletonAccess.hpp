#pragma once
#include <cstdint>
namespace red4ext_mac {
    // Forward decl — actual TweakDB struct definition lives in TweakDB.hpp (P1.4).
    // SingletonAccess only deals with the raw pointer.
    struct TweakDB;

    // Returns the live TweakDB singleton, or nullptr if not yet populated.
    // Safe to call before the game has initialized TweakDB — returns nullptr.
    // Safe to call before main image is loaded (Symbols not ready) — returns nullptr.
    // Uses mach_vm_read_overwrite under the hood — never crashes on bad pointers.
    TweakDB* GetTweakDB();

    // Same as GetTweakDB() but bypasses the cached state; always re-reads
    // from the global. Useful for polling-style apply-trigger (P1.3).
    TweakDB* GetTweakDBUncached();

    // Returns the static address of the global pointer (constant; useful for tests).
    static constexpr uintptr_t kStaticGlobalPtrAddr = 0x1080c92d0;
}
