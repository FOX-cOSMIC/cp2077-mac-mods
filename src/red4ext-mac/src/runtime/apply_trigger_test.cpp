// apply_trigger_test.cpp — P1.3 on-host test for the apply-trigger loop.
//
// Drives the REAL PollingLoop() (via the test seam) with a fake singleton
// provider and fake count reader, so no game is needed. Verifies:
//   1. The callback fires exactly once, with the correct TweakDB* pointer.
//   2. HasFired() transitions false → true.
//   3. The loop waits for count STABILITY (doesn't fire mid-populate).
//   4. Timeout path: no non-null singleton → loop bails, callback never fires.
//
// The fake pointer is never dereferenced — only its value is checked — so it
// is safe to use an arbitrary non-null address.

#include "ApplyTrigger.hpp"
#include <cstdint>
#include <cstdio>

namespace red4ext_mac {
struct TweakDB; // opaque here — we only pass the pointer around
namespace test {
    void SetSingletonProvider(TweakDB* (*fn)());
    void SetCountReader(bool (*fn)(TweakDB*, uint32_t*));
    void SetTimings(uint32_t pollMs, uint32_t stableN, uint64_t timeoutMs);
    void ResetApplyTrigger();
    void RunPollingLoopSync();
}
}

using red4ext_mac::TweakDB;

// ── Shared fake state (function-pointer seams can't capture, so use statics) ──
namespace {

TweakDB* const kFakeDB = reinterpret_cast<TweakDB*>(0x1ABCDEF000ull);

int      g_nullsBeforeNonNull = 0;  // provider returns null this many times first
int      g_providerCalls      = 0;

// Scripted count sequence: index clamps at the last value.
const uint32_t* g_countSeq    = nullptr;
int             g_countSeqLen = 0;
int             g_countCalls  = 0;

// Callback bookkeeping.
int      g_cbFireCount = 0;
TweakDB* g_cbSeenDB    = nullptr;

TweakDB* FakeProvider() {
    int n = g_providerCalls++;
    return (n < g_nullsBeforeNonNull) ? nullptr : kFakeDB;
}

bool FakeCountReader(TweakDB* /*db*/, uint32_t* out) {
    int i = g_countCalls++;
    if (i >= g_countSeqLen) i = g_countSeqLen - 1; // clamp at last
    *out = g_countSeq[i];
    return true;
}

bool AlwaysNullCountReader(TweakDB*, uint32_t*) { return false; }

// ── SIOF regression guard (Test 0) ────────────────────────────────────────────
// A callback registered from a constructor(101) — exactly the situation that
// produced batch.size()==0 in P1.3b. With the construct-on-first-use fix it must
// survive static init and fire. Simple seams that ignore the count sequence.
bool     g_ctorCbFired = false;
TweakDB* CtorTestProvider() { return kFakeDB; }
bool     StableCountReader(TweakDB*, uint32_t* out) { *out = 500; return true; }

void ResetFakes() {
    g_providerCalls = 0;
    g_countCalls    = 0;
    g_cbFireCount   = 0;
    g_cbSeenDB      = nullptr;
}

int g_failures = 0;
void check(bool cond, const char* what) {
    std::printf("  [%s] %s\n", cond ? "PASS" : "FAIL", what);
    if (!cond) ++g_failures;
}

} // namespace

// Registered BEFORE main(), at the same constructor priority Loader uses. If the
// callback registry weren't construct-on-first-use, this push would be wiped by
// the registry's own (later) dynamic init — the P1.3b bug. Kept outside the
// anonymous namespace so the attribute applies to an external-linkage symbol.
__attribute__((constructor(101)))
static void register_callback_from_constructor() {
    red4ext_mac::RegisterApplyCallback([](TweakDB*) { g_ctorCbFired = true; });
}

static void TestEarlyRegistrationSurvives() {
    std::printf("Test 0: callback registered from constructor(101) survives to fire (SIOF guard)\n");
    // Do NOT ResetApplyTrigger here — that would clear the ctor-registered
    // callback we are specifically trying to observe.
    red4ext_mac::test::SetSingletonProvider(&CtorTestProvider);
    red4ext_mac::test::SetCountReader(&StableCountReader);
    red4ext_mac::test::SetTimings(/*pollMs*/0, /*stableN*/3, /*timeoutMs*/5000);

    check(!g_ctorCbFired, "ctor-registered callback has not fired yet");
    red4ext_mac::test::RunPollingLoopSync();
    check(g_ctorCbFired, "ctor-registered callback fired (registry survived static init)");
}

