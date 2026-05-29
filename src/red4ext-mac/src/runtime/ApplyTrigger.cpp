// ApplyTrigger.cpp — P1.3 apply-trigger via singleton polling.
//
// Per F-018, the TweakDB initial-load orchestrator (FUN_102b75744) populates
// the singleton once during engine init; on its return the DB is fully built.
// We can't hook that return (no vtable F-016, no fn-ptr dispatch F-017, intra-
// image direct bl so GOT doesn't apply, __TEXT immutable FA-001), so we POLL
// the singleton (F-018 recommended mechanism #1) and fire registered callbacks
// once the records map count is non-zero and stable.
//
// Thread model: one detached background thread runs PollingLoop(). It sleeps
// 50ms between polls, reads the live records.count (TweakDB+0x90, F-012) via
// mach_vm_read_overwrite, and fires callbacks after the count has been the same
// non-zero value for 3 consecutive polls (~150ms) — guarding against firing
// mid-populate. After firing it EXITS (no perpetual CPU burn). A 5-minute
// safety bail-out prevents an infinite loop if the singleton never appears.
//
// All game-memory reads go through mach_vm_read_overwrite (never crash).
// No __TEXT writes, no inline hooks (FA-001).

#include "ApplyTrigger.hpp"
#include "SingletonAccess.hpp"
#include "TweakDB.hpp"   // TweakDB layout — records.count offset (F-012/F-015)

#include <atomic>
#include <mutex>
#include <vector>
#include <thread>
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstddef>
#include <mach/mach.h>
#include <mach/mach_vm.h>

namespace red4ext_mac {
namespace {

// ── Logging (same sink as Loader: /tmp file + stderr) ────────────────────────
constexpr const char* kLogPath = "/tmp/red4ext-mac.log";
void log_line(const char* fmt, ...) {
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (FILE* f = std::fopen(kLogPath, "a")) {
        std::fputs(buf, f);
        std::fputc('\n', f);
        std::fclose(f);
    }
    std::fprintf(stderr, "%s\n", buf);
    std::fflush(stderr);
}

// records.count lives at TweakDB+0x90 (records map @ +0x88, count @ +0x08).
constexpr uintptr_t kRecordsCountOffset =
    offsetof(TweakDB, hashMapB_records) + offsetof(HashMap, count);
static_assert(kRecordsCountOffset == 0x90, "F-012: records.count must be at +0x90");

// ── Injectable seams (defaults = real game-memory access) ────────────────────
// Tests override these to drive the loop deterministically without the game.
TweakDB* DefaultProvider() { return GetTweakDBUncached(); }

bool DefaultCountReader(TweakDB* db, uint32_t* out) {
    if (!db) return false;
    uintptr_t addr = reinterpret_cast<uintptr_t>(db) + kRecordsCountOffset;
    uint32_t value = 0;
    mach_vm_size_t got = 0;
    kern_return_t kr = mach_vm_read_overwrite(
        mach_task_self(),
        (mach_vm_address_t)addr,
        (mach_vm_size_t)sizeof(value),
        (mach_vm_address_t)&value,
        &got);
    if (kr != KERN_SUCCESS || got != sizeof(value)) return false;
    *out = value;
    return true;
}

TweakDB* (*g_provider)()                         = &DefaultProvider;
bool      (*g_countReader)(TweakDB*, uint32_t*)  = &DefaultCountReader;

// ── Tunables (tests shrink these for fast, deterministic runs) ───────────────
uint32_t g_pollIntervalMs      = 50;
uint32_t g_stablePollsRequired = 3;             // ~150ms of count stability
uint64_t g_timeoutMs           = 5ull * 60 * 1000; // 5-minute safety net

// ── State ────────────────────────────────────────────────────────────────────
// Atomics + once_flag are constant-initialized (no dynamic init), so they are
// live before any constructor runs — safe to touch from Loader's
// constructor(101). They are NOT subject to the SIOF that bit the callback list.
std::atomic<uint64_t> g_pollCount{0};
std::atomic<bool>     g_fired{false};
std::once_flag        g_startOnce;

// Construct-on-first-use (P1.3c fix for the P1.3b SIOF): the callback registry
// and its mutex are non-trivial std::-types whose namespace-scope dynamic init
// ran AFTER Loader's constructor(101) called RegisterApplyCallback — so the
// pushed callback was wiped when the vector's real constructor ran later.
// Function-local statics are initialized on first call (thread-safe guard),
// guaranteeing the registry exists before first use regardless of constructor
// ordering. Both accessors MUST be used everywhere; never reintroduce a
// namespace-scope std:: container here.
std::vector<ApplyCallback>& callbacks() {
    static std::vector<ApplyCallback> v;
    return v;
}
std::mutex& cbMutex() {
    static std::mutex m;
    return m;
}

// Snapshot + invoke the callback batch exactly once.
void FireCallbacks(TweakDB* db, uint64_t pollNo) {
    log_line("[apply-trigger] firing callbacks (db=%p, poll #%llu)",
             (void*)db, (unsigned long long)pollNo);

    std::vector<ApplyCallback> batch;
    {
        std::lock_guard<std::mutex> lock(cbMutex());
        batch = callbacks(); // copy so callbacks may register more without deadlock
    }
    // Kept from P1.3b: cheap, high-value canary. If the SIOF ever returns this
    // prints size=0 and the cause is obvious from the log alone.
    log_line("[apply-trigger] callback batch snapshot: size=%zu fired=%d",
             batch.size(), g_fired.load());
    for (auto& cb : batch) {
        if (cb) cb(db);
    }
    g_fired.store(true, std::memory_order_release);
}

// The polling loop. Runs on the background thread in production; the test seam
// runs it synchronously on the caller thread. Identical code either way.
void PollingLoop() {
    using clock = std::chrono::steady_clock;
    const auto start = clock::now();

    bool     loggedNonNull   = false;
    uint32_t lastCount       = 0;
    uint32_t consecutiveSame = 0;

    for (;;) {
        std::this_thread::sleep_for(std::chrono::milliseconds(g_pollIntervalMs));
        const uint64_t pollNo = g_pollCount.fetch_add(1, std::memory_order_relaxed) + 1;

        TweakDB* db = g_provider();
        if (!db) {
            // Safety bail-out: singleton never appeared.
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                clock::now() - start).count();
            if ((uint64_t)elapsed >= g_timeoutMs) {
                log_line("[apply-trigger] timeout — giving up after %llu ms (%llu polls), "
                         "singleton never became non-null",
                         (unsigned long long)elapsed, (unsigned long long)pollNo);
                return;
            }
            continue;
        }

        if (!loggedNonNull) {
            log_line("[apply-trigger] singleton non-null at poll %llu (TweakDB=%p)",
                     (unsigned long long)pollNo, (void*)db);
            loggedNonNull = true;
        }

        uint32_t count = 0;
        if (!g_countReader(db, &count)) {
            // Couldn't read the count this round; don't count toward stability.
            consecutiveSame = 0;
            continue;
        }

        if (count == 0) {
            // DB present but records not populated yet — keep waiting.
            consecutiveSame = 0;
            lastCount = 0;
            continue;
        }

        if (count == lastCount) {
            ++consecutiveSame;
        } else {
            lastCount = count;
            consecutiveSame = 1;
        }

        if (consecutiveSame >= g_stablePollsRequired) {
            log_line("[apply-trigger] count stable at %u after %u consecutive polls",
                     count, consecutiveSame);
            FireCallbacks(db, pollNo);
            return; // fired once; stop polling
        }

        // Elapsed safety net also applies once non-null (e.g. count never settles).
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            clock::now() - start).count();
        if ((uint64_t)elapsed >= g_timeoutMs) {
            log_line("[apply-trigger] timeout — giving up after %llu ms (%llu polls), "
                     "records.count never stabilized (last=%u)",
                     (unsigned long long)elapsed, (unsigned long long)pollNo, count);
            return;
        }
    }
}

} // namespace

