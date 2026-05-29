# tweakxl-mac — TweakDB patcher plugin (.tweak/YAML parser + applicator)
#
# P1.7 (mod scanner) + P1.9 (psiberx .tweak parser integration, D-005).
# Builds libtweakxl.dylib. The applicator (P1.10) and YAML parser land later.

# --- psiberx reference sources we reuse (D-005) -----------------------------
# The .tweak parser is psiberx's, compiled here under TWEAKXL_MAC_OFFLINE.
set(PSIBERX_ROOT "${CMAKE_SOURCE_DIR}/reference/windows-tweakxl")
set(PSIBERX_SRC  "${PSIBERX_ROOT}/src")
set(PEGTL_INC    "${PSIBERX_ROOT}/vendor/pegtl/include")
set(RED4EXT_INC  "${PSIBERX_ROOT}/vendor/RED4ext.SDK/include")

# NOTE: we do NOT add reference .../Source/Parser.cpp directly to the sources.
# It is MSVC-only and will not compile unmodified on libc++. Instead,
# src/parsers/PsiberxParser.cpp #includes that reference .cpp and supplies the
# two libc++ adaptations it needs (a constexpr CName, and remapping the MSVC
# `std::exception(const char*)` throw to std::runtime_error). This keeps the
# vendored spec mirror pristine. See PsiberxParser.cpp for the full rationale.
set(TWEAKXL_SOURCES
    src/loader/ModScanner.cpp
    src/parsers/Tweak.cpp
    src/parsers/PsiberxParser.cpp   # wraps reference Source/Parser.cpp
)

add_library(tweakxl SHARED ${TWEAKXL_SOURCES})

set_target_properties(tweakxl PROPERTIES
    OUTPUT_NAME "tweakxl"
    PREFIX "lib"
    SUFFIX ".dylib"
)

# The psiberx parser uses std::format (C++20). The rest of the project is C++17;
# we raise the bar for this target only.
target_compile_features(tweakxl PRIVATE cxx_std_20)

target_compile_definitions(tweakxl PRIVATE TWEAKXL_MAC_OFFLINE=1)

target_include_directories(tweakxl PRIVATE
    src                 # our headers (loader/, parsers/)
    "${PSIBERX_SRC}"    # Red/TweakDB/Source/*
    "${PEGTL_INC}"      # <tao/pegtl.hpp>
    "${RED4EXT_INC}"    # <RED4ext/Hashing/FNV1a.hpp>
)

# std::format is gated behind macOS 13.3 in libc++'s availability annotations.
# The game requires macOS 15.5 (FACTS F-001), so raising this target's floor is
# safe and necessary (the project default deployment target is 12.0).
target_compile_options(tweakxl PRIVATE -mmacosx-version-min=13.3)
target_link_options(tweakxl PRIVATE -mmacosx-version-min=13.3)

# Warnings. -Wall -Wextra on our code. Two documented waivers for psiberx code:
#   * -Wno-explicit-specialization-storage-class — Errors.hpp (included via
#     Parser.hpp by every TU) declares explicit template specializations with a
#     `static` storage class (MSVC-tolerated; clang warns but accepts). Applied
#     target-wide because Errors.hpp reaches more than one TU.
#   * -Wno-unused-parameter — psiberx's Parser.cpp ParseAction specializations
#     leave in/package unused in some cases. Scoped to PsiberxParser.cpp only so
#     our own sources stay strict (see set_source_files_properties below).
target_compile_options(tweakxl PRIVATE
    -Wall -Wextra -Wno-explicit-specialization-storage-class)
set_source_files_properties(src/parsers/PsiberxParser.cpp PROPERTIES
    COMPILE_OPTIONS "-Wno-unused-parameter")

# --- scanner_test: standalone exerciser for P1.7 (temporary) ----------------
add_executable(scanner_test src/loader/scanner_test.cpp src/loader/ModScanner.cpp)
target_include_directories(scanner_test PRIVATE src)
target_compile_features(scanner_test PRIVATE cxx_std_17)
target_compile_options(scanner_test PRIVATE -Wall -Wextra)
