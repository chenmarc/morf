#pragma once
#include <string>
#include <vector>

#include "morf/bench/guitar_synth.h"

namespace morf {

struct TestCase {
    std::string name;      // e.g. "bends_03"
    std::string category;  // single, legato, repeat, bends, vibrato, palm, chords
    std::vector<NoteSpec> notes;
    Audio audio;
    PitchTrack truth;
};

// Deterministic synthetic test set covering the playing techniques the tracker must handle.
std::vector<TestCase> makeSyntheticTestSet(uint32_t seed, double sampleRate, int filesPerCategory = 6);

}  // namespace morf
