// A single synthesiser voice. Renders either a melodic Patch or a DrumRecipe to mono.
#pragma once

#include "Patch.h"
#include "Drums.h"

#include <vector>

namespace gm
{

// Per-channel values a voice needs while rendering.
struct VoiceControl
{
    float pitchBendSemis = 0.0f;
    float modulation = 0.0f;   // mod wheel + channel pressure, 0..1
};

class Voice
{
public:
    static constexpr int controlInterval = 16; // samples between modulation updates

    void prepare (float sampleRate);

    void startMelodic (const Patch& patch, int channel, int note, float velocity, uint64_t serial);
    void startDrum (const DrumRecipe& drum, int channel, int note, float velocity, uint64_t serial);

    void release();          // note-off (ignored by drums)
    void fadeOut();          // quick ~3 ms fade, used for stealing and exclusive groups
    void reset();            // immediate silence

    // Adds numSamples of mono output into `out`.
    void render (float* out, int numSamples, const VoiceControl& ctl);

    bool isActive() const    { return active; }
    bool isReleased() const  { return released; }
    bool isDrum() const      { return drum != nullptr; }
    bool isFading() const    { return fadeSamples > 0; }
    int  getChannel() const  { return channel; }
    int  getNote() const     { return note; }
    uint64_t getSerial() const { return serial; }
    float getLevel() const   { return amp.level; }
    float getPan() const     { return drum != nullptr ? drum->pan : 0.0f; }
    int  getExclusiveGroup() const { return drum != nullptr ? drum->exclusiveGroup : 0; }

    bool sustainedByPedal = false;

private:
    struct LayerState
    {
        Oscillator osc;
        float phase = 0.0f, modPhase = 0.0f, lastMod = 0.0f;
        float inc = 0.0f, modInc = 0.0f;
        Decay fade, indexEnv;
        int numHarmonics = 8;

        // Karplus-Strong
        std::vector<float> line;
        int writePos = 0;
        float delay = 100.0f, loopGain = 0.99f, smoothing = 0.5f, last = 0.0f;
        float t60 = 1.0f;
    };

    void startCommon (int channel, int note, float velocity, uint64_t serial);
    void updateMelodicControl (const VoiceControl& ctl);
    float renderMelodicSample();
    float renderDrumSample();
    void exciteString (LayerState& s, const Layer& l, float freq);
    void setStringFrequency (LayerState& s, float freq);

    float sampleRate = 44100.0f;
    bool active = false, released = false;
    int channel = 0, note = 60;
    float velocity = 1.0f, velGain = 1.0f;
    uint64_t serial = 0;

    const Patch* patch = nullptr;
    const DrumRecipe* drum = nullptr;

    // melodic
    LayerState layers[2];
    ADSR amp, filterEnv;
    SVF filter, formant[3];
    float formantGain[3] = {};
    Decay pitchEnv;
    float lfoPhase = 0.0f, lfoValue = 0.0f, lfoFadeIn = 0.0f, lfoFadeInc = 0.0f;
    float tremGain = 1.0f, timeScale = 1.0f, fmIndexScale = 1.0f;
    int controlCounter = 0;
    Noise noise;
    DCBlocker dc;

    // drums
    Oscillator toneOsc, tone2Osc, metalOsc[6];
    float metalInc[6] = {};
    Decay sweepEnv, toneEnv, tone2Env, noiseEnv, metalEnv;
    SVF noiseFilter, metalHP1, metalHP2;
    float toneVibPhase = 0.0f, amPhase = 0.0f, attackGain = 1.0f, attackInc = 1.0f;
    int burstsLeft = 0, burstCounter = 0;

    // lifetime
    int fadeSamples = 0, fadeTotal = 1;
    int silentSamples = 0, ageSamples = 0;
    float blockPeak = 0.0f;
};

} // namespace gm