static void TestFiresOnceWhenStable() {
    std::printf("Test 1: fires once after stable non-zero count\n");
    red4ext_mac::test::ResetApplyTrigger();
    ResetFakes();

    g_nullsBeforeNonNull = 4;                 // 4 null polls, then non-null
    static const uint32_t seq[] = {1000};     // immediately stable, non-zero
    g_countSeq = seq; g_countSeqLen = 1;

    red4ext_mac::test::SetSingletonProvider(&FakeProvider);
    red4ext_mac::test::SetCountReader(&FakeCountReader);
    red4ext_mac::test::SetTimings(/*pollMs*/0, /*stableN*/3, /*timeoutMs*/5000);

    red4ext_mac::RegisterApplyCallback([](TweakDB* db) {
        ++g_cbFireCount;
        g_cbSeenDB = db;
    });

    check(!red4ext_mac::HasFired(), "HasFired() is false before the loop runs");
    red4ext_mac::test::RunPollingLoopSync();

    check(g_cbFireCount == 1, "callback fired exactly once");
    check(g_cbSeenDB == kFakeDB, "callback received the correct TweakDB*");
    check(red4ext_mac::HasFired(), "HasFired() is true after firing");
    // 4 null polls + need 3 consecutive stable reads → fire on the 7th poll.
    check(red4ext_mac::GetPollCount() == 7, "fired on the expected poll (7)");
}

static void TestWaitsForStability() {
    std::printf("Test 2: does NOT fire mid-populate (count changing / zero)\n");
    red4ext_mac::test::ResetApplyTrigger();
    ResetFakes();

    g_nullsBeforeNonNull = 0;                 // non-null immediately
    // zero (not populated), then growing, then settles at 9000 for 3+ reads.
    static const uint32_t seq[] = {0, 0, 100, 5000, 9000, 9000, 9000};
    g_countSeq = seq; g_countSeqLen = (int)(sizeof(seq) / sizeof(seq[0]));

    red4ext_mac::test::SetSingletonProvider(&FakeProvider);
    red4ext_mac::test::SetCountReader(&FakeCountReader);
    red4ext_mac::test::SetTimings(0, 3, 5000);

    red4ext_mac::RegisterApplyCallback([](TweakDB* db) {
        ++g_cbFireCount;
        g_cbSeenDB = db;
    });

    red4ext_mac::test::RunPollingLoopSync();

    check(g_cbFireCount == 1, "callback fired exactly once");
    // Stability reached only at the 3rd consecutive 9000 (7th count read / poll).
    check(red4ext_mac::GetPollCount() == 7, "fired only after count stabilized (poll 7)");
}

static void TestTimeout() {
    std::printf("Test 3: timeout when singleton never appears\n");
    red4ext_mac::test::ResetApplyTrigger();
    ResetFakes();

    g_nullsBeforeNonNull = 1000000;           // effectively always null
    static const uint32_t seq[] = {0};
    g_countSeq = seq; g_countSeqLen = 1;

    red4ext_mac::test::SetSingletonProvider(&FakeProvider);
    red4ext_mac::test::SetCountReader(&AlwaysNullCountReader);
    red4ext_mac::test::SetTimings(/*pollMs*/1, /*stableN*/3, /*timeoutMs*/25);

    red4ext_mac::RegisterApplyCallback([](TweakDB*) { ++g_cbFireCount; });

    red4ext_mac::test::RunPollingLoopSync();   // returns when timeout hit

    check(g_cbFireCount == 0, "callback never fired on timeout");
    check(!red4ext_mac::HasFired(), "HasFired() stays false on timeout");
    check(red4ext_mac::GetPollCount() >= 1, "loop polled at least once before bailing");
}

int main() {
    std::printf("=== apply_trigger_test (on-host, no game) ===\n");
    TestEarlyRegistrationSurvives();   // must run first: needs the ctor-registered cb intact
    TestFiresOnceWhenStable();
    TestWaitsForStability();
    TestTimeout();
    std::printf("=============================================\n");
    if (g_failures == 0) {
        std::printf("ALL TESTS PASSED\n");
        return 0;
    }
    std::printf("%d CHECK(S) FAILED\n", g_failures);
    return 1;
}
