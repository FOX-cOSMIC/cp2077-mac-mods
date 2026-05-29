# h001_probe — diagnostic dylib for H-001 (TweakDB function-pointer dispatch table)
#
# Load with DYLD_INSERT_LIBRARIES. No production hooking; read-only diagnostic only.
# See tools/run-h001-probe.sh for the launch wrapper.

add_library(h001_probe SHARED
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/probes/h001_probe.cpp
)

target_compile_options(h001_probe PRIVATE
    -Wall
    -Wextra
    -Wno-unused-parameter
)

# Output alongside the future libred4ext.dylib for easy discovery.
set_target_properties(h001_probe PROPERTIES
    OUTPUT_NAME              "h001_probe"
    PREFIX                   "lib"
    SUFFIX                   ".dylib"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/src/red4ext-mac"
)
