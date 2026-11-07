#pragma once
// Scores a tracker's MIDI output against ground truth.
#include <string>
#include <vector>

#include "morf/events.h"
#include "../synth/guitar_synth.h"

namespace morf {

struct EvalOptions {
    double onsetEarlyTolerance = 0.02;  // a Note On may come this early (s)...
    double onsetLateTolerance = 0.10;   // ...or this late and still count as a hit
    int bendRange = 12;                 // to decode pitch-bend messages
};

struct NoteResult {  // one ground-truth note
    int midi = 0;
    double onset = 0, offset = 0;
    bool matched = false;
    double latency = 0;     // Note On time - true onset
    double offLatency = 0;  // Note Off time - true offset
};

struct CaseResult {
    std::string name, category;
    int truthNotes = 0, estNotes = 0, matched = 0, octaveErrors = 0;
    std::vector<NoteResult> notes;
    std::vector<float> pitchErrorCents;  // per ms where exactly one truth and one estimated pitch sound
    double audioSeconds = 0, cpuSeconds = 0;
};

CaseResult evaluateCase(const std::vector<NoteSpec>& truthNotes, const PitchTrack* truthPitch,
                        const std::vector<MidiEvent>& events, double sampleRate, double audioSeconds,
                        const EvalOptions& opt = {});

// Markdown report: one row per category plus latency by register.
std::string summarise(const std::vector<CaseResult>& results, const std::string& title);

}  // namespace morf
