#include "PluginProcessor.h"

#include "PluginEditor.h"

namespace {
const juce::String kGain = "inputGain", kSens = "sensitivity", kBend = "bendRange", kChannel = "midiChannel",
                   kMonitor = "monitor", kMonitorLevel = "monitorLevel", kDry = "dryLevel";
}

constexpr int MorfProcessor::kBendRanges[];

MorfProcessor::MorfProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Guitar", juce::AudioChannelSet::mono(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      params(*this, nullptr, "morf", createLayout()) {
    inputGainDb = params.getRawParameterValue(kGain);
    sensitivityDb = params.getRawParameterValue(kSens);
    bendRangeIndex = params.getRawParameterValue(kBend);
    midiChannel = params.getRawParameterValue(kChannel);
    monitorOn = params.getRawParameterValue(kMonitor);
    monitorLevelDb = params.getRawParameterValue(kMonitorLevel);
    dryLevelDb = params.getRawParameterValue(kDry);

#if JUCE_MAC || JUCE_LINUX
    // The standalone app has no host to pass MIDI to, so it publishes a virtual
    // MIDI port that Logic, Ableton or any synth can select as an input.
    if (wrapperType == wrapperType_Standalone) virtualOut = juce::MidiOutput::createNewDevice("morf");
#endif
}

MorfProcessor::~MorfProcessor() = default;

juce::AudioProcessorValueTreeState::ParameterLayout MorfProcessor::createLayout() {
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;
    l.add(std::make_unique<AudioParameterFloat>(ParameterID{kGain, 1}, "Input gain", NormalisableRange<float>(-24.f, 24.f, 0.1f), 0.f,
                                                AudioParameterFloatAttributes().withLabel("dB")));
    l.add(std::make_unique<AudioParameterFloat>(ParameterID{kSens, 1}, "Note threshold", NormalisableRange<float>(-75.f, -30.f, 0.5f), -52.f,
                                                AudioParameterFloatAttributes().withLabel("dBFS")));
    l.add(std::make_unique<AudioParameterChoice>(ParameterID{kBend, 1}, "Pitch-bend range", StringArray{"2", "12", "24", "48"}, 1));
    l.add(std::make_unique<AudioParameterInt>(ParameterID{kChannel, 1}, "MIDI channel", 1, 16, 1));
    l.add(std::make_unique<AudioParameterBool>(ParameterID{kMonitor, 1}, "Monitor synth", true));
    l.add(std::make_unique<AudioParameterFloat>(ParameterID{kMonitorLevel, 1}, "Synth level", NormalisableRange<float>(-60.f, 6.f, 0.1f), -6.f,
                                                AudioParameterFloatAttributes().withLabel("dB")));
    l.add(std::make_unique<AudioParameterFloat>(ParameterID{kDry, 1}, "Guitar level", NormalisableRange<float>(-60.f, 6.f, 0.1f), -60.f,
                                                AudioParameterFloatAttributes().withLabel("dB")));
    return l;
}

bool MorfProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo()) return false;
    const auto in = layouts.getMainInputChannelSet();
    return in.isDisabled() || in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

void MorfProcessor::prepareToPlay(double sampleRate, int samplesPerBlock) {
    currentRate = sampleRate;
    morf::MonoTrackerConfig cfg;
    cfg.sampleRate = sampleRate;
    tracker = std::make_unique<morf::MonoTracker>(cfg);
    synth.prepare(sampleRate);
    mono.setSize(1, juce::jmax(1, samplesPerBlock));
    lastSentBendRange = -1;
    setLatencySamples(0);  // events are placed where they were detected; no extra delay
}

void MorfProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) {
    juce::ScopedNoDenormals noDenormals;
    midi.clear();
    if (!tracker) return;

    const int n = buffer.getNumSamples();
    const int inCh = getTotalNumInputChannels();
    const int outCh = getTotalNumOutputChannels();
    if (mono.getNumSamples() < n) mono.setSize(1, n, false, false, true);  // only if the host lied about block size

    // Mix the guitar input down to mono with the input gain.
    float* m = mono.getWritePointer(0);
    const float gain = juce::Decibels::decibelsToGain(inputGainDb->load()) / float(juce::jmax(1, inCh));
    juce::FloatVectorOperations::clear(m, n);
    for (int c = 0; c < inCh; ++c) juce::FloatVectorOperations::addWithMultiply(m, buffer.getReadPointer(c), gain, n);

    // Apply user settings, then track.
    const int bendRange = kBendRanges[juce::jlimit(0, 3, int(bendRangeIndex->load()))];
    const int channel = juce::jlimit(1, 16, int(midiChannel->load())) - 1;
    tracker->setGate(sensitivityDb->load());
    tracker->setBendRange(float(bendRange));
    tracker->setChannel(channel);

    if (bendRange != lastSentBendRange) {  // tell the receiving synth the bend range (RPN 0)
        midi.addEvent(juce::MidiMessage::controllerEvent(channel + 1, 101, 0), 0);
        midi.addEvent(juce::MidiMessage::controllerEvent(channel + 1, 100, 0), 0);
        midi.addEvent(juce::MidiMessage::controllerEvent(channel + 1, 6, bendRange), 0);
        midi.addEvent(juce::MidiMessage::controllerEvent(channel + 1, 38, 0), 0);
        lastSentBendRange = bendRange;
    }

    const int64_t blockStart = tracker->samplePosition();
    events.clear();
    tracker->process(m, n, events);

    // Output: optional dry guitar plus the monitor synth, with each MIDI event
    // placed at the sample where the tracker emitted it.
    const float dry = dryLevelDb->load() <= -59.9f ? 0.f : juce::Decibels::decibelsToGain(dryLevelDb->load());
    const bool monitor = monitorOn->load() > 0.5f;
    const float synthLevel = juce::Decibels::decibelsToGain(monitorLevelDb->load());
    float* out0 = outCh > 0 ? buffer.getWritePointer(0) : nullptr;
    if (out0) {
        // out0 may alias the input channel; m already holds a copy of it.
        juce::FloatVectorOperations::multiply(out0, m, dry, n);
    }
    int rendered = 0;
    for (const auto& e : events) {
        const int offset = int(juce::jlimit<int64_t>(0, n - 1, e.time - blockStart));
        if (monitor && out0 && offset > rendered) { synth.render(out0 + rendered, offset - rendered, synthLevel); rendered = offset; }
        midi.addEvent(juce::MidiMessage(e.status, e.data1, e.data2), offset);
        if (e.isNoteOn()) synth.noteOn(e.data1, e.data2);
        else if (e.isNoteOff()) synth.noteOff(e.data1);
        else if (e.isPitchBend()) synth.pitchBend(float(e.pitchBendValue() - 8192) / 8191.f * float(bendRange));
    }
    if (monitor && out0 && n > rendered) synth.render(out0 + rendered, n - rendered, synthLevel);
    for (int c = 1; c < outCh; ++c) {
        if (out0) buffer.copyFrom(c, 0, out0, n);
        else buffer.clear(c, 0, n);
    }
    for (int c = outCh; c < buffer.getNumChannels(); ++c) buffer.clear(c, 0, n);

    if (virtualOut && !midi.isEmpty()) virtualOut->sendBlockOfMessagesNow(midi);

    const auto& f = tracker->lastFrame();
    displayNote.store(tracker->currentNote());
    displayBend.store(tracker->currentBend());
    displayLevelDb.store(f.levelDb);
    displayF0.store(f.voiced ? f.f0 : 0.f);
}

juce::AudioProcessorEditor* MorfProcessor::createEditor() { return new MorfEditor(*this); }

void MorfProcessor::getStateInformation(juce::MemoryBlock& destData) {
    if (auto xml = params.copyState().createXml()) copyXmlToBinary(*xml, destData);
}

void MorfProcessor::setStateInformation(const void* data, int sizeInBytes) {
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        if (xml->hasTagName(params.state.getType())) params.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new MorfProcessor(); }
