// Plugin.cpp — tweakxl-mac startup entry (P1.11 orchestrator).
//
// At dylib load: registers an ApplyCallback with red4ext_mac. When the callback
// fires (DB populated + stable, on Hookline's polling thread), we resolve the
// mods directory, scan r6/tweaks/, parse each mod, apply YAML mods via ApplyMod,
// and log aggregate results. This closes the end-to-end loop:
//
//   game launch -> dylib injected -> apply-trigger fires -> scan -> parse ->
//   applicator writes to TweakDB.
//
// .tweak files are scanned + parsed here (validates the chain) but NOT applied:
// the TweakSourceHandle -> ModFile adapter is P1.11b. YAML is the demo path.
//
#include "loader/ModScanner.hpp"      // ScanModsDirectory, DiscoveredMod
#include "parsers/YAML.hpp"           // ParseYamlFile
#include "parsers/Tweak.hpp"          // ParseTweakFile (parse-only for now)
#include "applicator/Applicator.hpp"  // ApplyMod, ApplyResult

#include "runtime/ApplyTrigger.hpp"   // RegisterApplyCallback, EnsureStarted
namespace red4ext_mac { struct TweakDB; }

#include <cstdarg>
#include <cstdio>
#include <cstdlib>          // std::getenv
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

#include <mach-o/dyld.h>    // _NSGetExecutablePath

namespace {

constexpr const char* kLogPath = "/tmp/red4ext-mac.log";

// Same sink as the rest of the stack (Loader.cpp): append to the shared log file
// AND mirror to stderr, so the smoke test can grep a stable location.
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

// Resolve the mods directory. Env override wins (for testing); otherwise derive
// <game-install>/r6/tweaks/ from the running executable's path.
//
//   exe = <game-install>/Cyberpunk2077.app/Contents/MacOS/Cyberpunk2077
//   <game-install> = exe ↑4  (MacOS → Contents → *.app → install dir)
std::filesystem::path ResolveModsDir() {
    if (const char* env = std::getenv("TWEAKXL_MODS_DIR"); env && *env) {
        return std::filesystem::path(env);
    }

    uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);     // first call fills required size
    std::vector<char> buf(size ? size : 1);
    if (_NSGetExecutablePath(buf.data(), &size) != 0) {
        return {};                            // buffer too small / unavailable
    }

    std::filesystem::path exe(buf.data());
    std::error_code ec;
    std::filesystem::path canon = std::filesystem::weakly_canonical(exe, ec);
    if (!ec && !canon.empty()) exe = canon;   // resolve any symlinks in the path

    std::filesystem::path install = exe.parent_path()   // .../Contents/MacOS
                                       .parent_path()   // .../Contents
                                       .parent_path()   // .../Cyberpunk2077.app
                                       .parent_path();  // <game-install>
    return install / "r6" / "tweaks";
}

// The orchestrator body — runs once, on the polling thread, when the DB is
// populated and stable. Non-blocking: a handful of file parses + scalar writes.
void ApplyAllMods(red4ext_mac::TweakDB* db) {
    const std::filesystem::path modsDir = ResolveModsDir();
    log_line("[tweakxl] mods dir: %s", modsDir.c_str());

    std::error_code ec;
    if (modsDir.empty() || !std::filesystem::is_directory(modsDir, ec)) {
        log_line("[tweakxl] mods dir not found / not a directory — nothing to apply");
        log_line("[tweakxl] mods scanned: 0 (yaml=0, tweak=0)");
        return;
    }

    std::vector<tweakxl_mac::DiscoveredMod> mods = tweakxl_mac::ScanModsDirectory(modsDir);
    uint32_t yamlCount = 0, tweakCount = 0;
    for (const auto& m : mods) {
        if (m.type == tweakxl_mac::ModFileType::Yaml) ++yamlCount;
        else                                          ++tweakCount;
    }
    log_line("[tweakxl] mods scanned: %zu (yaml=%u, tweak=%u)",
             mods.size(), yamlCount, tweakCount);

    tweakxl_mac::ApplyResult agg;
    for (const auto& mod : mods) {
        if (mod.type == tweakxl_mac::ModFileType::Tweak) {
            // P1.11b: parse to validate the chain, but the TweakSource->ModFile
            // adapter isn't wired yet, so we do not apply.
            auto handle = tweakxl_mac::ParseTweakFile(mod.path);
            log_line("[tweakxl] P1.11b: .tweak applicator not yet wired — "
                     "parsed%s, skipping apply: %s",
                     handle ? " ok" : " FAILED", mod.path.c_str());
            continue;
        }

        std::unique_ptr<tweakxl_mac::ModFile> modFile = tweakxl_mac::ParseYamlFile(mod.path);
        if (!modFile) {
            log_line("[tweakxl] yaml parse failed (skipping): %s", mod.path.c_str());
            continue;   // ModFile null -> nothing to apply; keep going to next mod
        }

        // Apply ALL mods even if one fails — ApplyMod is atomic per-mod.
        tweakxl_mac::ApplyResult r = tweakxl_mac::ApplyMod(db, *modFile);
        agg.applied         += r.applied;
        agg.skipped         += r.skipped;
        agg.rejected        += r.rejected;
        agg.mods_ok         += r.mods_ok;
        agg.mods_rolled_back += r.mods_rolled_back;
    }

    log_line("[tweakxl] yaml applied: %u/%u, ops applied: %u, skipped: %u, "
             "rejected: %u, rollbacks: %u",
             agg.mods_ok, yamlCount, agg.applied, agg.skipped,
             agg.rejected, agg.mods_rolled_back);
    log_line("[tweakxl] tweak files skipped: %u (P1.11b)", tweakCount);
}

} // namespace

// High-priority constructor (101) — same pattern as Loader.cpp. ApplyTrigger
// uses construct-on-first-use, so registering from another constructor(101) TU
// is safe (no static-init-order fiasco — P1.3c fixed this).
__attribute__((constructor(101)))
static void tweakxl_mac_init() {
    red4ext_mac::RegisterApplyCallback([](red4ext_mac::TweakDB* db) {
        ApplyAllMods(db);
    });
    red4ext_mac::EnsureStarted();   // idempotent; Loader.cpp also calls it
    log_line("[tweakxl] plugin orchestrator init");
}
