#include "guitar_synth.h"

#include <algorithm>
#include <cmath>
#include <complex>
#include <random>

namespace morf {

namespace {

constexpr double kPi = 3.141592653589793;

class DelayLine {
public:
    static constexpr int kSize = 8192, kMask = kSize - 1;
    void write(float x) { buf_[w_ & kMask] = x; ++w_; }
    // Value written `delay` samples before the next write (delay >= 2), 3rd-order Lagrange.
    float read(double delay) const {
        const int k = int(std::floor(delay)) - 1;
        const double t = delay - k;  // in [1, 2)
        const double h0 = -(t - 1) * (t - 2) * (t - 3) / 6.0;
        const double h1 = t * (t - 2) * (t - 3) / 2.0;
        const double h2 = -t * (t - 1) * (t - 3) / 2.0;
        const double h3 = t * (t - 1) * (t - 2) / 6.0;
        auto at = [&](int m) { return double(buf_[(w_ - m) & kMask]); };
        return float(h0 * at(k) + h1 * at(k + 1) + h2 * at(k + 2) + h3 * at(k + 3));
    }

private:
    float buf_[kSize] = {};
    int64_t w_ = 0;
};

// Phase delay (samples) of the loop filter (1-s) + s z^-1 at radian frequency w.
double loopFilterDelay(double s, double w) {
    return std::atan2(s * std::sin(w), (1.0 - s) + s * std::cos(w)) / w;
}

}  // namespace

float bendAt(const NoteSpec& n, double t) {
    float b = 0.f;
    if (!n.bend.empty()) {
        if (t <= n.bend.front().first) b = n.bend.front().second;
        else if (t >= n.bend.back().first) b = n.bend.back().second;
        else {
            for (size_t i = 1; i < n.bend.size(); ++i) {
                if (t <= n.bend[i].first) {
                    const auto& a = n.bend[i - 1];
                    const auto& c = n.bend[i];
                    const double u = (t - a.first) / std::max(1e-9, c.first - a.first);
                    b = float(a.second + u * (c.second - a.second));
                    break;
                }
            }
        }
    }
    if (n.vibratoDepth > 0.f && t > n.vibratoStart) {
        const double ramp = std::min(1.0, (t - n.vibratoStart) / 0.15);
        b += float(n.vibratoDepth * ramp * std::sin(2 * kPi * n.vibratoHz * (t - n.vibratoStart)));
    }
    return b;
}

Audio renderGuitar(const std::vector<NoteSpec>& notes, const RenderOptions& opt, PitchTrack* truth) {
    const double fs = opt.sampleRate;
    double end = 0;
    for (const auto& n : notes) end = std::max(end, n.offset);
    end += opt.tailSeconds;
    Audio out;
    out.sampleRate = fs;
    out.samples.assign(size_t(end * fs) + 1, 0.f);
    if (truth) {
        truth->frames.assign(size_t(end / truth->hopSeconds) + 1, {});
    }

    std::mt19937 rng(opt.seed);
    std::uniform_real_distribution<float> uni(-1.f, 1.f);
    std::uniform_real_distribution<double> u01(0.0, 1.0);

    for (const auto& n : notes) {
        const double f0 = 440.0 * std::pow(2.0, (n.midi - 69) / 12.0);
        const double pickPos = 0.08 + 0.2 * u01(rng);    // fraction of string length
        const double pickupPos = 0.1 + 0.15 * u01(rng);
        const float vel = std::clamp(n.velocity, 0.05f, 1.f);
        // Brighter, longer-ringing for harder picks; dark and short for palm mutes.
        const double s = n.palmMute ? 0.5 : 0.5 - 0.35 * vel;
        const double t60Ring = n.palmMute ? 0.12 + 0.1 * u01(rng) : std::clamp(12.0 * std::pow(82.0 / f0, 0.5), 2.5, 15.0);
        const double t60Muted = 0.025;
        // Pick "glide": strings start slightly sharp from the extra tension of a hard pick.
        const double glideCents = 6.0 + 10.0 * vel;
        const double glideTau = 0.04;

        DelayLine line;
        float prevLoop = 0.f;
        // Excitation: one period of lowpassed noise, combed by pick position.
        const int period = int(fs / f0);
        std::vector<float> exc(period + 1, 0.f);
        {
            const float lp = n.palmMute ? 0.85f : 0.6f - 0.5f * vel;  // one-pole coefficient
            float st = 0.f;
            for (auto& e : exc) { st = lp * st + (1.f - lp) * uni(rng); e = st; }
            const int pd = std::max(1, int(pickPos * period));
            for (int i = int(exc.size()) - 1; i >= pd; --i) exc[i] -= exc[i - pd];
            float peak = 1e-9f;
            for (float e : exc) peak = std::max(peak, std::abs(e));
            for (auto& e : exc) e *= vel / peak;
        }

        const int64_t start = int64_t(n.onset * fs);
        const int64_t muteAt = int64_t(n.offset * fs);
        const int64_t stop = std::min<int64_t>(int64_t(out.samples.size()), muteAt + int64_t(0.3 * fs));
        for (int64_t i = start; i < stop; ++i) {
            const double t = double(i - start) / fs;
            const double semis = bendAt(n, t) + glideCents / 100.0 * std::exp(-t / glideTau);
            const double f = f0 * std::pow(2.0, semis / 12.0);
            const double w = 2 * kPi * f / fs;
            const double delay = std::max(2.0, fs / f - loopFilterDelay(s, w));
            const double t60 = i < muteAt ? t60Ring : t60Muted;
            // Loop gain so the fundamental decays with exactly this T60 (compensating the filter's own loss).
            const double filterMag = std::abs(std::complex<double>((1.0 - s) + s * std::cos(w), -s * std::sin(w)));
            const double g = std::pow(10.0, -3.0 / (t60 * f)) / filterMag;
            const float y = line.read(delay);
            const float loop = float(g * ((1.0 - s) * y + s * prevLoop));
            prevLoop = y;
            const int64_t k = i - start;
            const float e = k < int64_t(exc.size()) ? exc[size_t(k)] : 0.f;
            line.write(loop + e);
            // Pickup sees the string at a point: approximate with a comb on the travelling wave.
            const float sample = (loop + e) - line.read(std::max(2.0, pickupPos * fs / f));
            out.samples[size_t(i)] += sample;
            if (truth && i < muteAt) {
                const int64_t spf = std::max<int64_t>(1, std::llround(truth->hopSeconds * fs));
                if (i % spf == 0 && size_t(i / spf) < truth->frames.size())
                    truth->frames[size_t(i / spf)].push_back(float(n.midi + semis));
            }
        }
    }

    // Normalise, then add noise floor and hum (so the noise level is fixed relative to full scale).
    float peak = 1e-9f;
    for (float v : out.samples) peak = std::max(peak, std::abs(v));
    const float gain = std::pow(10.f, opt.peakDb / 20.f) / peak;
    const float noiseAmp = std::pow(10.f, opt.noiseDb / 20.f) * std::sqrt(3.f);
    const float humAmp = std::pow(10.f, opt.humDb / 20.f) * std::sqrt(2.f);
    for (size_t i = 0; i < out.samples.size(); ++i) {
        const double t = double(i) / fs;
        const double hum = std::sin(2 * kPi * 60 * t) + 0.5 * std::sin(2 * kPi * 180 * t + 0.3);
        out.samples[i] = out.samples[i] * gain + noiseAmp * uni(rng) + float(humAmp * hum);
    }
    return out;
}

}  // namespace morf
