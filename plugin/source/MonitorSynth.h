#pragma once
// Tiny monophonic synth so you can hear what the tracker plays without
// routing MIDI anywhere. Band-limited-ish saw (PolyBLEP) through a one-pole
// lowpass, with a short attack/release envelope. Real-time safe.
#include <cmath>

namespace morf {

class MonitorSynth {
public:
    void prepare(double sampleRate) {
        fs_ = sampleRate;
        phase_ = 0.0;
        env_ = 0.f;
        lp_ = 0.f;
    }
    void noteOn(int note, int velocity) {
        note_ = note;
        targetAmp_ = 0.25f * float(velocity) / 127.f;
        gate_ = true;
    }
    void noteOff(int note) {
        if (note == note_) gate_ = false;
    }
    void pitchBend(float semitones) { bend_ = semitones; }

    // Adds into out[0..n).
    void render(float* out, int n, float level) {
        const double freq = 440.0 * std::pow(2.0, (note_ + bend_ - 69.0) / 12.0);
        const double dt = freq / fs_;
        const float attack = 1.f - std::exp(-1.f / float(0.002 * fs_));
        const float release = 1.f - std::exp(-1.f / float(0.08 * fs_));
        const float lpCoef = 1.f - std::exp(-2.f * 3.14159265f * 2500.f / float(fs_));
        for (int i = 0; i < n; ++i) {
            const float target = gate_ ? targetAmp_ : 0.f;
            env_ += (target - env_) * (gate_ ? attack : release);
            if (env_ < 1e-5f && !gate_) { env_ = 0.f; continue; }
            phase_ += dt;
            if (phase_ >= 1.0) phase_ -= 1.0;
            float v = float(2.0 * phase_ - 1.0);
            v -= polyBlep(phase_, dt);
            lp_ += lpCoef * (v - lp_);
            out[i] += lp_ * env_ * level;
        }
    }

private:
    static float polyBlep(double t, double dt) {
        if (t < dt) { t /= dt; return float(t + t - t * t - 1.0); }
        if (t > 1.0 - dt) { t = (t - 1.0) / dt; return float(t * t + t + t + 1.0); }
        return 0.f;
    }
    double fs_ = 48000, phase_ = 0;
    int note_ = 60;
    float bend_ = 0, env_ = 0, targetAmp_ = 0, lp_ = 0;
    bool gate_ = false;
};

}  // namespace morf
