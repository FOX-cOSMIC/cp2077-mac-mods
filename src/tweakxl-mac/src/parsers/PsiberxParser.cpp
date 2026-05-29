// PsiberxParser.cpp — compiles psiberx's reference .tweak parser into our build.
//
// D-005: reuse psiberx's PEGTL parser rather than re-implement it. The reference
// translation unit is:
//
//   reference/windows-tweakxl/src/Red/TweakDB/Source/Parser.cpp
//
// It was written for the MSVC/Windows toolchain and does NOT compile unmodified
// on Apple clang + libc++. Rather than patch the pristine vendored spec mirror,
// this thin wrapper #includes the reference .cpp and supplies the two
// macOS-offline adaptations it needs. Both are confined to this TU.
//
//   1) `CName` — Parser.cpp's ResolveType()/ResolveOperation() use unqualified
//      `CName` for compile-time string switches. On Windows this resolves to
//      RED4ext::CName (pulled in via pch.hpp). Under TWEAKXL_MAC_OFFLINE no such
//      type is in scope, so we provide a minimal constexpr Red::CName that hashes
//      with RED4ext's own FNV1a64. Only internal consistency matters here (input
//      string vs. the TweakGrammar::Type/Op constants are hashed by the same
//      function), and using the real FNV1a64 keeps it faithful besides.
//
//   2) `std::exception(const char*)` — Parser.cpp throws `std::exception(msg)`,
//      an MSVC extension; libc++'s std::exception has no such constructor. We
//      remap `std::exception(...)` to `std::runtime_error(...)` (which has a
//      string constructor and derives from std::exception, so existing
//      `catch (const std::exception&)` sites are unaffected). The macro is
//      function-like, so it only fires on the two `exception(` call sites and
//      never on `exception&` in catch clauses. We define it only AFTER all
//      standard/PEGTL headers are included (via Parser.hpp below), so it cannot
//      perturb any library header.
//
// NOTE for researcher: this contradicts F-002's claim that the .tweak parser
// needs "no additional stubs or changes ... beyond defining the flag" and that
// Parser.cpp has "no RED4ext dependency." It compiles only with the two shims
// above. See FOLLOW-UPS in the P1.7/P1.9 handoff.

#ifndef TWEAKXL_MAC_OFFLINE
#define TWEAKXL_MAC_OFFLINE 1
#endif

#include <cstddef>
#include <cstdint>
#include <RED4ext/Hashing/FNV1a.hpp>

namespace Red {
    // Minimal stand-in for RED4ext::CName, sufficient for the compile-time
    // string switches in Parser.cpp. Constexpr so it works in case labels.
    struct CName {
        uint64_t hash;
        constexpr CName(uint64_t aHash = 0) noexcept : hash(aHash) {}
        constexpr CName(const char* aName) noexcept : hash(::RED4ext::FNV1a64(aName)) {}
        constexpr operator uint64_t() const noexcept { return hash; }
    };
}

// Pull in PEGTL, Grammar, Errors, Source (all #pragma once) up front so the
// remap macro below cannot touch any of them.
#include "Red/TweakDB/Source/Parser.hpp"

#include <stdexcept>

#define exception(...) runtime_error(__VA_ARGS__)
#include "Red/TweakDB/Source/Parser.cpp"
#undef exception
