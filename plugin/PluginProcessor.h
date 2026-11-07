#pragma once
#include <atomic>
#include <memory>

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_processors/juce_audio_processors.h>

#include "MonitorSynth.h"
#include "morf/mono_tracker.h"

class MorfProcessor : public juce::AudioProcessor {
public:
    MorfProcessor();
    ~MorfProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState params;

    // Read by the editor (GUI thread); written by the audio thread.
    std::atomic<int> displayNote{-1};
    std::atomic<float> displayBend{0.f};
    std::atomic<float> displayLevelDb{-120.f};
    std::atomic<float> displayF0{0.f};
    bool hasVirtualMidiPort() const { return virtualOut != nullptr; }

    static constexpr int kBendRanges[] = {2, 12, 24, 48};

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    std::unique_ptr<morf::MonoTracker> tracker;
    morf::EventQueue events;
    morf::MonitorSynth synth;
    juce::AudioBuffer<float> mono;
    std::unique_ptr<juce::MidiOutput> virtualOut;  // standalone app only
    double currentRate = 48000.0;
    int lastSentBendRange = -1;

    std::atomic<float>* inputGainDb = nullptr;
    std::atomic<float>* sensitivityDb = nullptr;
    std::atomic<float>* bendRangeIndex = nullptr;
    std::atomic<float>* midiChannel = nullptr;
    std::atomic<float>* monitorOn = nullptr;
    std::atomic<float>* monitorLevelDb = nullptr;
    std::atomic<float>* dryLevelDb = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MorfProcessor)
};
