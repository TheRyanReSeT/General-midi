// General MIDI sound module: a FluidSynth instance playing a SoundFont held in memory.
// Has no JUCE dependency so it can also be driven by command-line tools.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

typedef struct _fluid_hashtable_t fluid_settings_t;
typedef struct _fluid_synth_t fluid_synth_t;

namespace gm
{

struct PresetInfo
{
    int bank = 0;
    int program = 0;
    std::string name;
};

class GMSynth
{
public:
    static constexpr int numChannels = 16;
    static constexpr int drumChannel = 9;  // MIDI channel 10
    static constexpr int drumBank = 128;

    // `soundFontData` must stay valid for the lifetime of this object.
    GMSynth (const void* soundFontData, size_t soundFontSize);
    ~GMSynth();

    GMSynth (const GMSynth&) = delete;
    GMSynth& operator= (const GMSynth&) = delete;

    // (Re)creates the synth for the given sample rate. Not real-time safe.
    // Returns false if the SoundFont could not be loaded.
    bool prepare (double sampleRate);
    bool isReady() const { return synth != nullptr; }

    // Real-time safe calls (audio thread):
    void handleMidi (const uint8_t* data, int size);
    void render (float* left, float* right, int numSamples);
    void selectProgram (int channel, int bank, int program);
    void allSoundsOff();
    void resetToGM();

    bool getProgram (int channel, int& bank, int& program) const;
    int getActiveVoiceCount() const;

    // Mix settings (thread safe).
    void setGain (float linearGain);
    void setReverbLevel (float level);   // 0..1
    void setChorusLevel (float level);   // 0..1

    // Every preset in the SoundFont, sorted by bank then program.
    const std::vector<PresetInfo>& getPresets() const { return presets; }
    std::string getPresetName (int bank, int program) const;

private:
    void destroy();
    void applyMixSettings();

    const void* sfData;
    size_t sfSize;
    bool libraryLoaded;

    fluid_settings_t* settings = nullptr;
    fluid_synth_t* synth = nullptr;
    int sfontId = -1;
    double currentRate = 0.0;

    float gain = 1.0f, reverbLevel = 0.7f, chorusLevel = 0.5f;
    std::vector<PresetInfo> presets;
};

} // namespace gm
