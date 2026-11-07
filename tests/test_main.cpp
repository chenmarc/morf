// Small self-contained test runner (no framework dependency).
#include <cmath>
#include <cstdio>
#include <vector>

#include "../io/audio_io.h"
#include "../synth/guitar_synth.h"
#include "morf/mono_tracker.h"
#include "morf/yin.h"

using namespace morf;

static int failures = 0;
#define CHECK(cond, ...)                                              \
    do {                                                              \
        if (!(cond)) {                                                \
            ++failures;                                               \
            std::printf("FAIL %s:%d: %s — ", __FILE__, __LINE__, #cond); \
            std::printf(__VA_ARGS__);                                 \
            std::printf("\n");                                        \
        }                                                             \
    } while (0)

static double cents(double a, double b) { return 1200.0 * std::log2(a / b); }

static std::vector<MidiEvent> track(const std::vector<float>& x, int block, MonoTrackerConfig cfg = {}) {
    MonoTracker tr(cfg);
    EventQueue q;
    std::vector<MidiEvent> all;
    for (size_t i = 0; i < x.size(); i += block) {
        q.clear();
        tr.process(x.data() + i, int(std::min<size_t>(block, x.size() - i)), q);
        all.insert(all.end(), q.begin(), q.end());
    }
    return all;
}

static void testYinSines() {
    const double fs = 48000;
    YinDetector yin(fs, 72, 1400);
    std::vector<float> buf(yin.bufferSize());
    for (double f : {73.4, 82.41, 110.0, 196.0, 329.6, 659.3, 1318.5}) {
        for (size_t i = 0; i < buf.size(); ++i) buf[i] = float(0.5 * std::sin(2 * M_PI * f * i / fs) + 0.2 * std::sin(4 * M_PI * f * i / fs));
        const auto pe = yin.analyze(buf.data());
        CHECK(std::abs(cents(pe.f0, f)) < 2.0, "f=%.1f got %.2f (%.2f cents)", f, pe.f0, cents(pe.f0, f));
        CHECK(pe.aperiodicity < 0.05f, "f=%.1f aperiodicity %.3f", f, pe.aperiodicity);
    }
}

static void testSynthTuning() {
    // The synthetic guitar must actually play in tune, or the benchmark is meaningless.
    const double fs = 48000;
    YinDetector yin(fs, 72, 1400);
    for (int midi : {40, 52, 64, 76, 86}) {
        NoteSpec n;
        n.midi = midi;
        n.onset = 0.0;
        n.offset = 1.0;
        RenderOptions ro;
        ro.noiseDb = -120;
        ro.humDb = -120;
        PitchTrack pt;
        const Audio a = renderGuitar({n}, ro, &pt);
        const auto pe = yin.analyze(a.samples.data() + int(0.5 * fs));
        const double f = 440.0 * std::pow(2.0, (midi - 69) / 12.0);
        CHECK(std::abs(cents(pe.f0, f)) < 3.0, "midi %d: synth plays %.2f Hz (%.2f cents off)", midi, pe.f0, cents(pe.f0, f));
        CHECK(!pt.frames[500].empty() && std::abs(pt.frames[500][0] - midi) < 0.01, "truth track at 0.5 s");
    }
}

static void testSingleNote() {
    NoteSpec n;
    n.midi = 45;
    n.onset = 0.2;
    n.offset = 1.0;
    RenderOptions ro;
    const Audio a = renderGuitar({n}, ro, nullptr);
    const auto ev = track(a.samples, 128);
    int ons = 0, offs = 0, note = -1;
    double onT = 0, offT = 0;
    for (const auto& e : ev) {
        if (e.isNoteOn()) { ++ons; note = e.data1; onT = e.time / 48000.0; }
        if (e.isNoteOff()) { ++offs; offT = e.time / 48000.0; }
    }
    CHECK(ons == 1 && note == 45, "expected one A2, got %d note-ons (last %d)", ons, note);
    CHECK(onT - 0.2 < 0.04, "Note On latency %.1f ms", (onT - 0.2) * 1000);
    CHECK(offs == 1 && offT - 1.0 < 0.05 && offT > 0.99, "Note Off at %.3f s", offT);
}

static void testBlockSizeInvariance() {
    std::vector<NoteSpec> notes(3);
    notes[0] = {0.1, 0.5, 52, 0.8f};
    notes[1] = {0.5, 0.9, 55, 0.6f};
    notes[2] = {1.0, 1.8, 64, 0.9f, false, {{0.2, 0.f}, {0.4, 2.f}, {0.6, 0.f}}};
    const Audio a = renderGuitar(notes, {}, nullptr);
    const auto ref = track(a.samples, 1);
    for (int block : {32, 64, 128, 333, 1024}) {
        const auto ev = track(a.samples, block);
        bool same = ev.size() == ref.size();
        for (size_t i = 0; same && i < ev.size(); ++i)
            same = ev[i].time == ref[i].time && ev[i].status == ref[i].status && ev[i].data1 == ref[i].data1 && ev[i].data2 == ref[i].data2;
        CHECK(same, "block %d gives different output (%zu vs %zu events)", block, ev.size(), ref.size());
    }
}

static void testBendTracking() {
    NoteSpec n{0.1, 1.5, 64, 0.9f};
    n.bend = {{0.3, 0.f}, {0.5, 2.f}, {0.9, 2.f}, {1.1, 0.f}};
    const Audio a = renderGuitar({n}, {}, nullptr);
    const auto ev = track(a.samples, 64);
    int ons = 0;
    float maxBend = 0;
    for (const auto& e : ev) {
        if (e.isNoteOn()) ++ons;
        if (e.isPitchBend()) maxBend = std::max(maxBend, (e.pitchBendValue() - 8192) / 8191.f * 12.f);
    }
    CHECK(ons == 1, "a bend must not retrigger the note (got %d note-ons)", ons);
    CHECK(std::abs(maxBend - 2.f) < 0.1f, "peak bend %.3f semitones", maxBend);
}

static void testMidiRoundTrip() {
    std::vector<MidiEvent> ev = {makeNoteOn(4800, 0, 60, 100), makePitchBend(9600, 0, 9000), makeNoteOff(48000, 0, 60)};
    CHECK(writeMidiFile("/tmp/morf_test.mid", ev, 48000, 12), "write midi");
    Audio a;
    a.samples = {0.f, 0.5f, -0.5f};
    CHECK(writeWav("/tmp/morf_test.wav", a), "write wav");
    Audio b;
    CHECK(readWav("/tmp/morf_test.wav", b) && b.samples.size() == 3 && b.samples[1] == 0.5f, "wav round trip");
}

int main() {
    testYinSines();
    testSynthTuning();
    testSingleNote();
    testBlockSizeInvariance();
    testBendTracking();
    testMidiRoundTrip();
    if (failures) { std::printf("%d check(s) failed\n", failures); return 1; }
    std::printf("all tests passed\n");
    return 0;
}
