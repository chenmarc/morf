# Makes JUCE available: a local checkout via -DMORF_JUCE_DIR=/path/to/JUCE, otherwise a pinned download.
include(FetchContent)

set(MORF_JUCE_VERSION "8.0.4" CACHE STRING "JUCE release tag to download")

if(MORF_JUCE_DIR)
  add_subdirectory("${MORF_JUCE_DIR}" "${CMAKE_BINARY_DIR}/juce" EXCLUDE_FROM_ALL)
else()
  FetchContent_Declare(juce
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG ${MORF_JUCE_VERSION}
    GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(juce)
endif()
