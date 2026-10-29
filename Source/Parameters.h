// gemi's host parameters: three synth layers and the global strip. IDs, ranges and defaults match the
// controls in UI/index.html; knob values are 0..1 and are turned into sound in PluginProcessor.cpp
// with the same formulas the UI uses to label them.
#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <cmath>
#include <deque>
#include <string>
#include <vector>

namespace gemi
{

constexpr int numLayers = 3;

enum class Kind { knob, toggle, stepper };

struct ParamSpec
{
    const char* id;
    const char* name;
    Kind kind;
    float defaultValue;
    int minValue = 0, maxValue = 1;   // steppers only
};

inline juce::String layerId (int layer, const char* suffix) { return "l" + juce::String (layer + 1) + "_" + suffix; }

// A fresh layer: gemi starts with layer 1 (a grand piano); layers 2 and 3 start like this when added.
// These match LAYER_DEFAULTS in UI/index.html. The filter starts fully open (and so leaves the sound's
// own filter alone) and the envelope starts as the sound's own; the ADSR values here only apply once
// "ownenv" is turned off.
struct LayerDefaults
{
    float level, pan, cutoff, reso, attack, decay, sustain, release;
    int octave, semi, fine;
    int program;   // 1..128
};

inline const LayerDefaults& layerDefaults (int layer)
{
    static const LayerDefaults d[numLayers] = {
        { 0.80f, 0.50f, 1.00f, 0.00f, 0.00f, 0.90f, 0.25f, 0.15f, 0, 0, 0, 1 },     // Acoustic Grand Piano
        { 0.70f, 0.50f, 1.00f, 0.00f, 0.12f, 0.30f, 0.90f, 0.20f, 0, 0, 0, 49 },    // String Ensemble 1
        { 0.60f, 0.50f, 1.00f, 0.00f, 0.35f, 0.50f, 0.85f, 0.50f, 0, 0, 0, 90 } };  // Pad 2 (warm)
    return d[layer];
}

inline const std::vector<ParamSpec>& paramSpecs()
{
    static const std::vector<ParamSpec> specs = []
    {
        // Names and IDs need stable storage: a deque never moves its elements as it grows.
        static std::deque<std::string> strings;
        auto keep = [] (const juce::String& s) { strings.push_back (s.toStdString()); return strings.back().c_str(); };

        std::vector<ParamSpec> v;
        for (int l = 0; l < numLayers; ++l)
        {
            const auto& d = layerDefaults (l);
            const auto layerName = "Layer " + juce::String (l + 1) + " ";
            auto add = [&] (const char* suffix, const char* name, Kind kind, float def, int lo = 0, int hi = 1)
            {
                v.push_back ({ keep (layerId (l, suffix)), keep (layerName + name), kind, def, lo, hi });
            };
            add ("on", "On", Kind::toggle, 1.0f);
            add ("level", "Level", Kind::knob, d.level);
            add ("pan", "Pan", Kind::knob, d.pan);
            add ("cutoff", "Cutoff", Kind::knob, d.cutoff);
            add ("reso", "Resonance", Kind::knob, d.reso);
            add ("attack", "Attack", Kind::knob, d.attack);
            add ("decay", "Decay", Kind::knob, d.decay);
            add ("sustain", "Sustain", Kind::knob, d.sustain);
            add ("release", "Release", Kind::knob, d.release);
            add ("ownenv", "Own Envelope", Kind::toggle, 1.0f);
            add ("oct", "Octave", Kind::stepper, (float) d.octave, -3, 3);
            add ("semi", "Semitones", Kind::stepper, (float) d.semi, -12, 12);
            add ("fine", "Fine", Kind::stepper, (float) d.fine, -50, 50);
        }
        v.push_back ({ "reverb", "Reverb", Kind::knob, 0.0f });
        v.push_back ({ "chorus", "Chorus", Kind::knob, 0.0f });
        v.push_back ({ "vibrato", "Vibrato", Kind::knob, 0.0f });
        v.push_back ({ "glide", "Glide", Kind::knob, 0.0f });
        v.push_back ({ "volume", "Volume", Kind::knob, 0.80f });
        v.push_back ({ "mono", "Mono", Kind::toggle, 0.0f });
        v.push_back ({ "layers", "Layers", Kind::stepper, 1.0f, 1, numLayers });
        v.push_back ({ "mpe", "MPE", Kind::toggle, 0.0f });
        return v;
    }();
    return specs;
}

//==============================================================================
// Knob value (0..1) to sound, matching the UI's labels.
inline float cutoffHz (float v)      { return 20.0f * std::pow (1000.0f, v); }
inline float glideMs (float v)       { return v < 0.01f ? 0.0f : v * 1000.0f; }
inline float volumeDb (float v)      { return -48.0f + v * 60.0f; }
inline float attackSeconds (float v) { return v * 2.0f; }
inline float decaySeconds (float v)  { return v * 2.0f; }
inline float releaseSeconds (float v){ return v * 3.0f; }

inline juce::String displayText (const juce::String& id, float v)
{
    if (id.endsWith ("_pan"))
    {
        const int p = juce::roundToInt ((v - 0.5f) * 200.0f);
        return p == 0 ? juce::String ("C") : (p < 0 ? "L" + juce::String (-p) : "R" + juce::String (p));
    }
    if (id.endsWith ("_cutoff"))
    {
        const float hz = cutoffHz (v);
        return hz >= 1000.0f ? juce::String (hz / 1000.0f, 1) + "k" : juce::String (juce::roundToInt (hz));
    }
    if (id.endsWith ("_attack"))  return juce::String (attackSeconds (v), 2) + "s";
    if (id.endsWith ("_decay"))   return juce::String (decaySeconds (v), 2) + "s";
    if (id.endsWith ("_release")) return juce::String (releaseSeconds (v), 1) + "s";
    if (id == "glide")  return v < 0.01f ? juce::String ("Off") : juce::String (juce::roundToInt (glideMs (v))) + "ms";
    if (id == "volume") { const float db = volumeDb (v); return (db > 0 ? "+" : "") + juce::String (db, 1) + " dB"; }
    return juce::String (juce::roundToInt (v * 100.0f)) + "%";
}

} // namespace gemi
