#pragma once
// Synthetic guitar for testing with known ground truth.
//
// Each note is an extended Karplus-Strong string (Jaffe & Smith): a delay
// line one period long, a loop filter for frequency-dependent decay, a
// fractional (Lagrange) delay so the pitch can bend smoothly, a pick-position
// comb on the excitation and a pickup-position comb on the output. It isn't
// a real guitar, but it has the properties that make tracking hard: sharp
// attacks, a pitch glide right after the pick, harmonics that decay at
// different rates, bends, vibrato, palm mutes and hand-muted note endings.
#include <cstdint>
#include <utility>
#include <vector>

#include "../io/audio_io.h"

namespace morf {

struct NoteSpec {
    double onset = 0, offset = 0;  // seconds; at `offset` the string is hand-muted
    int midi = 60;
    float velocity = 0.8f;         // 0..1
    bool palmMute = false;
    // Pitch-bend curve in semitones, piecewise linear over (seconds after onset, semitones).
    std::vector<std::pair<double, float>> bend;
    float vibratoDepth = 0.f;      // semitones, peak
    float vibratoHz = 5.5f;
    double vibratoStart = 0.0;     // seconds after onset
};

// Ground-truth pitch per millisecond: every sounding note's pitch in MIDI units (fractional).
struct PitchTrack {
    double hopSeconds = 0.001;
    std::vector<std::vector<float>> frames;
};

struct RenderOptions {
    double sampleRate = 48000.0;
    float noiseDb = -72.f;       // broadband noise floor, dBFS RMS (before normalising)
    float humDb = -66.f;         // mains hum at 60 Hz plus harmonics
    float peakDb = -6.f;         // normalise the file to this peak
    double tailSeconds = 0.5;
    uint32_t seed = 1;
};

// Renders the notes. `truth` receives the true pitch of each note between onset and offset.
Audio renderGuitar(const std::vector<NoteSpec>& notes, const RenderOptions& opt, PitchTrack* truth);

// Pitch (in semitones from the nominal note) of a note at `t` seconds after onset, excluding the pick glide.
float bendAt(const NoteSpec& n, double t);

}  // namespace morf
