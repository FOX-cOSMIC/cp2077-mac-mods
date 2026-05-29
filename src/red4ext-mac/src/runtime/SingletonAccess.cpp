// SingletonAccess.cpp — P1.2 live TweakDB singleton accessor.
//
// Reads the heap TweakDB* from the __bss global at static 0x1080c92d0 (F-011)
// via the P1.1 slide capture. Read-only, no hook (F-014 target #1).
// Speculative read goes through mach_vm_read_overwrite (same pattern as the
// H-001 probe) so a bad/unmapped pointer yields nullptr instead of a crash.
//
// No mprotect, no __TEXT writes (FA-001).

#include "SingletonAccess.hpp"
#include "Symbols.hpp"

#include <atomic>
#include <mach/mach.h>
#include <mach/mach_vm.h>

namespace red4ext_mac {
namespace {

// Once a non-null singleton is observed, the game never relocates it for the
// life of the process, so caching is safe. atomic for cross-thread reads.
std::atomic<TweakDB*> g_cached{nullptr};

// Read the 8-byte pointer stored at the runtime address of the global.
// Returns nullptr if: main image not loaded yet (slide unknown), the read
// faults, or the stored pointer is null (DB not yet constructed by the game).
TweakDB* ReadGlobal() {
    const uintptr_t runtime_addr = StaticToRuntime(kStaticGlobalPtrAddr);
    if (runtime_addr == 0) return nullptr; // main image not loaded yet

    uintptr_t value = 0;
    mach_vm_size_t got = 0;
    kern_return_t kr = mach_vm_read_overwrite(
        mach_task_self(),
        (mach_vm_address_t)runtime_addr,
        (mach_vm_size_t)sizeof(value),
        (mach_vm_address_t)&value,
        &got);

    if (kr != KERN_SUCCESS || got != sizeof(value)) return nullptr;
    if (value == 0) return nullptr; // global zero-initialized; DB not built yet

    return reinterpret_cast<TweakDB*>(value);
}

} // namespace

TweakDB* GetTweakDBUncached() {
    return ReadGlobal();
}

TweakDB* GetTweakDB() {
    // Fast path: already captured. The pointer is stable once non-null.
    if (TweakDB* cached = g_cached.load(std::memory_order_acquire))
        return cached;

    TweakDB* live = ReadGlobal();
    if (live)
        g_cached.store(live, std::memory_order_release);
    return live;
}

} // namespace red4ext_mac
