#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "GMSynth.h"
#include "Parameters.h"
#include "Presets.h"

#include <array>
#include <atomic>

// gemi: every General MIDI sound, played two ways.
//  - Synth mode: incoming MIDI on any channel plays up to three layers, each a GM sound on its own
//    FluidSynth channel (0..2) with its own level, pan, tuning, filter and envelope.
//  - GM Player mode: the 16 MIDI channels pass straight through, one GM sound per channel, drums on 10.
class GemiProcessor : public juce::AudioProcessor
{
public:
    static constexpr int numChannels = gm::GMSynth::numChannels;
    enum class Mode { synth = 0, gmPlayer = 1 };

    GemiProcessor();
    ~GemiProcessor() override = default;

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
    // Message-thread interface used by the editor.

    // Plugin state that is not a host parameter: sounds, mode, GM Player settings and UI preferences,
    // as the JSON-like object the UI sends ({ view, layerProg, gmProg, kit, mute, solo, ch, follow, ... }).
    juce::var getUiState() const;
    void setUiState (const juce::var& state);

    void queueUiNote (int note, int velocity, bool on);
    void requestGMReset() { pendingReset = true; }
    void requestPanic()   { pendingPanic = true; }

    // Set when the audio thread changes a GM channel's sound (a program change in the song).
    bool takeProgramsChanged() { return programsChanged.exchange (false); }
    uint32_t getActiveChannels() const { return activeChannelMask.load(); }
    void getHeldNotes (juce::Array<int>& notes) const;
    int getActiveVoices() const { return activeVoices.load(); }

    // Combos (synth presets).
    gemi::PresetLibrary presets;
    void loadPreset (int index);
    void stepPreset (int delta);
    bool savePreset (const juce::String& name);
    int getPresetIndex() const { const juce::ScopedLock sl (prefsLock); return presetIndex; }
    juce::String getPresetName() const { const juce::ScopedLock sl (prefsLock); return presetName; }
    std::function<void()> onPresetsChanged;   // called on the message thread

    juce::AudioProcessorValueTreeState parameters;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameters();
    juce::var currentPresetData() const;
    void applyPresetData (const juce::var& data);

    struct LayerParams
    {
        std::atomic<float>* on; std::atomic<float>* level; std::atomic<float>* pan; std::atomic<float>* cutoff;
        std::atomic<float>* reso; std::atomic<float>* attack; std::atomic<float>* decay; std::atomic<float>* sustain;
        std::atomic<float>* release; std::atomic<float>* oct; std::atomic<float>* semi; std::atomic<float>* fine;
    };
    std::array<LayerParams, gemi::numLayers> layerParams {};
    std::atomic<float>* reverbParam = nullptr;
    std::atomic<float>* chorusParam = nullptr;
    std::atomic<float>* vibratoParam = nullptr;
    std::atomic<float>* glideParam = nullptr;
    std::atomic<float>* volumeParam = nullptr;
    std::atomic<float>* monoParam = nullptr;

    // Audio thread.
    void enterMode (Mode m);
    void applySynthSettings (bool force);
    void handleSynthMidi (const juce::MidiMessage& m, int source);
    void handleGMMidi (const juce::MidiMessage& m);
    void synthNoteOn (int source, int note, int velocity);
    void synthNoteOff (int source, int note);
    void releaseLayer (int layer);
    void trackHeld (int note, bool on);
    bool channelAudible (int channel) const;

    gm::GMSynth synth;
    Mode appliedMode = Mode::synth;
    bool engineFresh = true;

    struct AppliedLayer
    {
        bool on = true; int level = -1, pan = -1, program = -1, octave = 0, semi = 0;
        int fine = 1000; gm::VoiceShape shape;
    };
    std::array<AppliedLayer, gemi::numLayers> applied {};
    int appliedVibrato = -1, appliedGlide = -1, appliedMono = -1, incomingMod = 0;
    std::array<int, numChannels> appliedGmProgram {};
    int appliedKit = -1;
    uint32_t appliedMute = 0, appliedSolo = 0;

    // Which note each layer is playing for a held key: [source][key][layer], -1 = none.
    // Sources 0..15 are MIDI channels, 16 is the on-screen keyboard.
    int8_t layerNote[numChannels + 1][128][gemi::numLayers];
    std::array<int, numChannels> channelNotes {};   // GM Player: notes held per channel
    std::array<uint8_t, 128> heldCount {};

    // Shared between threads.
    std::atomic<int> mode { (int) Mode::synth };
    std::array<std::atomic<int>, gemi::numLayers> layerProgram;
    std::array<std::atomic<int>, numChannels> gmProgram;   // 1..128
    std::atomic<int> drumKit { 1 };                        // GS program number, 1-based
    std::atomic<uint32_t> muteMask { 0 }, soloMask { 0 };
    std::atomic<int> selectedChannel { 1 };
    std::atomic<bool> followKeyboard { true };
    std::atomic<bool> pendingReset { false }, pendingPanic { false }, programsChanged { false };
    std::atomic<uint32_t> activeChannelMask { 0 };
    std::array<std::atomic<uint32_t>, 4> heldBits {};
    std::atomic<int> activeVoices { 0 };

    struct UiNote { int note, velocity; bool on; };
    juce::AbstractFifo uiNoteFifo { 256 };
    std::array<UiNote, 256> uiNotes {};

    // UI preferences and the current combo, carried in the plugin state. The host may save state from
    // any thread, so these are read and written under prefsLock.
    juce::CriticalSection prefsLock;
    juce::var uiPrefs;   // { theme, reduceMotion, size, gmTip }
    int presetIndex = 0;
    juce::String presetName;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GemiProcessor)
};
