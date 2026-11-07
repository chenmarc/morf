#include "evaluate.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>

namespace morf {

namespace {

struct EstNote {
    int midi, ch;
    double on, off;
    bool used = false;
};

double percentile(std::vector<double> v, double p) {
    if (v.empty()) return NAN;
    std::sort(v.begin(), v.end());
    const double idx = p * (v.size() - 1);
    const size_t lo = size_t(std::floor(idx)), hi = size_t(std::ceil(idx));
    return v[lo] + (v[hi] - v[lo]) * (idx - lo);
}

std::string fmt(const char* f, double v) {
    if (std::isnan(v)) return "–";
    char b[64];
    std::snprintf(b, sizeof b, f, v);
    return b;
}

}  // namespace

CaseResult evaluateCase(const std::vector<NoteSpec>& truthNotes, const PitchTrack* truthPitch,
                        const std::vector<MidiEvent>& events, double fs, double audioSeconds, const EvalOptions& opt) {
    CaseResult r;
    r.audioSeconds = audioSeconds;

    // Rebuild estimated notes from the MIDI stream.
    std::vector<EstNote> est;
    std::map<std::pair<int, int>, size_t> open;  // (channel, note) -> index in est
    for (const auto& e : events) {
        const double t = double(e.time) / fs;
        const auto key = std::make_pair(e.channel(), int(e.data1));
        if (e.isNoteOn()) {
            if (auto it = open.find(key); it != open.end()) est[it->second].off = t;
            open[key] = est.size();
            est.push_back({e.data1, e.channel(), t, audioSeconds});
        } else if (e.isNoteOff()) {
            if (auto it = open.find(key); it != open.end()) {
                est[it->second].off = t;
                open.erase(it);
            }
        }
    }
    r.estNotes = int(est.size());
    r.truthNotes = int(truthNotes.size());

    std::vector<size_t> order(truthNotes.size());
    for (size_t i = 0; i < order.size(); ++i) order[i] = i;
    std::sort(order.begin(), order.end(), [&](size_t a, size_t b) { return truthNotes[a].onset < truthNotes[b].onset; });
    for (size_t ti : order) {
        const auto& tn = truthNotes[ti];
        NoteResult nr{tn.midi, tn.onset, tn.offset};
        EstNote* best = nullptr;
        for (auto& en : est) {
            if (en.used || en.midi != tn.midi) continue;
            if (en.on < tn.onset - opt.onsetEarlyTolerance || en.on > tn.onset + opt.onsetLateTolerance) continue;
            if (!best || en.on < best->on) best = &en;
        }
        if (best) {
            best->used = true;
            nr.matched = true;
            nr.latency = best->on - tn.onset;
            nr.offLatency = best->off - tn.offset;
            ++r.matched;
        }
        r.notes.push_back(nr);
    }
    for (const auto& en : est) {
        if (en.used) continue;
        for (const auto& tn : truthNotes) {
            const int d = std::abs(en.midi - tn.midi);
            if ((d == 12 || d == 19 || d == 24) && en.on >= tn.onset - opt.onsetEarlyTolerance && en.on <= tn.offset) {
                ++r.octaveErrors;
                break;
            }
        }
    }

    // Continuous pitch accuracy, sampled every truth frame.
    if (truthPitch) {
        std::map<int, float> bendByCh;
        std::map<std::pair<int, int>, bool> sounding;
        size_t ei = 0;
        for (size_t fi = 0; fi < truthPitch->frames.size(); ++fi) {
            const double t = fi * truthPitch->hopSeconds;
            while (ei < events.size() && double(events[ei].time) / fs <= t) {
                const auto& e = events[ei++];
                if (e.isPitchBend()) bendByCh[e.channel()] = float(e.pitchBendValue() - 8192) / 8191.f * opt.bendRange;
                else if (e.isNoteOn()) sounding[{e.channel(), e.data1}] = true;
                else if (e.isNoteOff()) sounding.erase({e.channel(), e.data1});
            }
            const auto& tf = truthPitch->frames[fi];
            if (tf.size() != 1 || sounding.size() != 1) continue;
            const auto& [ch, note] = sounding.begin()->first;
            const float estPitch = float(note) + bendByCh[ch];
            r.pitchErrorCents.push_back(100.f * (estPitch - tf[0]));
        }
    }
    return r;
}

std::string summarise(const std::vector<CaseResult>& results, const std::string& title) {
    std::string s = "### " + title + "\n\n";
    s += "| Category | Notes | Detected | Precision | Recall | F1 | Latency median | Latency p90 | Note-off lag median | Pitch error median | Within 10 cents | Octave errors |\n";
    s += "|---|---|---|---|---|---|---|---|---|---|---|---|\n";
    std::vector<std::string> cats;
    for (const auto& r : results)
        if (std::find(cats.begin(), cats.end(), r.category) == cats.end()) cats.push_back(r.category);
    cats.push_back("ALL (excl. chords)");

    std::vector<double> regLat[3];
    double audio = 0, cpu = 0;
    for (const auto& cat : cats) {
        const bool all = cat.rfind("ALL", 0) == 0;
        int truth = 0, estN = 0, matched = 0, oct = 0;
        std::vector<double> lat, offLat, pe;
        int within10 = 0;
        for (const auto& r : results) {
            if (all ? r.category == "chords" : r.category != cat) continue;
            truth += r.truthNotes; estN += r.estNotes; matched += r.matched; oct += r.octaveErrors;
            for (const auto& n : r.notes) {
                if (!n.matched) continue;
                lat.push_back(n.latency * 1000);
                offLat.push_back(n.offLatency * 1000);
                if (all) regLat[n.midi < 53 ? 0 : n.midi <= 68 ? 1 : 2].push_back(n.latency * 1000);
            }
            for (float c : r.pitchErrorCents) {
                pe.push_back(std::abs(c));
                if (std::abs(c) <= 10.f) ++within10;
            }
            if (all) { audio += r.audioSeconds; cpu += r.cpuSeconds; }
        }
        const double P = estN ? double(matched) / estN : NAN, R = truth ? double(matched) / truth : NAN;
        const double F = (P + R > 0) ? 2 * P * R / (P + R) : 0;
        s += "| " + cat + " | " + std::to_string(truth) + " | " + std::to_string(estN) + " | " + fmt("%.2f", P) + " | " +
             fmt("%.2f", R) + " | " + fmt("%.2f", F) + " | " + fmt("%.1f ms", percentile(lat, 0.5)) + " | " +
             fmt("%.1f ms", percentile(lat, 0.9)) + " | " + fmt("%.1f ms", percentile(offLat, 0.5)) + " | " +
             fmt("%.1f c", percentile(pe, 0.5)) + " | " + fmt("%.0f%%", pe.empty() ? NAN : 100.0 * within10 / pe.size()) +
             " | " + std::to_string(oct) + " |\n";
    }
    s += "\nNote On latency by register (all single-note categories):\n\n";
    s += "| Register | Notes | Median | p90 | Max |\n|---|---|---|---|---|\n";
    const char* names[3] = {"Low (E2–E3, 82–165 Hz)", "Mid (F3–G#4)", "High (A4 and up, 440 Hz+)"};
    for (int i = 0; i < 3; ++i) {
        double mx = regLat[i].empty() ? NAN : *std::max_element(regLat[i].begin(), regLat[i].end());
        s += std::string("| ") + names[i] + " | " + std::to_string(regLat[i].size()) + " | " +
             fmt("%.1f ms", percentile(regLat[i], 0.5)) + " | " + fmt("%.1f ms", percentile(regLat[i], 0.9)) + " | " +
             fmt("%.1f ms", mx) + " |\n";
    }
    if (audio > 0) s += "\nCPU: " + fmt("%.2f%%", 100.0 * cpu / audio) + " of one core (single-note categories).\n";
    return s;
}

}  // namespace morf
