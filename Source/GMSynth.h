// General MIDI sound module: a FluidSynth instance playing a SoundFont held in memory.
// Has no JUCE dependency so it can also be driven by command-line tools.
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

typedef struct _fluid_hashtable_t fluid_settings_t;
typedef struct _fluid_synth_t fluid_synth_t;

namespace gm
{

// Overrides a channel's filter and volume envelope on every voice it plays, so a synth layer's
// controls set absolute values instead of offsets from whatever each SoundFont preset specifies.
struct VoiceShape
{
    bool enabled = false;
    float filterCents = 13500.0f;   // absolute cutoff in cents (13500 = fully open)
    float filterQ = 0.0f;           // resonance in centibels
    float attack = -12000.0f;       // envelope times in timecents
    float decay = -12000.0f;
    float sustain = 0.0f;           // sustain attenuation in centibels (0 = full level)
    float release = -12000.0f;

    // Exact (bitwise) comparison: used to skip re-applying an unchanged shape.
    bool operator== (const VoiceShape& o) const
    {
        auto same = [] (float x, float y) { return std::memcmp (&x, &y, sizeof (float)) == 0; };
        return enabled == o.enabled && same (filterCents, o.filterCents) && same (filterQ, o.filterQ) && same (attack, o.attack)
            && same (decay, o.decay) && same (sustain, o.sustain) && same (release, o.release);
    }
    bool operator!= (const VoiceShape& o) const { return ! (*this == o); }
};

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

    void noteOn (int channel, int key, int velocity);
    void noteOff (int channel, int key);
    void controlChange (int channel, int controller, int value);
    void pitchBend (int channel, int value14);
    void channelPressure (int channel, int value);
    void keyPressure (int channel, int key, int value);

    // Applies to voices already sounding as well as new ones.
    void setVoiceShape (int channel, const VoiceShape& shape);
    void setFineTune (int channel, float cents);
    // Glide time in milliseconds, 0 = off.
    void setPortamento (int channel, int milliseconds);
    // The first `count` channels play one note at a time (legato); 0 makes every channel polyphonic.
    void setMonoChannels (int count);

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
    void applyShape (int channel, int key, bool newestOnly);

    const void* sfData;
    size_t sfSize;
    bool libraryLoaded;

    fluid_settings_t* settings = nullptr;
    fluid_synth_t* synth = nullptr;
    int sfontId = -1;
    double currentRate = 0.0;

    float gain = 1.0f, reverbLevel = 0.7f, chorusLevel = 0.5f;
    VoiceShape shapes[numChannels];
    std::vector<PresetInfo> presets;
};

} // namespace gm
