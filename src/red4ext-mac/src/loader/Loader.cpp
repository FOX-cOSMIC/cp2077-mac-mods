// Loader.cpp — dylib startup entry point for red4ext-mac.
//
// Minimal P1.1 stub: ensures the Symbols module is initialized at load and
// logs the captured slide once the main Cyberpunk2077 image is detected.
// The eventual tweakxl-mac Plugin.cpp orchestrator hooks in from here.
//
// No __TEXT writes, no mprotect (FA-001). Read-only slide capture only.

#include "../runtime/Symbols.hpp"
#include "../runtime/SingletonAccess.hpp"

#include <cstdarg>
#include <cstdio>
#include <ctime>
#include <chrono>
#include <thread>

// Symbols' init entry — declared here (kept out of the public header, which is
// the stable accessor API). Registers the dyld add-image callback idempotently.
namespace red4ext_mac { void EnsureInit(); }

namespace {

constexpr const char* kLogPath = "/tmp/red4ext-mac.log";

// Log to both a dedicated file and stderr so the injection smoke test can grep
// a stable location regardless of how the game multiplexes its own stderr.
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

} // namespace

// High priority (101): run before default-priority constructors so the slide
// is captured and logged at the earliest well-defined point in dylib startup.
__attribute__((constructor(101)))
static void red4ext_mac_loader_init() {
    // Guarantee the dyld callback is registered now. dyld fires it
    // synchronously for the already-mapped main image, so the slide is
    // available immediately on return (the main binary loads before any
    // DYLD_INSERT_LIBRARIES dylib's initializers run).
    red4ext_mac::EnsureInit();

    std::time_t t = std::time(nullptr);
    char ts[32] = {};
    std::strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%S", std::localtime(&t));

    if (red4ext_mac::IsMainImageLoaded()) {
        log_line("[red4ext-mac] loader init %s, base=0x%lx slide=0x%lx",
                 ts,
                 (unsigned long)red4ext_mac::GetImageBase(),
                 (unsigned long)red4ext_mac::GetSlide());
    } else {
        // Should not happen under DYLD_INSERT_LIBRARIES into the game; logged
        // so a standalone / unexpected load is diagnosable.
        log_line("[red4ext-mac] loader init %s, main image NOT detected "
                 "(slide unavailable)", ts);
    }

    // One-shot initial poll of the singleton (P1.2). At dylib-load time the
    // game has NOT yet constructed TweakDB (F-011: built later in engine init),
    // so this is expected to be null. It validates the inject→slide→read chain.
    {
        void* db = (void*)red4ext_mac::GetTweakDB();
        log_line("[singleton-access] initial poll: %p%s", db,
                 db ? "" : " (null — expected at load time; DB built later)");
    }

    // Deferred single sample (temporary P1.2 validation aid). The smoke test
    // waits ~10s for the game to construct TweakDB; this thread takes ONE late
    // reading and logs it so the test can confirm the accessor returns the live
    // heap pointer in-game. This is NOT the P1.3 apply-trigger: no loop, no
    // stability/count logic, no callbacks — a single read, then the thread
    // exits. P1.3 replaces it with the real polling loop; remove this then.
    std::thread([] {
        std::this_thread::sleep_for(std::chrono::seconds(6));
        void* db = (void*)red4ext_mac::GetTweakDBUncached();
        log_line("[singleton-access] deferred sample: %p%s", db,
                 db ? "" : " (still null after delay)");
    }).detach();
}
