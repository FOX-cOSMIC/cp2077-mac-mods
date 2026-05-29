# red4ext-mac — core framework (dylib injection, hooking, symbol resolution)
#
# Phase 0: skeleton only. Source files land here as hook-engineer and
# tweakdb-engineer agents complete their tasks.

# Once src/ is populated, this will become:
#
# file(GLOB_RECURSE RED4EXT_SOURCES CONFIGURE_DEPENDS src/*.cpp)
# add_library(red4ext SHARED ${RED4EXT_SOURCES})
# target_include_directories(red4ext PUBLIC include)
# set_target_properties(red4ext PROPERTIES
#     OUTPUT_NAME "red4ext"
#     PREFIX "lib"
#     SUFFIX ".dylib"
# )

# ── red4ext framework dylib ──────────────────────────────────────────────────
# P1.1: slide-capture primitive (Symbols) + dylib startup entry (Loader).
# More runtime/ sources land here as P1.2+ complete. Listed explicitly (not
# globbed) so the slide_test main() never gets pulled into the dylib.
add_library(red4ext SHARED
    src/loader/Loader.cpp
    src/runtime/Symbols.cpp
    src/runtime/SingletonAccess.cpp
    src/runtime/TweakDB.cpp
    src/runtime/HashMap.cpp
    src/runtime/ApplyTrigger.cpp
)

target_compile_features(red4ext PRIVATE cxx_std_17)
target_compile_options(red4ext PRIVATE -Wall -Wextra)

# Public accessor headers (Symbols.hpp et al.) live under src/runtime for now;
# consumers include "runtime/Symbols.hpp" relative to src/.
target_include_directories(red4ext PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

set_target_properties(red4ext PROPERTIES
    OUTPUT_NAME "red4ext"
    PREFIX      "lib"
    SUFFIX      ".dylib"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/src/red4ext-mac"
)

# ── slide_test — TEMPORARY standalone exerciser for the Symbols API ──────────
# Links the framework so its constructors run in the test process too.
add_executable(slide_test src/runtime/slide_test.cpp)
target_link_libraries(slide_test PRIVATE red4ext)
set_target_properties(slide_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/src/red4ext-mac"
)

# ── tweakdb_test — P1.4 standalone layout/offset exerciser ───────────────────
# Pure compile-time layout checks (static_asserts) + offset dump. Does not link
# the dylib: it needs only TweakDB.hpp (header-only accessors). If F-015 drift
# breaks the layout, this target fails to COMPILE — the cheapest possible guard.
add_executable(tweakdb_test src/runtime/tweakdb_test.cpp)
target_compile_features(tweakdb_test PRIVATE cxx_std_17)
target_compile_options(tweakdb_test PRIVATE -Wall -Wextra)
target_include_directories(tweakdb_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
set_target_properties(tweakdb_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/src/red4ext-mac"
)

# ── hashmap_test — P1.5 hash-primitive unit test ────────────────────────────
# Verifies Fnv1a32/Crc32 against published vectors + TweakDBID packing. Links
# the dylib for the hash symbols (HashMap.cpp), same pattern as slide_test.
add_executable(hashmap_test src/runtime/hashmap_test.cpp)
target_link_libraries(hashmap_test PRIVATE red4ext)
set_target_properties(hashmap_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/src/red4ext-mac"
)

# ── apply_trigger_test — P1.3 on-host polling-loop unit test ─────────────────
# Drives the real PollingLoop() via the test seam with fake memory + fast
# timings. Built from explicit sources (NOT linked against the dylib) so the
# Loader constructor's auto-EnsureStarted() does NOT run a background polling
# thread in the test process — that thread would race the synchronous test
# driver over the shared ApplyTrigger globals. ApplyTrigger only needs
# SingletonAccess + Symbols (and TweakDB.hpp header-only for the offset asserts).
add_executable(apply_trigger_test
    src/runtime/apply_trigger_test.cpp
    src/runtime/ApplyTrigger.cpp
    src/runtime/SingletonAccess.cpp
    src/runtime/Symbols.cpp
)
target_compile_features(apply_trigger_test PRIVATE cxx_std_17)
target_compile_options(apply_trigger_test PRIVATE -Wall -Wextra)
target_include_directories(apply_trigger_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
set_target_properties(apply_trigger_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/src/red4ext-mac"
)

# Diagnostic probes — built independently of the main framework.
add_subdirectory(probes)
