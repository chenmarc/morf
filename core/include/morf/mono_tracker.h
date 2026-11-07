#pragma once
// Baseline single-note tracker: YIN pitch every hop, plus an energy-based
// onset detector and a small state machine that turns frames into
// Note On / Note Off / Pitch Bend.
#include <vector>

#include "morf/tracker.h"
#include "morf/yin.h"

namespace morf {

struct MonoTrackerConfig {
    double sampleRate = 48000.0;
    double minHz = 72.0;      // just below drop-D low string (73.4 Hz)
    double maxHz = 1400.0;    // above the 24th fret of the high E (1319 Hz)
    int hop = 64;             // analysis hop in samples (1.33 ms at 48 kHz)
    float yinThreshold = 0.15f;
    float confidentAperiodicity = 0.2f;  // frames above this are "unvoiced"...
    float stableAperiodicity = 0.4f;     // ...unless below this and the pitch has held steady
    int stableFrames = 3;                // for this many frames (pick attacks are noisy but stable)
    float gateOnDb = -52.f;   // level needed to start a note (dBFS, 5 ms RMS)
    float gateOffDb = -60.f;  // level below which a note ends
    float relativeOffDb = 45.f;   // ...or once it has decayed this far below its peak
    int confirmFrames = 2;    // consistent voiced frames before a Note On
    int octaveConfirmFrames = 8;   // octave jumps are usually estimation errors: need ~10 ms
    float jumpSemitones = 0.35f;  // frame-to-frame pitch jump treated as a new (legato) note
    float trackingPeriods = 4.f;  // while a note sounds, follow its pitch with a window this many periods long
    float bendRange = 12.f;   // pitch-bend range in semitones (must match the synth)
    float bendStepCents = 1.f;    // don't send bends smaller than this
    float muteDropDb = 12.f;  // level drop within muteWindowMs that means "string was muted"
    float muteWindowMs = 15.f;
    float maxUnvoicedMs = 40.f;   // hold a note through this much unvoiced audio
    float onsetRatio = 4.f;   // attack-energy jump that counts as a new pluck
    float onsetRefractoryMs = 30.f;
    float pendingOnsetMs = 60.f;
    int channel = 0;
};

class MonoTracker : public Tracker {
public:
    explicit MonoTracker(const MonoTrackerConfig& cfg = {});
    const char* name() const override { return "mono-yin"; }
    void reset() override;
    void process(const float* in, int n, EventQueue& out) override;
    int analysisWindow() const override { return yin_.bufferSize(); }

    // Last frame's raw analysis, for debugging and visualisation.
    struct Frame {
        float f0 = 0, aperiodicity = 1, levelDb = -120;
        bool voiced = false, onset = false;
    };
    const Frame& lastFrame() const { return frame_; }

private:
    void analyseFrame(EventQueue& out);
    void startNote(float midi, EventQueue& out, int velocity = -1);
    void stopNote(EventQueue& out);
    void sendBend(float semis, bool force, EventQueue& out);

    MonoTrackerConfig cfg_;
    YinDetector yin_;
    std::vector<float> history_;  // last yin_.bufferSize() samples, oldest first
    std::vector<float> dbHistory_;
    int dbPos_ = 0;
    int hopFill_ = 0;
    int64_t samplePos_ = 0;
    int64_t segmentStart_ = 0;  // analysis only looks back to the last pick attack
    float prevSample_ = 0.f;
    float hopDiffEnergy_ = 0.f;
    float slowDiffEnergy_ = 0.f;
    float slowAlpha_ = 0.f;
    int framesSinceOnset_ = 1 << 20;
    int refractoryFrames_, unvoicedLimitFrames_, pendingLimitFrames_, muteFrames_;

    // Note state
    bool active_ = false;
    int note_ = -1;
    int velocity_ = 0;
    float bend_ = 0.f;        // last sent bend, semitones
    float peakDb_ = -120.f;
    int unvoiced_ = 0;
    bool pendingOnset_ = false;
    int pendingAge_ = 0;
    bool needOnset_ = false;  // after a mute, don't retrigger from the decaying residue
    // Candidate pitch (for confirming new notes)
    float candMidi_ = 0.f;
    int candCount_ = 0;
    int onTrackCount_ = 0;

    Frame frame_;
    float prevF0Midi_[4] = {};
    int stableRun_ = 0;
};

float hzToMidi(float hz);
float midiToHz(float midi);

}  // namespace morf
