#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>

#include "GMSynth.h"

#include <array>
#include <atomic>

class GeneralMidiProcessor : public juce::AudioProcessor
{
public:
    static constexpr int numChannels = gm::GMSynth::numChannels;

    // Programs are passed between threads encoded as bank * 128 + program.
    static int encodeProgram (int bank, int program) { return bank * 128 + program; }
    static int bankOf (int code)    { return code / 128; }
    static int programOf (int code) { return code % 128; }

    GeneralMidiProcessor();
    ~GeneralMidiProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 3.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    // Called from the editor (message thread); applied on the audio thread.
    void requestProgram (int channel, int bank, int program);
    void requestGMReset()   { pendingReset = true; }
    void requestPanic()     { pendingPanic = true; }

    int getChannelProgram (int channel) const { return currentProgram[(size_t) channel].load(); }
    uint32_t getChannelActivity (int channel) const { return noteCounter[(size_t) channel].load(); }
    int getActiveVoices() const { return activeVoices.load(); }

    const std::vector<gm::PresetInfo>& getPresets() const { return synth.getPresets(); }
    std::string getPresetName (int bank, int program) const { return synth.getPresetName (bank, program); }
    bool isSoundFontLoaded() const { return synth.isReady(); }

    juce::AudioProcessorValueTreeState parameters;
    juce::MidiKeyboardState keyboardState;
    std::atomic<int> keyboardChannel { 1 }; // 1-based MIDI channel the on-screen keyboard plays

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameters();
    void refreshProgramDisplay();

    gm::GMSynth synth;
    juce::AudioBuffer<float> monoScratch;

    std::atomic<float>* volumeParam = nullptr;
    std::atomic<float>* reverbParam = nullptr;
    std::atomic<float>* chorusParam = nullptr;

    std::array<std::atomic<int>, numChannels> pendingProgram;
    std::array<std::atomic<int>, numChannels> currentProgram;
    std::array<std::atomic<uint32_t>, numChannels> noteCounter;
    std::atomic<bool> pendingReset { false }, pendingPanic { false };
    std::atomic<int> activeVoices { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GeneralMidiProcessor)
};
