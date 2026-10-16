#include "PluginEditor.h"
#include "GMNames.h"

namespace
{
const juce::Colour background  { 0xff1b1d22 };
const juce::Colour panel       { 0xff262a31 };
const juce::Colour lcd         { 0xff10140f };
const juce::Colour lcdText     { 0xffb6f09c };
const juce::Colour accent      { 0xffffb347 };
const juce::Colour dimText     { 0xff8a919c };

constexpr int stripHeight = 30;
constexpr int stripGap = 4;
}

GeneralMidiEditor::GeneralMidiEditor (GeneralMidiProcessor& p)
    : AudioProcessorEditor (&p),
      processor (p),
      keyboard (p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    lookAndFeel.setColour (juce::ComboBox::backgroundColourId, lcd);
    lookAndFeel.setColour (juce::ComboBox::textColourId, lcdText);
    lookAndFeel.setColour (juce::ComboBox::outlineColourId, panel.brighter (0.15f));
    lookAndFeel.setColour (juce::ComboBox::arrowColourId, lcdText.withAlpha (0.7f));
    lookAndFeel.setColour (juce::PopupMenu::backgroundColourId, lcd);
    lookAndFeel.setColour (juce::PopupMenu::textColourId, lcdText);
    lookAndFeel.setColour (juce::PopupMenu::headerTextColourId, accent);
    lookAndFeel.setColour (juce::PopupMenu::highlightedBackgroundColourId, lcdText.withAlpha (0.25f));
    lookAndFeel.setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    lookAndFeel.setColour (juce::TextButton::buttonColourId, panel.brighter (0.1f));
    lookAndFeel.setColour (juce::TextButton::buttonOnColourId, accent);
    lookAndFeel.setColour (juce::TextButton::textColourOffId, dimText.brighter (0.4f));
    lookAndFeel.setColour (juce::TextButton::textColourOnId, juce::Colours::black);
    lookAndFeel.setColour (juce::Slider::rotarySliderFillColourId, accent);
    lookAndFeel.setColour (juce::Slider::thumbColourId, accent.brighter (0.3f));
    lookAndFeel.setColour (juce::Slider::textBoxTextColourId, dimText.brighter (0.5f));
    lookAndFeel.setColour (juce::Label::textColourId, dimText.brighter (0.5f));
    setLookAndFeel (&lookAndFeel);

    for (int ch = 0; ch < GeneralMidiProcessor::numChannels; ++ch)
    {
        auto& s = strips[(size_t) ch];

        s.number.setButtonText (juce::String (ch + 1));
        s.number.setClickingTogglesState (false);
        s.number.setTooltip ("Play this channel from the on-screen keyboard");
        s.number.onClick = [this, ch] { setKeyboardChannel (ch + 1); };
        addAndMakeVisible (s.number);

        fillInstrumentMenu (s.instrument, ch);
        s.instrument.onChange = [this, ch]
        {
            const int id = strips[(size_t) ch].instrument.getSelectedId();
            if (id > 0)
            {
                const int code = id - 1;
                processor.requestProgram (ch, GeneralMidiProcessor::bankOf (code), GeneralMidiProcessor::programOf (code));
            }
        };
        addAndMakeVisible (s.instrument);
        showProgram (ch);
    }

    setupKnob (volume, "volume", "Volume");
    setupKnob (reverb, "reverb", "Reverb");
    setupKnob (chorus, "chorus", "Chorus");

    resetButton.setTooltip ("Send a General MIDI reset: every channel back to its default instrument and controllers");
    resetButton.onClick = [this] { processor.requestGMReset(); };
    addAndMakeVisible (resetButton);

    panicButton.setTooltip ("Silence all notes immediately");
    panicButton.onClick = [this] { processor.requestPanic(); };
    addAndMakeVisible (panicButton);

    keyboard.setAvailableRange (21, 108);
    keyboard.setLowestVisibleKey (36);
    addAndMakeVisible (keyboard);
    setKeyboardChannel (processor.keyboardChannel.load());

    voicesLabel.setJustificationType (juce::Justification::centredRight);
    voicesLabel.setFont (juce::FontOptions (12.0f));
    addAndMakeVisible (voicesLabel);

    setSize (820, 520);
    startTimerHz (30);
}

GeneralMidiEditor::~GeneralMidiEditor()
{
    setLookAndFeel (nullptr);
}

void GeneralMidiEditor::setupKnob (Knob& knob, const juce::String& paramId, const juce::String& name)
{
    knob.slider.setColour (juce::Slider::textBoxOutlineColourId, panel);
    knob.slider.setColour (juce::Slider::textBoxBackgroundColourId, panel);
    knob.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 16);
    addAndMakeVisible (knob.slider);
    knob.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processor.parameters, paramId, knob.slider);

    knob.label.setText (name, juce::dontSendNotification);
    knob.label.setJustificationType (juce::Justification::centred);
    knob.label.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    addAndMakeVisible (knob.label);
}

void GeneralMidiEditor::fillInstrumentMenu (juce::ComboBox& box, int channel)
{
    const auto& presets = processor.getPresets();

    if (channel == gm::GMSynth::drumChannel)
    {
        box.addSectionHeading ("Drum Kits");
        for (const auto& p : presets)
            if (p.bank == gm::GMSynth::drumBank)
                box.addItem (juce::String (p.program + 1).paddedLeft ('0', 3) + "  " + p.name,
                             GeneralMidiProcessor::encodeProgram (p.bank, p.program) + 1);
        return;
    }

    // The 128 GM instruments by family, each followed by the SoundFont's GS variations.
    for (int program = 0; program < 128; ++program)
    {
        if (program % 8 == 0)
            box.addSectionHeading (gm::familyName (program));

        const auto number = juce::String (program + 1).paddedLeft ('0', 3);
        box.addItem (number + "  " + gm::programName (program), GeneralMidiProcessor::encodeProgram (0, program) + 1);

        for (const auto& p : presets)
            if (p.program == program && p.bank > 0 && p.bank < gm::GMSynth::drumBank)
                box.addItem ("        " + number + "." + juce::String (p.bank) + "  " + p.name,
                             GeneralMidiProcessor::encodeProgram (p.bank, p.program) + 1);
    }
}

