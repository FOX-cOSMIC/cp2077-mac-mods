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

# Placeholder to keep CMake happy.
add_custom_target(red4ext-mac-placeholder
    COMMENT "red4ext-mac: not yet implemented (Phase 0)"
)

# Diagnostic probes — built independently of the main framework.
add_subdirectory(probes)
