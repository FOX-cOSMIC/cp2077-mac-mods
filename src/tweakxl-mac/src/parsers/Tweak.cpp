#include "Tweak.hpp"
#include "TweakInternal.hpp"

#include "Red/TweakDB/Source/Parser.hpp"

#include <cstdio>
#include <ctime>
#include <exception>
#include <mutex>
#include <string>

// P1.9 — psiberx .tweak parser integration (D-005).
//
// We do NOT write a .tweak parser. We reuse psiberx's PEGTL parser
// (reference/windows-tweakxl/src/Red/TweakDB/Source/Parser.cpp) compiled into
// libtweakxl.dylib under TWEAKXL_MAC_OFFLINE. See PsiberxParser.cpp for the
// thin TU that compiles that reference source on libc++.
//
// This file is the public wrapper: parse a path, return an opaque handle, log
// and return nullptr on any parse error.

namespace {
    // Mirrors the path Hookline uses for the injected dylib log. Best-effort:
    // if it can't be opened we still emit to stderr.
    constexpr const char* kMacLogPath = "/tmp/red4ext-mac.log";

    void LogError(const std::string& message) {
        std::fprintf(stderr, "[tweakxl] %s\n", message.c_str());

        static std::mutex logMutex;
        std::lock_guard<std::mutex> lock(logMutex);
        if (FILE* log = std::fopen(kMacLogPath, "a")) {
            std::time_t now = std::time(nullptr);
            char stamp[32];
            std::strftime(stamp, sizeof(stamp), "%Y-%m-%d %H:%M:%S", std::localtime(&now));
            std::fprintf(log, "%s [tweakxl] %s\n", stamp, message.c_str());
            std::fclose(log);
        }
    }
}

std::shared_ptr<tweakxl_mac::TweakSourceHandle>
tweakxl_mac::ParseTweakFile(const std::filesystem::path& path) {
    try {
        // Red::TweakParser::Parse throws on syntax errors (a tao::pegtl::parse_error
        // rewrapped as a std::exception carrying a "file:line:col: message" string —
        // see PsiberxParser.cpp for how that rewrap compiles on libc++).
        std::shared_ptr<Red::TweakSource> source = Red::TweakParser::Parse(path);
        if (!source) {
            LogError("Parse returned null for \"" + path.string() + "\".");
            return nullptr;
        }

        auto handle = std::make_shared<TweakSourceHandle>();
        handle->source = std::move(source);
        return handle;
    }
    catch (const std::exception& ex) {
        LogError("Failed to parse \"" + path.string() + "\": " + ex.what());
        return nullptr;
    }
    catch (...) {
        LogError("Unknown error while parsing \"" + path.string() + "\".");
        return nullptr;
    }
}
