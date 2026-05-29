# tweakxl-mac — TweakDB patcher plugin (YAML/.tweak parser + applicator)
#
# Phase 0: skeleton only. Source files land here as mod-loader agent completes
# tasks. This plugin depends on red4ext-mac, so don't build it standalone.

# Once src/ is populated, this will become:
#
# file(GLOB_RECURSE TWEAKXL_SOURCES CONFIGURE_DEPENDS src/*.cpp)
# add_library(tweakxl SHARED ${TWEAKXL_SOURCES})
# target_include_directories(tweakxl PUBLIC include)
# target_link_libraries(tweakxl PRIVATE red4ext)
# set_target_properties(tweakxl PROPERTIES
#     OUTPUT_NAME "tweakxl"
#     PREFIX "lib"
#     SUFFIX ".dylib"
# )

# Placeholder to keep CMake happy.
add_custom_target(tweakxl-mac-placeholder
    COMMENT "tweakxl-mac: not yet implemented (Phase 0)"
)
