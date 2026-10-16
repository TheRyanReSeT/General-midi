// Data model describing how one General MIDI instrument is synthesised.
#pragma once

#include "DSP.h"

namespace gm
{

enum class LayerType : uint8_t
{
    None,
    Osc,       // band-limited oscillator
    FM,        // 2-operator FM (sine carrier, sine modulator with feedback)
    Additive,  // up to 8 harmonics (organs, flutes, choirs)
    Pluck,     // Karplus-Strong string
    Noise      // white noise (breath, bow, effects)
};

enum class FilterType : uint8_t { Off, LowPass, HighPass, BandPass, Formant };

struct Layer
{
    LayerType type = LayerType::None;
    Wave wave = Wave::Saw;
    float pulseWidth = 0.5f;
    float semis = 0.0f;        // transposition relative to the played note
    float cents = 0.0f;        // fine detune
    float level = 1.0f;
    float fade = 0.0f;         // > 0: layer decays to silence (~60 dB) over this many seconds

    // FM
    float modRatio = 1.0f;
    float index = 0.0f;        // peak modulation index
    float indexSustain = 1.0f; // fraction of the index left after indexDecay
    float indexDecay = 0.0f;   // seconds; 0 = constant index
    float feedback = 0.0f;
    float carrierRatio = 1.0f;

    // Additive (relative amplitudes of harmonics 1..8)
    float harmonics[8] = { 1, 0, 0, 0, 0, 0, 0, 0 };

    // Pluck
    float pluckDecay = 2.0f;      // seconds to decay 60 dB at middle C (shortens with pitch)
    float pluckBrightness = 0.5f; // 0 = dull, 1 = bright excitation and loop filter
};

struct Patch
{
    const char* name = "";
    Layer layers[2];

    FilterType filter = FilterType::Off;
    float cutoff = 20000.0f;   // Hz, before key tracking and envelope
    float resonance = 0.707f;  // Q
    float keyTrack = 0.0f;     // 1 = cutoff follows pitch exactly
    float filterEnvAmt = 0.0f; // octaves
    float velToCutoff = 0.0f;  // octaves at full velocity
    float fA = 0.001f, fD = 0.3f, fS = 0.0f, fR = 0.3f;
    int vowel = 0;             // for Formant filter: 0 = "ah", 1 = "oo", 2 = "ee"

    float aA = 0.002f, aD = 0.0f, aS = 1.0f, aR = 0.2f; // amp ADSR (aD = 0 => hold)
    float velSens = 0.8f;      // 0 = velocity ignored, 1 = full range

    float pitchEnvSemis = 0.0f; // starting pitch offset, decays to 0
    float pitchEnvTime = 0.0f;

    float lfoRate = 5.0f;       // Hz
    float lfoDelay = 0.0f;      // seconds before vibrato fades in
    float vibrato = 0.0f;       // cents
    float tremolo = 0.0f;       // 0..1
    float lfoToCutoff = 0.0f;   // octaves

    float drive = 0.0f;         // > 0: soft-clip the layer mix before the filter (amp/cabinet)
    float decayKeyScale = 0.0f; // decay times scale by 2^(-x * octaves above middle C)

    float gain = 0.5f;
};

const Patch& getMelodicPatch (int program);
const char* getProgramName (int program);
const char* getFamilyName (int program);

} // namespace gm
