# tweakxl-mac — TweakDB patcher plugin (.tweak/YAML parser + applicator)
#
# P1.7 (mod scanner) + P1.9 (psiberx .tweak parser) + P1.8 (YAML parser).
# Builds libtweakxl.dylib. The applicator (P1.10) lands later.

# --- yaml-cpp (P1.8) --------------------------------------------------------
# Pinned to 0.8.0 — the latest stable tag (released 2023-09; no newer release
# exists as of Spring 2026). yaml-cpp 0.8.0's CMakeLists declares a pre-3.5
# minimum, which CMake 4.x refuses; CMAKE_POLICY_VERSION_MINIMUM=3.5 lets it
# configure. We also disable its tests/contrib/tools to keep the build lean.
include(FetchContent)
set(YAML_CPP_BUILD_TESTS   OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_TOOLS   OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_CONTRIB OFF CACHE BOOL "" FORCE)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "" FORCE)  # yaml-cpp 0.8.0 + CMake 4.x
FetchContent_Declare(
    yaml_cpp
    GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
    GIT_TAG 0.8.0
)
FetchContent_MakeAvailable(yaml_cpp)

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
    src/parsers/YAML.cpp            # P1.8 YAML → Op AST
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
    src                 # our headers (loader/, parsers/, model/)
    "${PSIBERX_SRC}"    # Red/TweakDB/Source/*
    "${PEGTL_INC}"      # <tao/pegtl.hpp>
    "${RED4EXT_INC}"    # <RED4ext/Hashing/FNV1a.hpp>
)

target_link_libraries(tweakxl PRIVATE yaml-cpp::yaml-cpp)

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

# --- yaml_test: standalone unit tests for P1.8 (temporary) ------------------
# YAML.cpp has no std::format dependency, so the tests build at C++17 with the
# project default deployment target.
add_executable(yaml_test src/parsers/yaml_test.cpp src/parsers/YAML.cpp)
target_include_directories(yaml_test PRIVATE src)
target_compile_features(yaml_test PRIVATE cxx_std_17)
target_compile_options(yaml_test PRIVATE -Wall -Wextra)
target_link_libraries(yaml_test PRIVATE yaml-cpp::yaml-cpp)
add_test(NAME yaml_test COMMAND yaml_test)
