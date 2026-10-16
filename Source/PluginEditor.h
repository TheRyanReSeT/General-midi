#pragma once

#include "PluginProcessor.h"

class GeneralMidiEditor : public juce::AudioProcessorEditor,
                          private juce::Timer
{
public:
    explicit GeneralMidiEditor (GeneralMidiProcessor&);
    ~GeneralMidiEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct ChannelStrip
    {
        juce::TextButton number;   // click to play this channel from the on-screen keyboard
        juce::ComboBox instrument;
        float activity = 0.0f;
        uint32_t lastCounter = 0;
        juce::Rectangle<int> ledBounds;
    };

    struct Knob
    {
        juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    void timerCallback() override;
    void fillInstrumentMenu (juce::ComboBox& box, int channel);
    void showProgram (int channel);
    void setKeyboardChannel (int channel);
    void showAbout();
    void setupKnob (Knob& knob, const juce::String& paramId, const juce::String& name);

    GeneralMidiProcessor& processor;

    std::array<ChannelStrip, GeneralMidiProcessor::numChannels> strips;
    Knob volume, reverb, chorus;
    juce::TextButton resetButton { "GM Reset" }, panicButton { "All Sound Off" }, aboutButton { "About" };
    juce::MidiKeyboardComponent keyboard;
    juce::Label voicesLabel;

    juce::LookAndFeel_V4 lookAndFeel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GeneralMidiEditor)
};