// ── Public API ────────────────────────────────────────────────────────────────

void RegisterApplyCallback(ApplyCallback cb) {
    std::lock_guard<std::mutex> lock(cbMutex());
    callbacks().push_back(std::move(cb));
}

void EnsureStarted() {
    std::call_once(g_startOnce, [] {
        log_line("[apply-trigger] polling thread started "
                 "(interval=%ums, stable-threshold=%u polls, timeout=%llums)",
                 g_pollIntervalMs, g_stablePollsRequired,
                 (unsigned long long)g_timeoutMs);
        std::thread(PollingLoop).detach();
    });
}

uint64_t GetPollCount() { return g_pollCount.load(std::memory_order_relaxed); }
bool     HasFired()     { return g_fired.load(std::memory_order_acquire); }

// ── Test-only seam (declared, not in the public header; forward-declared by
//    apply_trigger_test.cpp). Lets the on-host test drive the real loop code
//    with fake memory and fast timings, fully deterministically. ─────────────
namespace test {

void SetSingletonProvider(TweakDB* (*fn)()) {
    g_provider = fn ? fn : &DefaultProvider;
}
void SetCountReader(bool (*fn)(TweakDB*, uint32_t*)) {
    g_countReader = fn ? fn : &DefaultCountReader;
}
void SetTimings(uint32_t pollMs, uint32_t stableN, uint64_t timeoutMs) {
    g_pollIntervalMs      = pollMs;
    g_stablePollsRequired = stableN ? stableN : 1;
    g_timeoutMs           = timeoutMs;
}
void ResetApplyTrigger() {
    std::lock_guard<std::mutex> lock(cbMutex());
    callbacks().clear();
    g_pollCount.store(0, std::memory_order_relaxed);
    g_fired.store(false, std::memory_order_release);
    g_provider     = &DefaultProvider;
    g_countReader  = &DefaultCountReader;
    g_pollIntervalMs      = 50;
    g_stablePollsRequired = 3;
    g_timeoutMs           = 5ull * 60 * 1000;
}
// Runs the loop on the caller's thread (no detach) so the test is deterministic.
void RunPollingLoopSync() { PollingLoop(); }

} // namespace test

} // namespace red4ext_mac
