# Which machines the build runs on. Included before project(), because CMake reads these
# settings when it sets up the compiler.
#
# macOS
#   Default:              builds for the Mac you're on (Intel -> x86_64, Apple silicon -> arm64).
#   -DMORF_MAC_UNIVERSAL=ON  one binary for both Intel and Apple silicon (what CI ships).
#   Either way the result runs on macOS 10.15 or later (Apple silicon always needs 11.0+).
#
# Windows
#   64-bit, with the C++ runtime linked statically so the plugin doesn't need the
#   Visual C++ redistributable on the user's machine.

option(MORF_MAC_UNIVERSAL "macOS: build one universal binary for Intel and Apple silicon" OFF)

if(MORF_MAC_UNIVERSAL AND NOT CMAKE_OSX_ARCHITECTURES)
  set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "macOS architectures" FORCE)
endif()

set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum macOS version")

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" CACHE STRING "MSVC runtime library")
