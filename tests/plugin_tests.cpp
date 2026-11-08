// Drives the real plugin processor the way a host would: odd block sizes,
// stereo input, and checks the MIDI it outputs matches the engine on its own.
// Also simulates Ableton Live's unrouted VST3 side-chain, which feeds the
// plugin's previous output back in as input.
#include <cstdio>
#include <vector>

#include "PluginProcessor.h"
#include "morf/bench/guitar_synth.h"

int main() {
    juce::ScopedJuceInitialiser_GUI init;
    using namespace morf;
    std::vector<NoteSpec> notes = {{0.2, 0.7, 40, 0.9f}, {0.9, 1.4, 52, 0.7f}, {1.6, 2.6, 64, 0.8f, false, {{0.2, 0.f}, {0.4, 2.f}, {0.7, 0.f}}}};
    const double fs = 48000;
    const Audio a = renderGuitar(notes, {}, nullptr);

    std::unique_ptr<juce::AudioProcessor> plugin(createPluginFilter());
    juce::AudioProcessor::BusesLayout layout;
    layout.inputBuses.add(juce::AudioChannelSet::stereo());
    layout.outputBuses.add(juce::AudioChannelSet::stereo());
    if (!plugin->setBusesLayout(layout)) { std::printf("FAIL: stereo layout rejected\n"); return 1; }
    const int maxBlock = 512;
    plugin->setRateAndBufferSizeDetails(fs, maxBlock);
    plugin->prepareToPlay(fs, maxBlock);

    std::vector<MidiEvent> got;
    juce::AudioBuffer<float> buf(2, maxBlock);
    juce::MidiBuffer midi;
    const int sizes[] = {512, 100, 333, 64, 7};
    size_t pos = 0;
    int k = 0, rpn = 0;
    float outPeak = 0.f;
    while (pos < a.samples.size()) {
        const int n = int(std::min<size_t>(sizes[k++ % 5], a.samples.size() - pos));
        buf.setSize(2, n, false, false, true);
        for (int c = 0; c < 2; ++c) buf.copyFrom(c, 0, a.samples.data() + pos, n);
        midi.clear();
        plugin->processBlock(buf, midi);
        outPeak = std::max(outPeak, buf.getMagnitude(0, 0, n));
        for (const auto m : midi) {
            const auto msg = m.getMessage();
            if (msg.isController()) { ++rpn; continue; }
            const auto* d = msg.getRawData();
            got.push_back({int64_t(pos) + m.samplePosition, d[0], d[1], msg.getRawDataSize() > 2 ? d[2] : uint8_t(0)});
        }
        pos += size_t(n);
    }

    // Reference: the engine alone, with the same settings and the input mixed to mono.
    MonoTrackerConfig cfg;
    cfg.sampleRate = fs;
    MonoTracker ref(cfg);
    EventQueue q;
    std::vector<MidiEvent> want;
    for (size_t i = 0; i < a.samples.size(); i += 256) {
        q.clear();
        ref.process(a.samples.data() + i, int(std::min<size_t>(256, a.samples.size() - i)), q);
        want.insert(want.end(), q.begin(), q.end());
    }

    int failures = 0, ons = 0;
    if (got.size() != want.size()) { std::printf("FAIL: %zu events, engine gave %zu\n", got.size(), want.size()); ++failures; }
    for (size_t i = 0; i < std::min(got.size(), want.size()); ++i) {
        const int64_t expectTime = want[i].time;  // plugin places events at the detection sample
        if (got[i].status != want[i].status || got[i].data1 != want[i].data1 || std::llabs(got[i].time - expectTime) > 1) {
            std::printf("FAIL: event %zu differs (time %lld vs %lld)\n", i, (long long)got[i].time, (long long)expectTime);
            ++failures;
            break;
        }
        if (got[i].isNoteOn()) ++ons;
    }
    if (ons != 3) { std::printf("FAIL: %d note-ons, expected 3\n", ons); ++failures; }
    if (rpn != 4) { std::printf("FAIL: expected one bend-range RPN (4 CCs), got %d\n", rpn); ++failures; }
    if (outPeak < 0.01f) { std::printf("FAIL: monitor synth is silent\n"); ++failures; }

    // Echo test: one guitar note, then the host keeps handing back our own output as input.
    // morf must not track its own synth: the note has to end and no new notes may start.
    {
        std::unique_ptr<juce::AudioProcessor> p2(createPluginFilter());
        p2->setBusesLayout(layout);
        p2->setRateAndBufferSizeDetails(fs, maxBlock);
        p2->prepareToPlay(fs, maxBlock);
        const Audio one = renderGuitar({{0.1, 0.6, 57, 0.9f}}, {}, nullptr);
        const int bs = 256;
        juce::AudioBuffer<float> b(2, bs);
        int onsBefore = 0, onsDuringEcho = 0, offs = 0;
        float echoPeak = 0.f;
        const size_t guitarEnd = size_t(0.6 * fs), total = size_t(2.5 * fs);
        for (size_t at = 0; at + bs <= total; at += bs) {
            const bool echo = at >= guitarEnd;
            if (!echo)
                for (int c = 0; c < 2; ++c) b.copyFrom(c, 0, one.samples.data() + at, bs);
            // In the echo phase b still holds the previous block's output, exactly like Live's buffer.
            if (echo) echoPeak = std::max(echoPeak, b.getMagnitude(0, 0, bs));
            juce::MidiBuffer mb;
            p2->processBlock(b, mb);
            for (const auto m : mb) {
                const auto msg = m.getMessage();
                if (msg.isNoteOn()) (echo ? onsDuringEcho : onsBefore)++;
                if (msg.isNoteOff()) ++offs;
            }
        }
        if (onsBefore != 1) { std::printf("FAIL: echo test expected 1 note before the echo, got %d\n", onsBefore); ++failures; }
        if (echoPeak < 0.01f) { std::printf("FAIL: echo test never fed audio back (peak %.3f)\n", echoPeak); ++failures; }
        if (onsDuringEcho != 0) { std::printf("FAIL: morf tracked its own echoed output (%d notes)\n", onsDuringEcho); ++failures; }
        if (offs != onsBefore + onsDuringEcho) { std::printf("FAIL: a note was left hanging during the echo\n"); ++failures; }
    }

    if (failures) return 1;
    std::printf("plugin test passed: %d notes, %zu events, synth peak %.2f; echo test passed\n", ons, got.size(), outPeak);
    return 0;
}
