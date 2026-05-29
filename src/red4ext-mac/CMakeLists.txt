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

# Diagnostic probes — built independently of the main framework.
add_subdirectory(probes)
