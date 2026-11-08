#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

#include "PluginProcessor.h"

class MorfEditor : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit MorfEditor(MorfProcessor&);
    ~MorfEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    struct Knob {
        juce::Slider slider{juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow};
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attach;
    };
    void setupKnob(Knob&, const juce::String& paramId, const juce::String& name);

    MorfProcessor& proc;
    juce::LookAndFeel_V4 laf;
    Knob gain, threshold, synthLevel, dryLevel, channel;
    juce::ComboBox bendRange;
    juce::Label bendLabel;
    juce::ToggleButton monitor{"Monitor synth"};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> bendAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> monitorAttach;

    int note = -1;
    float bend = 0.f, levelDb = -120.f, peakHoldDb = -120.f, f0 = 0.f;
    juce::Rectangle<int> noteArea, meterArea;
};
