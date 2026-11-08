// Loads the built morf.vst3 file through the VST3 API, the way a DAW does, and checks
// that it scans, opens, accepts guitar audio on its side-chain and sends the right MIDI.
// This tests the actual plugin binary, not just the processor code.
//
//   morf_vst3_host_tests /path/to/morf.vst3
#include <cstdio>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>

#include "morf/bench/guitar_synth.h"

static int failures = 0;
static void check(bool ok, const juce::String& what) {
    if (!ok) {
        ++failures;
        std::printf("FAIL: %s\n", what.toRawUTF8());
    }
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::printf("usage: morf_vst3_host_tests /path/to/morf.vst3\n");
        return 2;
    }
    juce::ScopedJuceInitialiser_GUI init;
    const juce::String path = juce::File(juce::String::fromUTF8(argv[1])).getFullPathName();
    check(juce::File(path).exists(), "plugin not found at " + path);

    // 1. Scan, as a host's plug-in scanner would.
    juce::VST3PluginFormat format;
    juce::OwnedArray<juce::PluginDescription> found;
    format.findAllTypesForFile(found, path);
    check(found.size() == 1, "scan found " + juce::String(found.size()) + " plugins, expected 1");
    if (found.isEmpty()) return 1;
    const auto& desc = *found[0];
    std::printf("scanned: %s by %s, category \"%s\", instrument=%d, inputs=%d outputs=%d\n",
                desc.name.toRawUTF8(), desc.manufacturerName.toRawUTF8(), desc.category.toRawUTF8(),
                int(desc.isInstrument), desc.numInputChannels, desc.numOutputChannels);
    check(desc.name == "morf", "name is " + desc.name);
    check(desc.isInstrument, "not reported as an instrument");

    // 2. Open it.
    const double fs = 48000;
    const int block = 256;
    juce::String error;
    auto plugin = format.createInstanceFromDescription(desc, fs, block, error);
    check(plugin != nullptr, "could not open plugin: " + error);
    if (!plugin) return 1;

    // 3. The guitar input must be there (as the VST3 side-chain) and accept mono or stereo.
    check(plugin->getBusCount(true) == 1, "expected 1 input bus, got " + juce::String(plugin->getBusCount(true)));
    check(plugin->getBusCount(false) == 1, "expected 1 output bus, got " + juce::String(plugin->getBusCount(false)));
    juce::AudioProcessor::BusesLayout layout;
    layout.inputBuses.add(juce::AudioChannelSet::mono());
    layout.outputBuses.add(juce::AudioChannelSet::stereo());
    check(plugin->setBusesLayout(layout), "mono guitar input + stereo output rejected");
    check(plugin->producesMidi(), "plugin doesn't declare MIDI output");

    // 4. Play three notes (one with a bend) into it and collect the MIDI it sends.
    std::vector<morf::NoteSpec> notes = {{0.2, 0.7, 40, 0.9f}, {0.9, 1.4, 52, 0.7f}, {1.6, 2.6, 64, 0.8f, false, {{0.2, 0.f}, {0.4, 2.f}, {0.7, 0.f}}}};
    const morf::Audio guitar = morf::renderGuitar(notes, {}, nullptr);
    plugin->prepareToPlay(fs, block);
    const int inCh = plugin->getTotalNumInputChannels(), outCh = plugin->getTotalNumOutputChannels();
    juce::AudioBuffer<float> buf(juce::jmax(inCh, outCh), block);
    std::vector<int> noteOns;
    int noteOffs = 0, bends = 0;
    float peak = 0.f;
    for (size_t at = 0; at + block <= guitar.samples.size(); at += block) {
        buf.clear();
        for (int c = 0; c < inCh; ++c) buf.copyFrom(c, 0, guitar.samples.data() + at, block);
        juce::MidiBuffer midi;
        plugin->processBlock(buf, midi);
        peak = juce::jmax(peak, buf.getMagnitude(0, 0, block));
        for (const auto m : midi) {
            const auto msg = m.getMessage();
            if (msg.isNoteOn()) noteOns.push_back(msg.getNoteNumber());
            else if (msg.isNoteOff()) ++noteOffs;
            else if (msg.isPitchWheel()) ++bends;
        }
    }
    plugin->releaseResources();

    juce::String got;
    for (int n : noteOns) got << n << " ";
    std::printf("played E2, E3, E4 with a bend; plugin sent notes [ %s], %d note-offs, %d bends, audio peak %.2f\n",
                got.toRawUTF8(), noteOffs, bends, peak);
    check(noteOns == std::vector<int>({40, 52, 64}), "wrong notes: " + got);
    check(noteOffs == 3, "expected 3 note-offs");
    check(bends > 10, "bend wasn't sent as pitch-bend messages");
    check(peak > 0.01f, "monitor synth made no sound");

    if (failures) return 1;
    std::printf("VST3 host test passed\n");
    return 0;
}
