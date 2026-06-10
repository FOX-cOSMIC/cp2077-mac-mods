cmake_minimum_required(VERSION 3.15)
project(cp2077-mac-mods
    VERSION 0.1.0
    DESCRIPTION "RED4ext + TweakXL port for macOS Cyberpunk 2077"
    LANGUAGES CXX
)

# Hard requirement: Apple Silicon macOS.
if(NOT APPLE)
    message(FATAL_ERROR "This project targets macOS Apple Silicon only.")
endif()

if(NOT CMAKE_SYSTEM_PROCESSOR MATCHES "arm64")
    message(WARNING
        "Configuring on non-arm64 host. Cross-compilation to arm64-apple-darwin "
        "is supported but the resulting dylib will not load on Intel Macs (game is arm64).")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "macOS deployment target")

# Position-independent code required for dylibs.
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# enable_testing() MUST precede add_subdirectory() so subdirectory add_test() calls
# register with ctest (otherwise CTestTestfile.cmake is never generated for them).
enable_testing()

# Phase 0: source trees are skeletons. They register their targets when ready.
add_subdirectory(src/red4ext-mac)
add_subdirectory(src/tweakxl-mac)

# add_subdirectory(tests)   # uncomment once tests/ has CMakeLists.txt
