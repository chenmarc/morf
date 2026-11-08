# Compiler and platform settings shared by every target.

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)   # morf_core is linked into plugin bundles

# Single-config generators (Make, Ninja) default to an optimised build.
get_property(_morf_multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(NOT _morf_multi_config AND NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Command-line tools and test programs all land in build/bin (build/bin/<Config> for Xcode/Visual Studio).
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")

# Warnings for morf's own code only (third-party code such as JUCE keeps its own settings).
add_library(morf_warnings INTERFACE)
if(MSVC)
  target_compile_options(morf_warnings INTERFACE /W4)
else()
  target_compile_options(morf_warnings INTERFACE -Wall -Wextra -Wno-sign-compare -Wno-missing-field-initializers)
endif()