void GeneralMidiEditor::showProgram (int channel)
{
    auto& box = strips[(size_t) channel].instrument;
    const int code = processor.getChannelProgram (channel);
    const int id = code + 1;

    if (box.getSelectedId() == id)
        return;

    if (box.indexOfItemId (id) >= 0)
    {
        box.setSelectedId (id, juce::dontSendNotification);
    }
    else
    {
        // e.g. a drum kit selected on a melodic channel via SysEx
        const int bank = GeneralMidiProcessor::bankOf (code), program = GeneralMidiProcessor::programOf (code);
        const auto name = processor.getPresetName (bank, program);
        box.setText (juce::String (program + 1).paddedLeft ('0', 3) + "  "
                         + (name.empty() ? juce::String ("(bank " + juce::String (bank) + ")") : juce::String (name)),
                     juce::dontSendNotification);
    }
}

void GeneralMidiEditor::setKeyboardChannel (int channel)
{
    processor.keyboardChannel = channel;
    keyboard.setMidiChannel (channel);

    for (int ch = 0; ch < GeneralMidiProcessor::numChannels; ++ch)
        strips[(size_t) ch].number.setToggleState (ch + 1 == channel, juce::dontSendNotification);
}

void GeneralMidiEditor::timerCallback()
{
    for (int ch = 0; ch < GeneralMidiProcessor::numChannels; ++ch)
    {
        auto& s = strips[(size_t) ch];
        showProgram (ch);

        const auto counter = processor.getChannelActivity (ch);
        if (counter != s.lastCounter)
        {
            s.lastCounter = counter;
            s.activity = 1.0f;
            repaint (s.ledBounds);
        }
        else if (s.activity > 0.0f)
        {
            s.activity = juce::jmax (0.0f, s.activity - 0.08f);
            repaint (s.ledBounds);
        }
    }

    voicesLabel.setText (processor.isSoundFontLoaded()
                             ? "Voices: " + juce::String (processor.getActiveVoices())
                             : "SoundFont failed to load",
                         juce::dontSendNotification);
}

void GeneralMidiEditor::paint (juce::Graphics& g)
{
    g.fillAll (background);

    auto header = getLocalBounds().removeFromTop (110).reduced (16, 12);
    g.setColour (panel);
    g.fillRoundedRectangle (getLocalBounds().removeFromTop (110).reduced (8, 6).toFloat(), 8.0f);

    auto titleArea = header.removeFromLeft (330);
    g.setColour (accent);
    g.setFont (juce::FontOptions (26.0f, juce::Font::bold));
    g.drawText ("GENERAL MIDI SYNTH", titleArea.removeFromTop (44), juce::Justification::bottomLeft);
    g.setColour (dimText);
    g.setFont (juce::FontOptions (13.0f));
    g.drawText ("128 instruments + drum kits  |  GeneralUser GS SoundFont", titleArea.removeFromTop (22),
                juce::Justification::topLeft);
    g.drawText ("Click a channel number to play it from the keyboard", titleArea.removeFromTop (18),
                juce::Justification::topLeft);

    for (const auto& s : strips)
    {
        const auto led = s.ledBounds.toFloat();
        g.setColour (lcd);
        g.fillEllipse (led);
        g.setColour (lcdText.withAlpha (0.15f + 0.85f * s.activity));
        g.fillEllipse (led.reduced (2.0f));
    }
}

void GeneralMidiEditor::resized()
{
    auto area = getLocalBounds();

    // Header: title (painted), knobs, buttons
    auto header = area.removeFromTop (110).reduced (16, 12);
    header.removeFromLeft (330);

    auto buttons = header.removeFromRight (120);
    buttons.removeFromTop (14);
    resetButton.setBounds (buttons.removeFromTop (28));
    buttons.removeFromTop (8);
    panicButton.setBounds (buttons.removeFromTop (28));

    header.removeFromRight (10);
    for (auto* knob : { &chorus, &reverb, &volume })
    {
        auto k = header.removeFromRight (90);
        knob->label.setBounds (k.removeFromTop (16));
        knob->slider.setBounds (k);
    }

    // Keyboard and status at the bottom
    auto bottom = area.removeFromBottom (110).reduced (8, 0);
    voicesLabel.setBounds (bottom.removeFromTop (20).reduced (4, 0));
    keyboard.setBounds (bottom.removeFromTop (82));

    // 16 channel strips in two columns
    area = area.reduced (12, 10);
    const int columnWidth = (area.getWidth() - 16) / 2;

    for (int ch = 0; ch < GeneralMidiProcessor::numChannels; ++ch)
    {
        const int column = ch / 8, row = ch % 8;
        juce::Rectangle<int> r (area.getX() + column * (columnWidth + 16),
                                area.getY() + row * (stripHeight + stripGap),
                                columnWidth, stripHeight);

        auto& s = strips[(size_t) ch];
        s.ledBounds = r.removeFromLeft (14).withSizeKeepingCentre (12, 12);
        r.removeFromLeft (6);
        s.number.setBounds (r.removeFromLeft (38));
        r.removeFromLeft (6);
        s.instrument.setBounds (r);
    }
}
