#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <BinaryData.h>

namespace
{
constexpr double defaultSampleRate = 44100.0;
}

GeneralMidiProcessor::GeneralMidiProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      parameters (*this, nullptr, "GeneralMidiSynth", createParameters()),
      synth (GMData::GeneralUserGS_sf2, (size_t) GMData::GeneralUserGS_sf2Size)
{
    volumeParam = parameters.getRawParameterValue ("volume");
    reverbParam = parameters.getRawParameterValue ("reverb");
    chorusParam = parameters.getRawParameterValue ("chorus");

    for (auto& p : pendingProgram) p = -1;
    for (auto& c : noteCounter) c = 0;

    // Load the SoundFont straight away so the editor can list the instruments even
    // before the host starts playback. prepareToPlay() reloads only if the rate differs.
    synth.prepare (defaultSampleRate);
    refreshProgramDisplay();
}

juce::AudioProcessorValueTreeState::ParameterLayout GeneralMidiProcessor::createParameters()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "volume", 1 }, "Volume",
                    NormalisableRange<float> (-48.0f, 12.0f, 0.1f), 0.0f,
                    AudioParameterFloatAttributes().withLabel ("dB")
                        .withStringFromValueFunction ([] (float v, int) { return String (v, 1) + " dB"; })));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "reverb", 1 }, "Reverb",
                    NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.7f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "chorus", 1 }, "Chorus",
                    NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.15f));
    return layout;
}

void GeneralMidiProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    synth.prepare (sampleRate);
    monoScratch.setSize (2, samplesPerBlock);
    refreshProgramDisplay();
}

bool GeneralMidiProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void GeneralMidiProcessor::requestProgram (int channel, int bank, int program)
{
    if (channel >= 0 && channel < numChannels)
        pendingProgram[(size_t) channel] = encodeProgram (bank, program);
}

void GeneralMidiProcessor::refreshProgramDisplay()
{
    for (int ch = 0; ch < numChannels; ++ch)
    {
        int bank = 0, program = 0;
        if (synth.getProgram (ch, bank, program))
            currentProgram[(size_t) ch] = encodeProgram (bank, program);
    }
}

void GeneralMidiProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    buffer.clear();

    keyboardState.processNextMidiBuffer (midi, 0, numSamples, true);

    synth.setGain (juce::Decibels::decibelsToGain (volumeParam->load(), -48.0f));
    synth.setReverbLevel (reverbParam->load());
    synth.setChorusLevel (chorusParam->load());

    bool programsChanged = false;

    if (pendingPanic.exchange (false))
        synth.allSoundsOff();

    if (pendingReset.exchange (false))
    {
        synth.allSoundsOff();
        synth.resetToGM();
        programsChanged = true;
    }

    for (int ch = 0; ch < numChannels; ++ch)
    {
        const int code = pendingProgram[(size_t) ch].exchange (-1);
        if (code >= 0)
        {
            synth.selectProgram (ch, bankOf (code), programOf (code));
            programsChanged = true;
        }
    }

    // Render in slices between MIDI events so timing is sample accurate.
    float* left;
    float* right;
    const bool stereo = buffer.getNumChannels() >= 2;
    if (stereo)
    {
        left = buffer.getWritePointer (0);
        right = buffer.getWritePointer (1);
    }
    else
    {
        if (monoScratch.getNumSamples() < numSamples)
            monoScratch.setSize (2, numSamples, false, false, true);
        left = monoScratch.getWritePointer (0);
        right = monoScratch.getWritePointer (1);
    }

    int position = 0;
    for (const auto metadata : midi)
    {
        const int eventPos = juce::jlimit (0, numSamples, metadata.samplePosition);
        if (eventPos > position)
        {
            synth.render (left + position, right + position, eventPos - position);
            position = eventPos;
        }

        const auto* data = metadata.data;
        const int size = metadata.numBytes;
        synth.handleMidi (data, size);

        const int type = data[0] & 0xF0;
        if (type == 0x90 && size > 2 && data[2] > 0)
            noteCounter[(size_t) (data[0] & 0x0F)].fetch_add (1, std::memory_order_relaxed);
        else if (type == 0xC0 || data[0] == 0xF0 || data[0] == 0xFF)
            programsChanged = true;
    }

    if (position < numSamples)
        synth.render (left + position, right + position, numSamples - position);

    if (! stereo && buffer.getNumChannels() == 1)
    {
        auto* out = buffer.getWritePointer (0);
        for (int i = 0; i < numSamples; ++i)
            out[i] = 0.5f * (left[i] + right[i]);
    }

    if (programsChanged)
        refreshProgramDisplay();

    activeVoices = synth.getActiveVoiceCount();
}

//==============================================================================
juce::AudioProcessorEditor* GeneralMidiProcessor::createEditor()
{
    return new GeneralMidiEditor (*this);
}

void GeneralMidiProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();

    juce::ValueTree channels ("Channels");
    for (int ch = 0; ch < numChannels; ++ch)
        channels.setProperty ("ch" + juce::String (ch), currentProgram[(size_t) ch].load(), nullptr);
    channels.setProperty ("keyboardChannel", keyboardChannel.load(), nullptr);

    state.removeChild (state.getChildWithName ("Channels"), nullptr);
    state.appendChild (channels, nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void GeneralMidiProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (parameters.state.getType()))
        return;

    auto state = juce::ValueTree::fromXml (*xml);
    parameters.replaceState (state);

    const auto channels = state.getChildWithName ("Channels");
    if (! channels.isValid())
        return;

    for (int ch = 0; ch < numChannels; ++ch)
    {
        const int code = channels.getProperty ("ch" + juce::String (ch), -1);
        if (code >= 0)
        {
            pendingProgram[(size_t) ch] = code;
            currentProgram[(size_t) ch] = code;
        }
    }
    keyboardChannel = juce::jlimit (1, 16, (int) channels.getProperty ("keyboardChannel", 1));
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new GeneralMidiProcessor();
}
