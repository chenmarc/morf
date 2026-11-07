#include "morf/yin.h"

#include <algorithm>
#include <cmath>

namespace morf {

static int nextPow2(int n) {
    int p = 1;
    while (p < n) p <<= 1;
    return p;
}

YinDetector::YinDetector(double sampleRate, double minHz, double maxHz, float threshold)
    : fs_(sampleRate),
      tauMin_(std::max(2, int(std::floor(sampleRate / maxHz)))),
      tauMax_(int(std::ceil(sampleRate / minHz))),
      W_(tauMax_),
      threshold_(threshold),
      fft_(nextPow2(W_ + tauMax_ + 1)),
      a_(fft_.size()),
      b_(fft_.size()),
      cumsq_(W_ + tauMax_ + 1),
      d_(W_ + tauMax_ + 2),
      cmnd_(W_ + tauMax_ + 2) {}

PitchEstimate YinDetector::analyze(const float* x, int len) {
    const int N = fft_.size();
    const bool full = len >= W_ + tauMax_;
    // Full buffer: W = tauMax as configured. Partial: W = len/3, lags up to 2*len/3,
    // accepted periods up to len/3.
    const int W = full ? W_ : len / 3;
    const int tauMax = full ? tauMax_ : len - W;
    const int tauAccept = full ? tauMax_ : tauMax / 2;
    if (tauAccept <= tauMin_ + 2) return {};
    const int B = W + tauMax;

    // r(tau) = sum_{j<W} x[j] * x[j+tau], via IFFT(conj(FFT(a)) * FFT(b)).
    // No circular wrap because j + tau < B <= N.
    for (int i = 0; i < N; ++i) {
        a_[i] = {i < W ? x[i] : 0.f, 0.f};
        b_[i] = {i < B ? x[i] : 0.f, 0.f};
    }
    fft_.forward(a_.data());
    fft_.forward(b_.data());
    for (int i = 0; i < N; ++i) b_[i] = std::conj(a_[i]) * b_[i];
    fft_.inverse(b_.data());
    const float invN = 1.f / float(N);

    cumsq_[0] = 0.0;
    for (int i = 0; i < B; ++i) cumsq_[i + 1] = cumsq_[i] + double(x[i]) * x[i];
    const double e0 = cumsq_[W];
    if (e0 + (cumsq_[B] - cumsq_[tauMax]) < 1e-12) return {};

    // Difference function d(tau) = e0 + e(tau) - 2 r(tau), then the
    // cumulative-mean-normalised version d'(tau).
    d_[0] = 0.f;
    cmnd_[0] = 1.f;
    double running = 0.0;
    for (int tau = 1; tau <= tauMax; ++tau) {
        const double et = cumsq_[tau + W] - cumsq_[tau];
        const double r = double(b_[tau].real()) * invN;
        const double d = std::max(0.0, e0 + et - 2.0 * r);
        d_[tau] = float(d);
        running += d;
        cmnd_[tau] = running > 0.0 ? float(d * tau / running) : 1.f;
    }

    // Absolute threshold: first dip below threshold, then walk to its local minimum.
    int best = -1;
    for (int tau = tauMin_; tau < tauAccept; ++tau) {
        if (cmnd_[tau] < threshold_) {
            while (tau + 1 < tauMax && cmnd_[tau + 1] < cmnd_[tau]) ++tau;
            best = tau;
            break;
        }
    }
    if (best < 0) {  // No dip: fall back to the global minimum, reported with its (high) aperiodicity.
        best = tauMin_;
        for (int tau = tauMin_ + 1; tau < tauAccept; ++tau)
            if (cmnd_[tau] < cmnd_[best]) best = tau;
    }

    // Sub-harmonic check: during attacks the true period's dip can sit just above the
    // threshold while the dip at 2x or 3x the period (an octave or a twelfth down) is
    // below it. If a dip at best/2 or best/3 is nearly as good, prefer the shorter period.
    for (int k = 3; k >= 2; --k) {
        int t = int(std::lround(double(best) / k));
        if (t < tauMin_ + 1) continue;
        for (int j = std::max(tauMin_, t - 2); j <= t + 2; ++j)
            if (cmnd_[j] < cmnd_[t]) t = j;
        if (cmnd_[t] < 0.4f && cmnd_[t] < cmnd_[best] + 0.12f) {
            best = t;
            break;
        }
    }

    // Octave-up check: with a weak fundamental, the dip at half the period (2nd harmonic)
    // can just scrape under the threshold although the true period's dip is far deeper.
    // A real period's double is about as deep as the period itself, so only move up
    // when the longer lag is several times better.
    {
        const int limit = full ? tauMax_ : tauAccept;
        int t = 2 * best;
        if (t + 2 < limit) {
            for (int j = t - 2; j <= t + 2; ++j)
                if (cmnd_[j] < cmnd_[t]) t = j;
            if (cmnd_[best] > 0.05f && cmnd_[t] < 0.25f * cmnd_[best]) best = t;
        }
    }

    // Parabolic interpolation around the minimum for sub-sample period accuracy.
    double tauRefined = best;
    if (best > 1 && best < tauMax) {
        const double s0 = cmnd_[best - 1], s1 = cmnd_[best], s2 = cmnd_[best + 1];
        const double denom = s0 - 2.0 * s1 + s2;
        if (std::abs(denom) > 1e-12) tauRefined = best + 0.5 * (s0 - s2) / denom;
    }

    if (best >= tauAccept && !full) return {};  // walked past what this window can vouch for
    PitchEstimate est;
    est.f0 = float(fs_ / tauRefined);
    est.aperiodicity = cmnd_[best];
    return est;
}

}  // namespace morf
