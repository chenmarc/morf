#include "test_set.h"

#include <algorithm>
#include <cstdio>
#include <random>

namespace morf {

namespace {

struct Gen {
    std::mt19937 rng;
    double u(double a, double b) { return std::uniform_real_distribution<double>(a, b)(rng); }
    int i(int a, int b) { return std::uniform_int_distribution<int>(a, b)(rng); }
    bool coin(double p) { return u(0, 1) < p; }
};

// Standard tuning open strings, low to high.
constexpr int kOpen[6] = {40, 45, 50, 55, 59, 64};

// Spread notes evenly over low (E2-E3), mid and high registers.
int randomNote(Gen& g) {
    switch (g.i(0, 2)) {
        case 0: return g.i(40, 52);
        case 1: return g.i(53, 68);
        default: return g.i(69, 86);
    }
}

std::vector<NoteSpec> singles(Gen& g) {
    std::vector<NoteSpec> v;
    double t = 0.3;
    for (int k = 0; k < 12; ++k) {
        NoteSpec n;
        n.midi = randomNote(g);
        n.velocity = float(g.u(0.3, 1.0));
        n.onset = t;
        n.offset = t + g.u(0.3, 0.9);
        v.push_back(n);
        t = n.offset + g.u(0.15, 0.4);
    }
    return v;
}

// A single-string line where each new pick (or hammer-on) cuts the previous note.
std::vector<NoteSpec> legato(Gen& g) {
    std::vector<NoteSpec> v;
    double t = 0.3;
    int note = g.i(45, 72);
    for (int k = 0; k < 24; ++k) {
        NoteSpec n;
        n.midi = note;
        n.velocity = float(g.u(0.4, 0.95));
        n.onset = t;
        const double dur = g.u(0.08, 0.22);
        n.offset = t + dur;
        v.push_back(n);
        t += dur;
        int step;
        do { step = g.i(-4, 4); } while (step == 0);
        note = std::clamp(note + step, 40, 86);
        if (k % 8 == 7) t += g.u(0.2, 0.4), v.back().offset += 0.1;  // phrase gap
    }
    return v;
}

std::vector<NoteSpec> repeats(Gen& g) {
    std::vector<NoteSpec> v;
    double t = 0.3;
    for (int phrase = 0; phrase < 4; ++phrase) {
        const int note = randomNote(g);
        const double ioi = 60.0 / g.u(100, 170) / 2.0;  // 8th notes
        for (int k = 0; k < 6; ++k) {
            NoteSpec n;
            n.midi = note;
            n.velocity = float(g.u(0.5, 1.0));
            n.onset = t;
            n.offset = t + ioi;
            v.push_back(n);
            t += ioi;
        }
        v.back().offset = t + 0.2;
        t += 0.5;
    }
    return v;
}

std::vector<NoteSpec> bends(Gen& g) {
    std::vector<NoteSpec> v;
    double t = 0.3;
    for (int k = 0; k < 6; ++k) {
        NoteSpec n;
        n.midi = g.i(52, 81);  // bends happen mostly on the G/B/E strings
        n.velocity = float(g.u(0.6, 1.0));
        n.onset = t;
        const float amount = g.coin(0.5) ? 2.f : (g.coin(0.5) ? 1.f : 3.f);
        const double hold = g.u(0.1, 0.3), rise = g.u(0.12, 0.3), sustain = g.u(0.2, 0.4), fall = g.u(0.15, 0.3);
        n.bend = {{hold, 0.f}, {hold + rise, amount}, {hold + rise + sustain, amount}, {hold + rise + sustain + fall, 0.f}};
        n.offset = t + hold + rise + sustain + fall + g.u(0.1, 0.3);
        v.push_back(n);
        t = n.offset + g.u(0.2, 0.4);
    }
    return v;
}

std::vector<NoteSpec> vibratos(Gen& g) {
    std::vector<NoteSpec> v;
    double t = 0.3;
    for (int k = 0; k < 6; ++k) {
        NoteSpec n;
        n.midi = g.i(50, 84);
        n.velocity = float(g.u(0.5, 1.0));
        n.onset = t;
        n.vibratoDepth = float(g.u(0.1, 0.45));
        n.vibratoHz = float(g.u(4.5, 7.0));
        n.vibratoStart = g.u(0.15, 0.3);
        n.offset = t + g.u(0.8, 1.4);
        v.push_back(n);
        t = n.offset + g.u(0.2, 0.4);
    }
    return v;
}

// Palm-muted chugs on the low strings.
std::vector<NoteSpec> palm(Gen& g) {
    std::vector<NoteSpec> v;
    double t = 0.3;
    for (int phrase = 0; phrase < 4; ++phrase) {
        const double ioi = 60.0 / g.u(90, 160) / (g.coin(0.5) ? 2.0 : 4.0);
        int note = kOpen[g.i(0, 1)] + g.i(0, 5);
        for (int k = 0; k < 8; ++k) {
            NoteSpec n;
            n.midi = note;
            n.palmMute = true;
            n.velocity = float(g.u(0.5, 1.0));
            n.onset = t;
            n.offset = t + ioi;
            v.push_back(n);
            t += ioi;
            if (g.coin(0.25)) note = kOpen[g.i(0, 1)] + g.i(0, 5);
        }
        v.back().offset = t + 0.1;
        t += 0.5;
    }
    return v;
}

// Strummed chords: one note per string, 10-35 ms spread. The mono baseline can't do these;
// they're here so step 3 (polyphonic) has a scoreboard from day one.
std::vector<NoteSpec> chords(Gen& g) {
    std::vector<NoteSpec> v;
    double t = 0.3;
    for (int k = 0; k < 6; ++k) {
        const int lowString = g.i(0, 2), nStrings = g.i(3, 6 - lowString);
        const int fret = g.i(0, 9);
        const double spread = g.u(0.01, 0.035);
        const double dur = g.u(0.6, 1.2);
        const bool down = g.coin(0.7);
        for (int s = 0; s < nStrings; ++s) {
            const int str = lowString + s;
            NoteSpec n;
            n.midi = kOpen[str] + std::max(0, fret + g.i(-2, 2));
            n.velocity = float(g.u(0.5, 0.9));
            const int order = down ? s : nStrings - 1 - s;
            n.onset = t + spread * order / std::max(1, nStrings - 1);
            n.offset = t + dur;
            v.push_back(n);
        }
        t += dur + g.u(0.2, 0.4);
    }
    std::sort(v.begin(), v.end(), [](const NoteSpec& a, const NoteSpec& b) { return a.onset < b.onset; });
    return v;
}

}  // namespace

std::vector<TestCase> makeSyntheticTestSet(uint32_t seed, double sampleRate, int filesPerCategory) {
    struct Cat { const char* name; std::vector<NoteSpec> (*fn)(Gen&); };
    const Cat cats[] = {{"single", singles}, {"legato", legato}, {"repeat", repeats}, {"bends", bends},
                        {"vibrato", vibratos}, {"palm", palm}, {"chords", chords}};
    std::vector<TestCase> out;
    Gen g{std::mt19937(seed)};
    for (const auto& c : cats) {
        for (int f = 0; f < filesPerCategory; ++f) {
            TestCase tc;
            char buf[64];
            std::snprintf(buf, sizeof buf, "%s_%02d", c.name, f);
            tc.name = buf;
            tc.category = c.name;
            tc.notes = c.fn(g);
            RenderOptions ro;
            ro.sampleRate = sampleRate;
            ro.seed = g.i(1, 1 << 30);
            ro.peakDb = float(g.u(-18, -3));  // interfaces get set up with very different gain
            tc.audio = renderGuitar(tc.notes, ro, &tc.truth);
            out.push_back(std::move(tc));
        }
    }
    return out;
}

}  // namespace morf
