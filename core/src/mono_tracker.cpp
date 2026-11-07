#include "morf/mono_tracker.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace morf {

float hzToMidi(float hz) { return 69.f + 12.f * std::log2(hz / 440.f); }
float midiToHz(float midi) { return 440.f * std::exp2((midi - 69.f) / 12.f); }

static int msToFrames(float ms, double fs, int hop) { return std::max(1, int(std::lround(ms * 0.001 * fs / hop))); }

MonoTracker::MonoTracker(const MonoTrackerConfig& cfg)
    : cfg_(cfg), yin_(cfg.sampleRate, cfg.minHz, cfg.maxHz, cfg.yinThreshold) {
    history_.assign(yin_.bufferSize(), 0.f);
    refractoryFrames_ = msToFrames(cfg_.onsetRefractoryMs, cfg_.sampleRate, cfg_.hop);
    unvoicedLimitFrames_ = msToFrames(cfg_.maxUnvoicedMs, cfg_.sampleRate, cfg_.hop);
    pendingLimitFrames_ = msToFrames(cfg_.pendingOnsetMs, cfg_.sampleRate, cfg_.hop);
    muteFrames_ = msToFrames(cfg_.muteWindowMs, cfg_.sampleRate, cfg_.hop);
    dbHistory_.assign(muteFrames_ + 1, -120.f);
    slowAlpha_ = float(std::exp(-double(cfg_.hop) / (0.025 * cfg_.sampleRate)));
    reset();
}

void MonoTracker::reset() {
    std::fill(history_.begin(), history_.end(), 0.f);
    std::fill(dbHistory_.begin(), dbHistory_.end(), -120.f);
    dbPos_ = 0;
    hopFill_ = 0;
    samplePos_ = 0;
    segmentStart_ = 0;
    prevSample_ = 0.f;
    hopDiffEnergy_ = slowDiffEnergy_ = 0.f;
    framesSinceOnset_ = 1 << 20;
    active_ = false;
    note_ = -1;
    bend_ = 0.f;
    peakDb_ = -120.f;
    unvoiced_ = 0;
    pendingOnset_ = false;
    pendingAge_ = 0;
    needOnset_ = false;
    candCount_ = onTrackCount_ = 0;
    frame_ = {};
    prevF0Midi_[0] = -100.f;
    stableRun_ = 0;
}

void MonoTracker::process(const float* in, int n, EventQueue& out) {
    const int H = cfg_.hop;
    const int B = int(history_.size());
    for (int i = 0; i < n; ++i) {
        const float x = in[i];
        const float d = x - prevSample_;
        prevSample_ = x;
        hopDiffEnergy_ += d * d;
        history_[B - H + hopFill_] = x;  // the tail H slots are the hop being filled
        ++samplePos_;
        if (++hopFill_ == H) {
            hopDiffEnergy_ /= float(H);
            analyseFrame(out);
            // Shift history left by one hop to make room for the next.
            std::memmove(history_.data(), history_.data() + H, sizeof(float) * (B - H));
            hopFill_ = 0;
            hopDiffEnergy_ = 0.f;
        }
    }
}

