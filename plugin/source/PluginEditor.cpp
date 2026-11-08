#include "PluginEditor.h"

namespace {
const juce::Colour kBg{0xff15171c}, kPanel{0xff1f232b}, kText{0xffe8eaed}, kDim{0xff8a93a3}, kAccent{0xff4fd1c5}, kWarn{0xfff6ad55};

juce::String noteName(int midi) {
    static const char* names[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    return juce::String(names[midi % 12]) + juce::String(midi / 12 - 1);
}
}  // namespace

MorfEditor::MorfEditor(MorfProcessor& p) : AudioProcessorEditor(&p), proc(p) {
    laf.setColourScheme(juce::LookAndFeel_V4::getMidnightColourScheme());
    laf.setColour(juce::Slider::rotarySliderFillColourId, kAccent);
    laf.setColour(juce::Slider::thumbColourId, kAccent);
    setLookAndFeel(&laf);

    setupKnob(gain, "inputGain", "Input gain");
    setupKnob(threshold, "sensitivity", "Note threshold");
    setupKnob(synthLevel, "monitorLevel", "Synth level");
    setupKnob(dryLevel, "dryLevel", "Guitar level");
    setupKnob(channel, "midiChannel", "MIDI channel");

    bendRange.addItemList({"±2 semitones", "±12 semitones", "±24 semitones", "±48 semitones"}, 1);
    addAndMakeVisible(bendRange);
    bendAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(p.params, "bendRange", bendRange);
    bendLabel.setText("Pitch-bend range", juce::dontSendNotification);
    bendLabel.setColour(juce::Label::textColourId, kDim);
    addAndMakeVisible(bendLabel);

    addAndMakeVisible(monitor);
    monitorAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.params, "monitor", monitor);

    setSize(640, 400);
    startTimerHz(30);
}

MorfEditor::~MorfEditor() { setLookAndFeel(nullptr); }

void MorfEditor::setupKnob(Knob& k, const juce::String& id, const juce::String& name) {
    k.slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 18);
    addAndMakeVisible(k.slider);
    k.attach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(proc.params, id, k.slider);
    k.label.setText(name, juce::dontSendNotification);
    k.label.setJustificationType(juce::Justification::centred);
    k.label.setColour(juce::Label::textColourId, kDim);
    addAndMakeVisible(k.label);
}

void MorfEditor::timerCallback() {
    note = proc.displayNote.load();
    bend = proc.displayBend.load();
    levelDb = proc.displayLevelDb.load();
    f0 = proc.displayF0.load();
    peakHoldDb = juce::jmax(levelDb, peakHoldDb - 1.5f);
    repaint(noteArea.getUnion(meterArea));
}

void MorfEditor::paint(juce::Graphics& g) {
    g.fillAll(kBg);
    g.setColour(kText);
    g.setFont(juce::FontOptions(22.f, juce::Font::bold));
    g.drawText("morf", 20, 12, 200, 30, juce::Justification::centredLeft);
    g.setColour(kDim);
    g.setFont(juce::FontOptions(13.f));
    juce::String status = "guitar to MIDI";
    if (proc.hasVirtualMidiPort()) status += "  ·  sending to virtual MIDI port \"morf\"";
    g.drawText(status, 90, 14, 500, 28, juce::Justification::centredLeft);

    // Detected note and how far it's bent.
    g.setColour(kPanel);
    g.fillRoundedRectangle(noteArea.toFloat(), 10.f);
    auto na = noteArea.reduced(16);
    if (note >= 0) {
        g.setColour(kText);
        g.setFont(juce::FontOptions(64.f, juce::Font::bold));
        g.drawText(noteName(note), na.removeFromTop(80), juce::Justification::centred);
        const float cents = bend * 100.f;
        g.setColour(std::abs(cents) < 10.f ? kAccent : kWarn);
        g.setFont(juce::FontOptions(16.f));
        g.drawText((cents >= 0 ? "+" : "") + juce::String(cents, 0) + " cents" + (f0 > 0 ? "   " + juce::String(f0, 1) + " Hz" : ""),
                   na.removeFromTop(24), juce::Justification::centred);
        // Bend bar: centre = in tune, full width = ±2 semitones.
        auto bar = na.removeFromTop(18).reduced(20, 4).toFloat();
        g.setColour(kBg);
        g.fillRoundedRectangle(bar, 4.f);
        const float x = bar.getCentreX() + juce::jlimit(-1.f, 1.f, bend / 2.f) * bar.getWidth() * 0.5f;
        g.setColour(kAccent);
        g.fillRect(juce::Rectangle<float>(juce::jmin(x, bar.getCentreX()), bar.getY(), std::abs(x - bar.getCentreX()) + 1.f, bar.getHeight()));
    } else {
        g.setColour(kDim);
        g.setFont(juce::FontOptions(20.f));
        g.drawText("play a note", na, juce::Justification::centred);
    }

    // Input meter with the note threshold marked.
    auto m = meterArea.toFloat();
    g.setColour(kPanel);
    g.fillRoundedRectangle(m, 4.f);
    auto toX = [&](float db) { return m.getX() + m.getWidth() * juce::jlimit(0.f, 1.f, (db + 80.f) / 80.f); };
    g.setColour(levelDb > -3.f ? juce::Colours::red : kAccent);
    g.fillRoundedRectangle(m.withRight(toX(levelDb)), 4.f);
    g.setColour(kText);
    g.fillRect(toX(peakHoldDb) - 1.f, m.getY(), 2.f, m.getHeight());
    const float thr = proc.params.getRawParameterValue("sensitivity")->load();
    g.setColour(kWarn);
    g.fillRect(toX(thr) - 1.f, m.getY() - 3.f, 2.f, m.getHeight() + 6.f);
    g.setColour(kDim);
    g.setFont(juce::FontOptions(12.f));
    g.drawText("input", meterArea.getX(), meterArea.getBottom() + 2, 100, 16, juce::Justification::centredLeft);
}

void MorfEditor::resized() {
    auto r = getLocalBounds().reduced(20);
    r.removeFromTop(40);
    auto top = r.removeFromTop(150);
    noteArea = top.removeFromLeft(300);
    top.removeFromLeft(20);
    auto side = top;
    bendLabel.setBounds(side.removeFromTop(22));
    bendRange.setBounds(side.removeFromTop(28));
    side.removeFromTop(14);
    monitor.setBounds(side.removeFromTop(28));
    r.removeFromTop(12);
    meterArea = r.removeFromTop(14);
    r.removeFromTop(24);
    Knob* knobs[] = {&gain, &threshold, &synthLevel, &dryLevel, &channel};
    const int w = r.getWidth() / 5;
    for (auto* k : knobs) {
        auto c = r.removeFromLeft(w);
        k->label.setBounds(c.removeFromTop(18));
        k->slider.setBounds(c.reduced(6, 0));
    }
}
