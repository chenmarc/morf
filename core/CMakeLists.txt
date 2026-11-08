# Real-time engine: no dependencies, no allocation on the audio thread.
add_library(morf_core
  src/fft.cpp
  src/yin.cpp
  src/mono_tracker.cpp)
target_include_directories(morf_core PUBLIC include)
target_link_libraries(morf_core PRIVATE morf_warnings)
