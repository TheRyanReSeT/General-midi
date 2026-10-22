#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "GMSynth.h"
#include "Parameters.h"
#include "Presets.h"

#include <array>
#include <atomic>

// gemi: every General MIDI sound, played two ways.
//  - Synth mode: incoming MIDI on any channel plays up to three layers, each a GM sound on its own
//    FluidSynth channel (0..2) with its own level, pan, tuning, filter and envelope. With MPE on,
//    channel 1 is the master channel and each of channels 2..16 gets its own copy of the layers
//    (engine channels 16..60), so every note bends, presses and slides on its own.
//  - GM Player mode: the 16 MIDI channels pass straight through, one GM sound per channel, drums on 10.
//    Each channel can also play on its own stereo output ("Ch 1".."Ch 16") when the host enables it.
class GemiProcessor : public juce::AudioProcessor
{
public:
    static constexpr int numChannels = gm::GMSynth::numChannels;
    static constexpr int numSlots = 16;   // per layer: slot 0 is the plain layer, slots 1..15 are MPE channels 2..16
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

    // Each layer's sound's own envelope, in knob units ([{ a, d, s, r }, ...]), for the UI to show.
    juce::var getOwnEnvelopes();
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
    static BusesProperties makeBuses();
    juce::var currentPresetData() const;
    void applyPresetData (const juce::var& data);

    struct LayerParams
    {
        std::atomic<float>* on; std::atomic<float>* level; std::atomic<float>* pan; std::atomic<float>* cutoff;
        std::atomic<float>* reso; std::atomic<float>* attack; std::atomic<float>* decay; std::atomic<float>* sustain;
        std::atomic<float>* release; std::atomic<float>* oct; std::atomic<float>* semi; std::atomic<float>* fine;
        std::atomic<float>* ownEnv;
    };
    std::array<LayerParams, gemi::numLayers> layerParams {};
    std::atomic<float>* reverbParam = nullptr;
    std::atomic<float>* chorusParam = nullptr;
    std::atomic<float>* vibratoParam = nullptr;
    std::atomic<float>* glideParam = nullptr;
    std::atomic<float>* volumeParam = nullptr;
    std::atomic<float>* monoParam = nullptr;
    std::atomic<float>* layersParam = nullptr;
    std::atomic<float>* mpeParam = nullptr;

    // Audio thread.
    void enterMode (Mode m);
    void applySynthSettings (bool force);
    void handleSynthMidi (const juce::MidiMessage& m, int source);
    void handleGMMidi (const juce::MidiMessage& m);
    void synthNoteOn (int source, int note, int velocity);
    void synthNoteOff (int source, int note);
    void releaseLayer (int layer);
    static int engineChannel (int layer, int slot) { return slot == 0 ? layer : 16 + (slot - 1) * gemi::numLayers + layer; }
    bool layerActive (int layer) const { return applied[(size_t) layer].on && layer < appliedLayers; }
    void setMpe (bool on);
    void updateBend (int slot);
    void updateExpression (int slot);
    void renderSpan (juce::AudioBuffer<float>& buffer, int start, int count);
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
    int appliedVibrato = -1, appliedGlide = -1, appliedMono = -1, appliedMpe = -1, appliedLayers = 1, incomingMod = 0;

    // MPE expression per slot (index 1..15), all -1..1 or 0..1.
    float masterBend = 0.0f;
    std::array<float, numSlots> memberBend {}, memberPressure {}, memberSlide {};

    // Each output group rendered separately, then mixed to the main output or its own bus.
    std::vector<float> scratch;
    int scratchSize = 0;
    std::array<int, numChannels> appliedGmProgram {};
    int appliedKit = -1;
    uint32_t appliedMute = 0, appliedSolo = 0;

    // Which note each layer is playing for a held key: [source][key][layer], -1 = none.
    // Sources 0..15 are MIDI channels, 16 is the on-screen keyboard.
    int8_t layerNote[numChannels + 1][128][gemi::numLayers];
    int8_t noteSlot[numChannels + 1][128];   // the slot a held key plays on
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
    std::atomic<bool> separateOutputs { false };   // GM Player: each channel to its own output
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
