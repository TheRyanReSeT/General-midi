// Synthesis recipes for the General MIDI percussion map (channel 10, notes 35-81).
#pragma once

#include "Patch.h"

namespace gm
{

struct DrumRecipe
{
    const char* name = nullptr;

    // Pitched body: oscillator with an exponential pitch sweep
    float toneLevel = 0.0f;
    Wave  toneWave = Wave::Sine;
    float toneStartHz = 100.0f, toneEndHz = 100.0f, toneSweep = 0.05f;
    float toneDecay = 0.2f;
    float toneVibHz = 0.0f, toneVibDepth = 0.0f; // pitch warble (whistles)

    // Second, fixed partial (cowbell, agogo, triangle, bell)
    float tone2Level = 0.0f, tone2Hz = 0.0f, tone2Decay = 0.2f;
    Wave  tone2Wave = Wave::Sine;

    // Filtered noise
    float noiseLevel = 0.0f, noiseDecay = 0.1f;
    FilterType noiseFilter = FilterType::Off;
    float noiseCutoff = 5000.0f, noiseQ = 0.707f;

    // Metallic cluster: six square waves at inharmonic ratios, high-passed
    float metalLevel = 0.0f, metalDecay = 0.1f, metalScale = 1.0f, metalHighPass = 7000.0f;

    // Envelope shaping
    float attack = 0.0005f;     // seconds
    int   bursts = 1;           // >1 = repeated attacks (hand clap)
    float burstSpacing = 0.01f; // seconds between bursts
    float amRate = 0.0f;        // amplitude modulation rate (guiro, vibraslap)

    float pan = 0.0f;           // -1 left .. +1 right
    int   exclusiveGroup = 0;   // notes in the same group cut each other off
    float gain = 1.0f;
};

// Returns nullptr for notes outside the kit.
const DrumRecipe* getDrum (int note);

} // namespace gm
