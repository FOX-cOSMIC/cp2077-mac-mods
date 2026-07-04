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

# h004_probe — diagnostic dylib for H-004/T-005 (inline __TEXT hooking:
# mprotect/mach_vm_protect under a given re-sign entitlement configuration).
# NEVER writes to __TEXT; only tests + immediately reverts page protection.
# See tools/run-h004-probe.sh for the launch wrapper.

add_library(h004_probe SHARED
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/probes/h004_probe.cpp
)

target_compile_options(h004_probe PRIVATE
    -Wall
    -Wextra
    -Wno-unused-parameter
)

set_target_properties(h004_probe PROPERTIES
    OUTPUT_NAME              "h004_probe"
    PREFIX                   "lib"
    SUFFIX                   ".dylib"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/src/red4ext-mac"
)
