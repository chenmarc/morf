# Offline test bench: synthetic guitar, WAV/MIDI file I/O, scoring, and the command-line tools.
add_library(morf_bench
  src/audio_io.cpp
  src/guitar_synth.cpp
  src/test_set.cpp
  src/evaluate.cpp)
target_include_directories(morf_bench PUBLIC include)
target_link_libraries(morf_bench PUBLIC morf_core PRIVATE morf_warnings)

add_executable(morf_eval tools/morf_eval.cpp)
target_link_libraries(morf_eval PRIVATE morf_bench morf_warnings)

add_executable(morf_track tools/morf_track.cpp)
target_link_libraries(morf_track PRIVATE morf_bench morf_warnings)
