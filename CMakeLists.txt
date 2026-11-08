# morf: guitar to MIDI.
#
#   core/    real-time engine library (no dependencies)
#   bench/   offline test bench: synthetic guitar, scoring, command-line tools
#   plugin/  JUCE plugin (VST3, AU) and standalone app       (-DMORF_BUILD_PLUGIN=ON)
#   tests/   unit and integration tests (run with ctest)
cmake_minimum_required(VERSION 3.22)
project(morf VERSION 0.2.0 LANGUAGES C CXX)

option(MORF_BUILD_PLUGIN "Build the JUCE plugin and standalone app (downloads JUCE)" OFF)
option(MORF_BUILD_TESTS "Build the tests" ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(MorfPlatform)

add_subdirectory(core)
add_subdirectory(bench)

if(MORF_BUILD_PLUGIN)
  include(FetchJUCE)
  add_subdirectory(plugin)
endif()

if(MORF_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
