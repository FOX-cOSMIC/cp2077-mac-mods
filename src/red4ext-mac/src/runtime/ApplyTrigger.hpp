#pragma once
#include <cstdint>
#include <functional>
namespace red4ext_mac {
    struct TweakDB; // forward

    // Callback signature for "DB is populated and stable, apply mods now."
    // Called exactly once per process lifetime. Receives the live TweakDB*.
    // Callbacks run on the polling thread, not the main game thread.
    // Must be non-blocking — long work should be queued.
    using ApplyCallback = std::function<void(TweakDB* db)>;

    // Register a callback. Order is unspecified but stable. Safe to call
    // before EnsureStarted(). Multiple registrations supported.
    void RegisterApplyCallback(ApplyCallback cb);

    // Idempotent: start the background polling thread. Safe to call multiple
    // times. Call from dylib constructor (Loader.cpp).
    void EnsureStarted();

    // For tests / debugging: how many polls have happened so far.
    uint64_t GetPollCount();

    // For tests / debugging: has the callback batch fired? Once true, stays true.
    bool HasFired();
}
