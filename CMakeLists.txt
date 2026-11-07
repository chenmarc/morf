cmake_minimum_required(VERSION 3.20)
project(morf VERSION 0.2.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)   # morf_core links into plugin bundles
option(MORF_BUILD_PLUGIN "Build the JUCE plugin and standalone app (downloads JUCE)" OFF)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()
if(MSVC)
  add_compile_options(/W4)
else()
  add_compile_options(-Wall -Wextra -Wno-sign-compare -Wno-missing-field-initializers)
endif()

# Real-time engine: no dependencies, no allocation on the audio thread.
# The JUCE plugin (step 2) links this same library.
add_library(morf_core
  core/src/fft.cpp
  core/src/yin.cpp
  core/src/mono_tracker.cpp)
target_include_directories(morf_core PUBLIC core/include)

# Offline test bench: synthetic guitar, file I/O, scoring.
add_library(morf_bench
  io/audio_io.cpp
  synth/guitar_synth.cpp
  synth/test_set.cpp
  eval/evaluate.cpp)
target_link_libraries(morf_bench PUBLIC morf_core)

add_executable(morf_eval tools/morf_eval.cpp)
target_link_libraries(morf_eval PRIVATE morf_bench)

add_executable(morf_track tools/morf_track.cpp)
target_link_libraries(morf_track PRIVATE morf_bench)

enable_testing()
add_executable(morf_tests tests/test_main.cpp)
target_link_libraries(morf_tests PRIVATE morf_bench)
add_test(NAME morf_tests COMMAND morf_tests)

if(MORF_BUILD_PLUGIN)
  add_subdirectory(plugin)
endif()
