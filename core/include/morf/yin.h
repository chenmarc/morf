#pragma once
// YIN fundamental-frequency estimator (de Cheveigne & Kawahara, 2002),
// with the difference function computed through FFT cross-correlation so
// the per-frame cost is O(N log N) instead of O(W * tauMax).
//
// The analysis buffer holds W + tauMax samples (oldest first). For a
// 72 Hz lowest note at 48 kHz that's about 28 ms of audio, and that buffer
// length is what sets the worst-case latency for the low strings.
#include <complex>
#include <vector>

#include "morf/fft.h"

namespace morf {

struct PitchEstimate {
    float f0 = 0.f;            // Hz, 0 if no candidate
    float aperiodicity = 1.f;  // CMNDF value at the chosen lag: 0 = perfectly periodic, ~1 = noise
};

class YinDetector {
public:
    YinDetector(double sampleRate, double minHz, double maxHz, float threshold = 0.15f);

    // Number of samples analyze() expects.
    int bufferSize() const { return W_ + tauMax_; }
    int tauMax() const { return tauMax_; }

    // buf: bufferSize() samples, oldest first. Real-time safe.
    PitchEstimate analyze(const float* buf) { return analyze(buf, bufferSize()); }

    // Analyse only the most recent `len` samples (e.g. just the part since the
    // last pick attack, so the previous note or the silence before it doesn't
    // smear the estimate). The integration window and lag range shrink to fit;
    // a lag is only accepted if the window could also have seen twice that lag,
    // which stops a young window reporting the 2nd harmonic of a low note.
    // buf points at the oldest of those `len` samples.
    PitchEstimate analyze(const float* buf, int len);

private:
    double fs_;
    int tauMin_, tauMax_, W_;
    float threshold_;
    FFT fft_;
    std::vector<std::complex<float>> a_, b_;
    std::vector<double> cumsq_;
    std::vector<float> d_, cmnd_;
};

}  // namespace morf