void MonoTracker::analyseFrame(EventQueue& out) {
    const int B = int(history_.size());

    // Short-term level over the most recent 5 ms.
    const int levelN = std::min(B, int(0.005 * cfg_.sampleRate));
    double sq = 0.0;
    for (int i = B - levelN; i < B; ++i) sq += double(history_[i]) * history_[i];
    const float levelDb = float(10.0 * std::log10(sq / levelN + 1e-14));

    // Onset: energy of the first difference (emphasises the pick attack) jumps
    // well above its recent average.
    ++framesSinceOnset_;
    bool onset = false;
    if (levelDb > cfg_.gateOnDb && hopDiffEnergy_ > cfg_.onsetRatio * slowDiffEnergy_ &&
        framesSinceOnset_ > refractoryFrames_) {
        onset = true;
        framesSinceOnset_ = 0;
        segmentStart_ = samplePos_ - cfg_.hop;
    }
    slowDiffEnergy_ = slowAlpha_ * slowDiffEnergy_ + (1.f - slowAlpha_) * hopDiffEnergy_;

    // Level `muteWindowMs` ago, for detecting a fast mute.
    const float dbAgo = dbHistory_[dbPos_];
    dbHistory_[dbPos_] = levelDb;
    dbPos_ = (dbPos_ + 1) % int(dbHistory_.size());

    PitchEstimate pe;
    if (levelDb > cfg_.gateOffDb) {
        const int len = int(std::min<int64_t>(B, samplePos_ - segmentStart_));
        pe = yin_.analyze(history_.data() + (B - len), len);
    }
    const float gate = active_ ? cfg_.gateOffDb : cfg_.gateOnDb;
    // Stability: how many consecutive frames the raw estimate stayed within 0.3 semitones.
    const float rawMidi = pe.f0 > 0.f ? hzToMidi(pe.f0) : -100.f;
    if (pe.f0 > 0.f && pe.aperiodicity < cfg_.stableAperiodicity && std::abs(rawMidi - prevF0Midi_[0]) < 0.3f) ++stableRun_;
    else stableRun_ = 0;
    prevF0Midi_[0] = rawMidi;
    const bool periodic = pe.aperiodicity < cfg_.confidentAperiodicity ||
                          (pe.aperiodicity < cfg_.stableAperiodicity && stableRun_ + 1 >= cfg_.stableFrames);
    const bool voiced = pe.f0 > 0.f && periodic && levelDb > gate;

    frame_ = {pe.f0, pe.aperiodicity, levelDb, voiced, onset};

    if (!active_) {
        if (needOnset_ && (onset || levelDb < cfg_.gateOffDb)) needOnset_ = false;
        if (levelDb > cfg_.gateOnDb) peakDb_ = std::max(peakDb_, levelDb);
        else peakDb_ = -120.f;
        if (needOnset_ || !voiced) { candCount_ = 0; return; }
        const float mf = hzToMidi(pe.f0);
        if (candCount_ > 0 && std::abs(mf - candMidi_) < 0.5f) ++candCount_;
        else candCount_ = 1;
        candMidi_ = mf;
        if (candCount_ >= cfg_.confirmFrames) startNote(mf, out);
        return;
    }

    // --- A note is sounding ---
    peakDb_ = std::max(peakDb_, levelDb);
    if (levelDb < std::max(cfg_.gateOffDb, peakDb_ - cfg_.relativeOffDb)) { stopNote(out); return; }
    if (levelDb - dbAgo < -cfg_.muteDropDb) {  // string choked / muted
        stopNote(out);
        needOnset_ = true;
        return;
    }
    if (onset) {
        pendingOnset_ = true;
        pendingAge_ = 0;
        candCount_ = 0;
        onTrackCount_ = 0;
        peakDb_ = levelDb;
    }
    if (pendingOnset_ && ++pendingAge_ > pendingLimitFrames_) pendingOnset_ = false;

    if (!voiced) {
        if (++unvoiced_ > unvoicedLimitFrames_) stopNote(out);
        return;
    }
    unvoiced_ = 0;

    float mf = hzToMidi(pe.f0);
    const float current = float(note_) + bend_;
    // Bends and vibrato: a long window averages the pitch over ~28 ms and lags behind.
    // Once we know the note, re-measure over just a few of its periods and use that
    // if it agrees with where the note is.
    {
        const int segLen = int(std::min<int64_t>(B, samplePos_ - segmentStart_));
        const int shortLen = std::min(segLen, int(cfg_.trackingPeriods * cfg_.sampleRate / midiToHz(current) * 1.2f));
        if (shortLen < segLen) {
            const PitchEstimate ps = yin_.analyze(history_.data() + (B - shortLen), shortLen);
            if (ps.f0 > 0.f && ps.aperiodicity < cfg_.confidentAperiodicity && std::abs(hzToMidi(ps.f0) - current) < cfg_.jumpSemitones)
                mf = hzToMidi(ps.f0);
        }
    }
    const float dist = std::abs(mf - current);

    if (dist > cfg_.jumpSemitones) {
        // Off the current pitch: either a new note (legato or picked) or an octave glitch.
        if (candCount_ > 0 && std::abs(mf - candMidi_) < 0.3f) ++candCount_;
        else candCount_ = 1;
        candMidi_ = mf;
        onTrackCount_ = 0;
        const bool octaveLike = std::abs(dist - 12.f) < 0.6f || std::abs(dist - 19.f) < 0.6f || std::abs(dist - 24.f) < 0.6f;
        const int need = octaveLike ? cfg_.octaveConfirmFrames : cfg_.confirmFrames;
        // Switching notes without a pick attack (hammer-on, pull-off, slide) needs a
        // clean, strong signal; near the noise floor hum and decay give unstable estimates.
        const bool cleanEnough = pendingOnset_ || (levelDb > cfg_.gateOnDb && pe.aperiodicity < cfg_.confidentAperiodicity);
        if (candCount_ >= need && cleanEnough) {
            if (int(std::lround(mf)) != note_) {
                const int vel = pendingOnset_ ? -1 : velocity_;  // legato notes keep the picked velocity
                stopNote(out);
                startNote(mf, out, vel);
            } else {
                sendBend(mf - float(note_), false, out);
            }
            candCount_ = 0;
            pendingOnset_ = false;
        }
        return;
    }

    candCount_ = 0;
    ++onTrackCount_;
    // Re-pick of the same note: an onset, followed by ~10 ms that stays on the
    // same pitch (long enough for the YIN window to have seen a different note).
    if (pendingOnset_ && onTrackCount_ >= std::max(cfg_.confirmFrames, msToFrames(10.f, cfg_.sampleRate, cfg_.hop))) {
        stopNote(out);
        startNote(mf, out);
        pendingOnset_ = false;
        return;
    }
    sendBend(mf - float(note_), false, out);
}

void MonoTracker::startNote(float mf, EventQueue& out, int velocity) {
    note_ = std::clamp(int(std::lround(mf)), 0, 127);
    peakDb_ = std::max(peakDb_, frame_.levelDb);
    const float t = (peakDb_ - cfg_.gateOnDb) / (-6.f - cfg_.gateOnDb);
    const int vel = velocity > 0 ? velocity : std::clamp(int(std::lround(1 + 126 * t)), 1, 127);
    velocity_ = vel;
    sendBend(mf - float(note_), true, out);
    out.push(makeNoteOn(samplePos_, cfg_.channel, note_, vel));
    active_ = true;
    unvoiced_ = 0;
    candCount_ = onTrackCount_ = 0;
    pendingOnset_ = false;
}

void MonoTracker::sendBend(float semis, bool force, EventQueue& out) {
    semis = std::clamp(semis, -cfg_.bendRange, cfg_.bendRange);
    if (!force && std::abs(semis - bend_) * 100.f < cfg_.bendStepCents) return;
    bend_ = semis;
    const int v = std::clamp(8192 + int(std::lround(semis / cfg_.bendRange * 8191.f)), 0, 16383);
    out.push(makePitchBend(samplePos_, cfg_.channel, v));
}

void MonoTracker::stopNote(EventQueue& out) {
    if (active_) out.push(makeNoteOff(samplePos_, cfg_.channel, note_));
    active_ = false;
    candCount_ = onTrackCount_ = 0;
    pendingOnset_ = false;
    peakDb_ = -120.f;
}

}  // namespace morf
