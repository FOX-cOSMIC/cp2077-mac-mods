// Loader.cpp — dylib startup entry point for red4ext-mac.
//
// At dylib load: ensures the Symbols module is initialized (captures the slide,
// P1.1), does a one-shot singleton poll for diagnostics (P1.2), then registers
// the framework's "DB ready" callbacks and starts the apply-trigger polling
// loop (P1.3). The eventual tweakxl-mac Plugin.cpp orchestrator registers its
// applicator callback the same way.
//
// No __TEXT writes, no mprotect (FA-001). Read-only slide capture + polling.

#include "../runtime/Symbols.hpp"
#include "../runtime/SingletonAccess.hpp"
#include "../runtime/TweakDB.hpp"
#include "../runtime/HashMap.hpp"
#include "../runtime/ApplyTrigger.hpp"

#include <cstdarg>
#include <cstdio>
#include <ctime>

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

    // P1.3: register the framework's "DB populated & stable" callback, then
    // start the apply-trigger polling loop. The callback runs ONCE on the
    // polling thread when the records map is non-zero and stable (F-018 trigger
    // point). It folds in the bring-up probes that P1.2's deferred-sample
    // scaffold used to run (H-008 map identification, records hash function,
    // flats value-buffer layout) — now driven by the real trigger. Patchwork's
    // P1.10 applicator registers its own callback the same way; all registered
    // callbacks receive the same live TweakDB*.
    red4ext_mac::RegisterApplyCallback([](red4ext_mac::TweakDB* db) {
        log_line("[apply-trigger] lambda entered (db=%p)", (void*)db);
        red4ext_mac::VerifyH008(db);          // P1.4 — which map (+0x58/+0x108) is flats
        log_line("[apply-trigger] after VerifyH008");
        red4ext_mac::VerifyHashFunction(db);  // P1.5 — records bucket-hash function
        log_line("[apply-trigger] after VerifyHashFunction");
        red4ext_mac::VerifyFlatEntry(db);     // P1.6 — flats value-buffer layout + R/W mode
        log_line("[apply-trigger] after VerifyFlatEntry");
        red4ext_mac::VerifyCandidateFlats(db); // P1.12 — runtime-verify Scope's flat candidates
        log_line("[apply-trigger] after VerifyCandidateFlats");
        red4ext_mac::VerifyFlatArrayAccess(db); // F-031 — correct flats array (+0x40) read + round-trip self-test
        log_line("[apply-trigger] after VerifyFlatArrayAccess");
        red4ext_mac::VerifyGameSeesEdit(db); // ground-truth: does the game GetFlat see our edit? (env-gated)
        log_line("[apply-trigger] after VerifyGameSeesEdit");
        red4ext_mac::FindStatFlatByValue(db); // identify HP/RAM flats by live value (env-gated)
        log_line("[apply-trigger] after FindStatFlatByValue");
        red4ext_mac::DumpStatModifiers(db); // F-030: walk Health/Memory modifier graph (env-gated)
        log_line("[apply-trigger] after DumpStatModifiers");
        red4ext_mac::IdentifyStatModifiers(db); // F-039: find/double stored Memory(RAM) modifier (env-gated)
        log_line("[apply-trigger] after IdentifyStatModifiers");
        red4ext_mac::TestCreateFlat(db); // de-risk CreateRecord: new-flat +0x40 insert, verify via game GetFlat (env-gated)
        log_line("[apply-trigger] after TestCreateFlat");
        red4ext_mac::TestRecordInfo(db); // discover DynArray layout + ConstantStatModifier baseHash + typed-flat vtables (env-gated)
        log_line("[apply-trigger] after TestRecordInfo");
        red4ext_mac::TestCreateRecordAppend(db); // DECISIVE: replicate DoubleRam.yaml create+append (env-gated)
        log_line("[apply-trigger] after TestCreateRecordAppend");
        red4ext_mac::ApplyLiveStatBoosts(db); // create+append RAM-regen(live)+RAM/HP-cap(new-game) (env-gated)
        log_line("[apply-trigger] after ApplyLiveStatBoosts");
        red4ext_mac::MultiplyPrices(db); // visible-win demo: x10 all Price.* records (env-gated)
        log_line("[apply-trigger] after MultiplyPrices");
        red4ext_mac::MultiplyPricesUnique(db); // SAFE in-place: only unique-FlatValue prices (env-gated)
        log_line("[apply-trigger] after MultiplyPricesUnique");
        red4ext_mac::BoostStatFlats(db); // in-place x N curated HP/RAM stat flats (env-gated)
        log_line("[apply-trigger] after BoostStatFlats");
        red4ext_mac::StatOracle(db); // F-037: self-verifying runtime stat oracle (env-gated, poll thread)
        log_line("[apply-trigger] after StatOracle");
        red4ext_mac::StatPoke(db); // Path 1: heap-scan player StatsContainer + WRITE RAM cell (env-gated, poll thread)
        log_line("[apply-trigger] after StatPoke");
        red4ext_mac::TestFlatWritePath(db);   // F-031 — named-flat resolve + scalar edit round-trip (no save)
        log_line("[apply-trigger] after TestFlatWritePath");
        red4ext_mac::TestUpdateRecordBuild(db); // H-011 — factory build-from-edited-flats validation (env-gated)
        log_line("[apply-trigger] after TestUpdateRecordBuild");
        red4ext_mac::ProbeReflectedFlats(db); // F-034 — which candidate flats back a reflected field (env-gated)
        log_line("[apply-trigger] after ProbeReflectedFlats");
        red4ext_mac::DumpFlatsSample(db);     // P1.13 — walk +0x58 entries, dump for cinema prospecting
        log_line("[apply-trigger] after DumpFlatsSample");
        red4ext_mac::MapNamesToVftables(db);  // P1.14b — record-name → vtable mapping (vtable→type ID)
        log_line("[apply-trigger] after MapNamesToVftables");
        red4ext_mac::CinemaMutateStat(db);    // P1.17 — first end-to-end cinema mutation (env-gated)
        log_line("[apply-trigger] after CinemaMutateStat");
        red4ext_mac::CinemaBulkMutateStats(db); // P1.17b — bulk-mutate every Stat_Record Float (env-gated)
        log_line("[apply-trigger] after CinemaBulkMutateStats");
        red4ext_mac::ScanMemoryForFloat(db);  // P1.20 — process-wide float scan (env-gated, detached)
        log_line("[apply-trigger] after ScanMemoryForFloat");
        red4ext_mac::CinemaUnleash(db);       // P1.21 — aggressive shotgun: nuke every plausible float in every record (env-gated)
        log_line("[apply-trigger] after CinemaUnleash");
        log_line("[apply-trigger] system callbacks done; user mods can apply now");
    });
    red4ext_mac::EnsureStarted();
}
