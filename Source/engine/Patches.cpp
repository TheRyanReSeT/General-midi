// The 128 General MIDI melodic instruments, each defined as a synthesis recipe.
#include "Patch.h"

#include <array>
#include <initializer_list>

namespace gm
{
namespace
{

//==============================================================================
// Layer helpers

Layer osc (Wave w, float level = 1.0f, float semis = 0.0f, float cents = 0.0f)
{
    Layer l;
    l.type = LayerType::Osc;
    l.wave = w;
    l.level = level;
    l.semis = semis;
    l.cents = cents;
    return l;
}

Layer pulse (float width, float level = 1.0f, float semis = 0.0f, float cents = 0.0f)
{
    Layer l = osc (Wave::Pulse, level, semis, cents);
    l.pulseWidth = width;
    return l;
}

Layer fm (float modRatio, float index, float indexDecay = 0.0f, float indexSustain = 1.0f,
          float level = 1.0f, float feedback = 0.0f)
{
    Layer l;
    l.type = LayerType::FM;
    l.modRatio = modRatio;
    l.index = index;
    l.indexDecay = indexDecay;
    l.indexSustain = indexSustain;
    l.level = level;
    l.feedback = feedback;
    return l;
}

Layer additive (std::initializer_list<float> harmonics, float level = 1.0f, float semis = 0.0f)
{
    Layer l;
    l.type = LayerType::Additive;
    l.level = level;
    l.semis = semis;
    int i = 0;
    for (auto h : harmonics)
        if (i < 8) l.harmonics[i++] = h;
    for (; i < 8; ++i) l.harmonics[i] = 0.0f;
    return l;
}

Layer pluck (float decay, float brightness, float level = 1.0f, float semis = 0.0f, float cents = 0.0f)
{
    Layer l;
    l.type = LayerType::Pluck;
    l.pluckDecay = decay;
    l.pluckBrightness = brightness;
    l.level = level;
    l.semis = semis;
    l.cents = cents;
    return l;
}

Layer noise (float level, float fade = 0.0f)
{
    Layer l;
    l.type = LayerType::Noise;
    l.level = level;
    l.fade = fade;
    return l;
}

Layer faded (Layer l, float seconds) { l.fade = seconds; return l; }
Layer shifted (Layer l, float semis, float cents = 0.0f) { l.semis = semis; l.cents = cents; return l; }

//==============================================================================
// Fluent patch builder

struct P
{
    Patch p;

    explicit P (const char* name) { p.name = name; }

    P& L (Layer a)          { p.layers[0] = a; return *this; }
    P& L (Layer a, Layer b) { p.layers[0] = a; p.layers[1] = b; return *this; }

    P& amp (float a, float d, float s, float r) { p.aA = a; p.aD = d; p.aS = s; p.aR = r; return *this; }

    P& filt (FilterType t, float cutoff, float q = 0.707f, float keyTrack = 0.0f)
    {
        p.filter = t; p.cutoff = cutoff; p.resonance = q; p.keyTrack = keyTrack;
        return *this;
    }
    P& lp (float cutoff, float q = 0.707f, float keyTrack = 0.0f) { return filt (FilterType::LowPass, cutoff, q, keyTrack); }
    P& hp (float cutoff, float q = 0.707f, float keyTrack = 0.0f) { return filt (FilterType::HighPass, cutoff, q, keyTrack); }
    P& bp (float cutoff, float q = 1.0f, float keyTrack = 0.0f)   { return filt (FilterType::BandPass, cutoff, q, keyTrack); }
    P& formant (int vowel) { p.filter = FilterType::Formant; p.vowel = vowel; return *this; }

    P& fenv (float octaves, float a, float d, float s, float r)
    {
        p.filterEnvAmt = octaves; p.fA = a; p.fD = d; p.fS = s; p.fR = r;
        return *this;
    }
    P& vel (float sens, float toCutoff = 0.0f) { p.velSens = sens; p.velToCutoff = toCutoff; return *this; }
    P& vib (float cents, float rate = 5.5f, float delay = 0.3f) { p.vibrato = cents; p.lfoRate = rate; p.lfoDelay = delay; return *this; }
    P& trem (float depth, float rate) { p.tremolo = depth; p.lfoRate = rate; return *this; }
    P& lfoFilter (float octaves, float rate) { p.lfoToCutoff = octaves; p.lfoRate = rate; return *this; }
    P& pitchEnv (float semis, float time) { p.pitchEnvSemis = semis; p.pitchEnvTime = time; return *this; }
    P& drive (float amount) { p.drive = amount; return *this; }
    P& keyScale (float amount) { p.decayKeyScale = amount; return *this; }
    P& gain (float g) { p.gain = g; return *this; }

    operator Patch() const { return p; }
};

using W = Wave;

//==============================================================================
std::array<Patch, 128> buildPatches()
{
    std::array<Patch, 128> t {};
    int i = 0;
    auto add = [&] (const Patch& p) { t[(size_t) i++] = p; };

    // --- Piano (0-7)
    add (P ("Acoustic Grand Piano").L (fm (1.0f, 2.6f, 1.8f, 0.12f), pluck (5.0f, 0.55f, 0.35f))
            .lp (900, 0.707f, 0.8f).fenv (3.0f, 0.001f, 1.6f, 0.0f, 0.3f).vel (0.9f, 2.0f)
            .amp (0.001f, 7.0f, 0.0f, 0.35f).keyScale (0.45f).gain (0.55f));
    add (P ("Bright Acoustic Piano").L (fm (1.0f, 3.2f, 1.2f, 0.2f), pluck (4.0f, 0.8f, 0.4f))
            .lp (1800, 0.707f, 0.8f).fenv (3.0f, 0.001f, 1.2f, 0.0f, 0.3f).vel (0.9f, 1.5f)
            .amp (0.001f, 6.0f, 0.0f, 0.3f).keyScale (0.45f).gain (0.5f));
    add (P ("Electric Grand Piano").L (fm (1.0f, 2.0f, 1.0f, 0.25f), faded (fm (3.0f, 1.2f, 0.4f, 0.1f, 0.3f), 1.0f))
            .lp (2500, 0.707f, 0.5f).vel (0.85f, 1.5f).amp (0.001f, 5.0f, 0.0f, 0.3f).keyScale (0.4f).gain (0.5f));
    add (P ("Honky-tonk Piano").L (shifted (fm (1.0f, 2.8f, 1.2f, 0.15f), 0, -14.0f), shifted (fm (1.0f, 2.8f, 1.2f, 0.15f), 0, 14.0f))
            .lp (1400, 0.707f, 0.8f).fenv (2.5f, 0.001f, 1.0f, 0.0f, 0.3f).vel (0.9f, 1.5f)
            .amp (0.001f, 5.0f, 0.0f, 0.3f).keyScale (0.45f).gain (0.4f));
    add (P ("Electric Piano 1").L (fm (1.0f, 1.3f, 1.5f, 0.3f), faded (fm (14.0f, 1.4f, 0.0f, 1.0f, 0.22f), 0.35f))
            .vel (0.9f).amp (0.001f, 4.5f, 0.0f, 0.4f).keyScale (0.35f).gain (0.6f));
    add (P ("Electric Piano 2").L (fm (1.0f, 2.4f, 1.0f, 0.35f), faded (shifted (fm (14.0f, 2.0f, 0.0f, 1.0f, 0.3f), 0, 7.0f), 0.5f))
            .vel (0.9f).amp (0.001f, 3.5f, 0.0f, 0.4f).keyScale (0.35f).gain (0.5f));
    add (P ("Harpsichord").L (pluck (2.5f, 0.97f), pluck (1.8f, 0.95f, 0.35f, 12.0f))
            .hp (120).vel (0.3f).amp (0.001f, 0.0f, 1.0f, 0.12f).keyScale (0.3f).gain (0.45f));
    add (P ("Clavi").L (pulse (0.22f)).lp (700, 2.5f, 0.6f).fenv (3.5f, 0.001f, 0.25f, 0.3f, 0.1f).vel (0.8f, 1.5f)
            .amp (0.001f, 1.6f, 0.15f, 0.06f).keyScale (0.3f).gain (0.4f));

    // --- Chromatic Percussion (8-15)
    add (P ("Celesta").L (fm (4.0f, 1.4f, 0.35f, 0.1f), osc (W::Sine, 0.6f))
            .amp (0.001f, 1.8f, 0.0f, 0.4f).keyScale (0.4f).gain (0.5f));
    add (P ("Glockenspiel").L (shifted (fm (3.5f, 1.8f, 0.4f, 0.1f), 12), faded (osc (W::Sine, 0.5f, 24.0f), 0.6f))
            .amp (0.001f, 2.0f, 0.0f, 0.6f).keyScale (0.3f).gain (0.4f));
    add (P ("Music Box").L (fm (5.0f, 1.4f, 0.25f, 0.05f), osc (W::Sine, 0.6f, 12.0f))
            .amp (0.001f, 1.6f, 0.0f, 0.5f).keyScale (0.3f).gain (0.4f));
    add (P ("Vibraphone").L (osc (W::Sine), faded (fm (4.0f, 1.2f, 0.25f, 0.0f, 0.5f), 0.4f))
            .trem (0.35f, 5.5f).amp (0.001f, 3.5f, 0.0f, 0.6f).keyScale (0.3f).gain (0.5f));
    add (P ("Marimba").L (osc (W::Sine), faded (fm (4.0f, 2.0f, 0.04f, 0.0f, 0.4f), 0.08f))
            .amp (0.001f, 0.7f, 0.0f, 0.3f).keyScale (0.5f).gain (0.6f));
    add (P ("Xylophone").L (shifted (fm (3.0f, 2.2f, 0.06f, 0.1f), 12), faded (osc (W::Sine, 0.4f, 24.0f), 0.1f))
            .amp (0.001f, 0.4f, 0.0f, 0.2f).keyScale (0.4f).gain (0.5f));
    add (P ("Tubular Bells").L (fm (3.5f, 3.0f, 2.0f, 0.15f), faded (fm (1.41f, 1.5f, 1.0f, 0.2f, 0.4f), 2.0f))
            .amp (0.001f, 6.0f, 0.0f, 1.5f).keyScale (0.2f).gain (0.4f));
    add (P ("Dulcimer").L (pluck (3.0f, 0.85f), pluck (3.0f, 0.85f, 0.7f, 0.0f, 9.0f))
            .amp (0.001f, 0.0f, 1.0f, 0.5f).keyScale (0.3f).gain (0.4f));

    // --- Organ (16-23)
    add (P ("Drawbar Organ").L (additive ({ 1.0f, 0.8f, 0.6f, 0.5f, 0.0f, 0.3f, 0.0f, 0.2f }))
            .vel (0.0f).amp (0.005f, 0.0f, 1.0f, 0.05f).gain (0.3f));
    add (P ("Percussive Organ").L (additive ({ 1.0f, 0.6f, 0.0f, 0.4f }), faded (additive ({ 0.0f, 0.0f, 1.0f }, 0.8f), 0.35f))
            .vel (0.2f).amp (0.003f, 0.0f, 1.0f, 0.05f).gain (0.35f));
    add (P ("Rock Organ").L (additive ({ 1.0f, 1.0f, 1.0f, 0.6f, 0.3f, 0.3f, 0.0f, 0.4f }))
            .lp (5000).drive (1.8f).trem (0.25f, 6.5f).vel (0.0f).amp (0.004f, 0.0f, 1.0f, 0.05f).gain (0.28f));
    add (P ("Church Organ").L (additive ({ 1.0f, 0.7f, 0.4f, 0.5f, 0.2f, 0.25f, 0.0f, 0.3f }), additive ({ 1.0f, 0.5f, 0.0f, 0.2f }, 0.7f, -12.0f))
            .vel (0.0f).amp (0.08f, 0.0f, 1.0f, 0.5f).gain (0.25f));
    add (P ("Reed Organ").L (pulse (0.3f), osc (W::Saw, 0.4f, 0.0f, 4.0f))
            .lp (1800, 0.8f, 0.5f).vel (0.2f).amp (0.05f, 0.0f, 1.0f, 0.12f).gain (0.3f));
    add (P ("Accordion").L (osc (W::Saw, 1.0f, 0.0f, -11.0f), osc (W::Saw, 1.0f, 0.0f, 11.0f))
            .lp (2400, 1.2f, 0.4f).vel (0.3f).trem (0.08f, 5.0f).amp (0.03f, 0.0f, 1.0f, 0.1f).gain (0.28f));
    add (P ("Harmonica").L (pulse (0.2f), noise (0.06f))
            .lp (2300, 2.0f, 0.4f).fenv (0.8f, 0.04f, 0.3f, 0.5f, 0.1f).vib (14.0f, 5.0f, 0.35f)
            .vel (0.4f).amp (0.03f, 0.0f, 1.0f, 0.1f).gain (0.35f));
    add (P ("Tango Accordion").L (osc (W::Saw, 1.0f, 0.0f, -6.0f), pulse (0.35f, 0.8f, 0.0f, 7.0f))
            .lp (2000, 1.0f, 0.4f).vel (0.3f).amp (0.03f, 0.0f, 1.0f, 0.1f).gain (0.3f));

    // --- Guitar (24-31)
    add (P ("Acoustic Guitar (nylon)").L (pluck (3.0f, 0.45f)).lp (3000, 0.707f, 0.3f).vel (0.8f)
            .amp (0.001f, 0.0f, 1.0f, 0.25f).keyScale (0.2f).gain (0.6f));
    add (P ("Acoustic Guitar (steel)").L (pluck (4.0f, 0.75f), pluck (4.0f, 0.75f, 0.5f, 0.0f, 5.0f))
            .lp (5000).vel (0.8f).amp (0.001f, 0.0f, 1.0f, 0.25f).keyScale (0.2f).gain (0.45f));
    add (P ("Electric Guitar (jazz)").L (pluck (3.0f, 0.4f)).lp (1500, 0.8f, 0.3f).vel (0.8f)
            .amp (0.001f, 0.0f, 1.0f, 0.2f).keyScale (0.2f).gain (0.65f));
    add (P ("Electric Guitar (clean)").L (pluck (4.0f, 0.62f)).lp (3500, 1.0f, 0.3f).vel (0.8f)
            .amp (0.001f, 0.0f, 1.0f, 0.2f).keyScale (0.2f).gain (0.6f));
    add (P ("Electric Guitar (muted)").L (pluck (0.25f, 0.5f)).lp (1600, 0.9f, 0.3f).vel (0.8f)
            .amp (0.001f, 0.0f, 1.0f, 0.08f).gain (0.7f));
    add (P ("Overdriven Guitar").L (pluck (7.0f, 0.7f), pluck (7.0f, 0.7f, 0.5f, 0.0f, 8.0f))
            .drive (6.0f).lp (2800, 1.2f).vel (0.4f).amp (0.001f, 0.0f, 1.0f, 0.15f).gain (0.3f));
    add (P ("Distortion Guitar").L (pluck (9.0f, 0.8f), pluck (9.0f, 0.8f, 0.4f, 12.0f))
            .drive (16.0f).lp (3000, 1.3f).vel (0.3f).amp (0.001f, 0.0f, 1.0f, 0.15f).gain (0.28f));
    add (P ("Guitar Harmonics").L (faded (osc (W::Sine, 1.0f, 12.0f), 2.5f), pluck (1.0f, 0.3f, 0.25f, 12.0f))
            .amp (0.001f, 0.0f, 1.0f, 0.3f).keyScale (0.2f).gain (0.5f));

    // --- Bass (32-39)
    add (P ("Acoustic Bass").L (pluck (2.5f, 0.3f), faded (osc (W::Sine, 0.5f), 1.5f))
            .lp (1100, 0.707f, 0.3f).amp (0.001f, 0.0f, 1.0f, 0.12f).gain (0.75f));
    add (P ("Electric Bass (finger)").L (pluck (3.0f, 0.45f), faded (osc (W::Sine, 0.4f), 2.0f))
            .lp (1500, 0.8f, 0.3f).amp (0.001f, 0.0f, 1.0f, 0.1f).gain (0.7f));
    add (P ("Electric Bass (pick)").L (pluck (2.5f, 0.75f), faded (osc (W::Sine, 0.3f), 1.5f))
            .lp (2600, 1.0f, 0.3f).amp (0.001f, 0.0f, 1.0f, 0.1f).gain (0.65f));
    add (P ("Fretless Bass").L (osc (W::Triangle), osc (W::Saw, 0.4f))
            .lp (500, 1.2f, 0.5f).fenv (1.5f, 0.01f, 0.6f, 0.2f, 0.2f).vel (0.7f, 1.0f)
            .amp (0.01f, 3.0f, 0.25f, 0.12f).gain (0.6f));
    add (P ("Slap Bass 1").L (pluck (1.6f, 0.9f), faded (fm (1.0f, 3.0f, 0.05f, 0.0f, 0.4f), 0.15f))
            .lp (4000, 1.2f).amp (0.001f, 0.0f, 1.0f, 0.1f).gain (0.6f));
    add (P ("Slap Bass 2").L (pluck (1.3f, 0.95f), faded (fm (2.0f, 3.5f, 0.04f, 0.0f, 0.4f), 0.12f))
            .lp (5000, 1.5f).amp (0.001f, 0.0f, 1.0f, 0.1f).gain (0.55f));
    add (P ("Synth Bass 1").L (osc (W::Saw), osc (W::Square, 0.5f, -12.0f))
            .lp (250, 4.0f, 0.3f).fenv (4.0f, 0.001f, 0.25f, 0.1f, 0.1f).vel (0.6f, 1.0f)
            .amp (0.001f, 0.6f, 0.7f, 0.08f).gain (0.4f));
    add (P ("Synth Bass 2").L (pulse (0.3f), osc (W::Saw, 0.6f, 0.0f, 8.0f))
            .lp (450, 2.0f, 0.3f).fenv (3.0f, 0.001f, 0.4f, 0.2f, 0.1f).vel (0.6f, 1.0f)
            .amp (0.001f, 0.8f, 0.6f, 0.08f).gain (0.4f));

    // --- Strings (40-47)
    add (P ("Violin").L (osc (W::Saw, 1.0f, 0.0f, -5.0f), osc (W::Saw, 0.8f, 0.0f, 5.0f))
            .lp (3000, 1.3f, 0.5f).vib (18.0f, 5.6f, 0.25f).vel (0.5f, 0.8f).amp (0.08f, 0.0f, 1.0f, 0.25f).gain (0.3f));
    add (P ("Viola").L (osc (W::Saw, 1.0f, 0.0f, -5.0f), osc (W::Saw, 0.8f, 0.0f, 5.0f))
            .lp (2200, 1.3f, 0.5f).vib (16.0f, 5.3f, 0.3f).vel (0.5f, 0.8f).amp (0.09f, 0.0f, 1.0f, 0.28f).gain (0.32f));
    add (P ("Cello").L (osc (W::Saw, 1.0f, 0.0f, -4.0f), osc (W::Saw, 0.8f, 0.0f, 4.0f))
            .lp (1600, 1.3f, 0.5f).vib (14.0f, 5.0f, 0.3f).vel (0.5f, 0.8f).amp (0.1f, 0.0f, 1.0f, 0.3f).gain (0.35f));
    add (P ("Contrabass").L (osc (W::Saw, 1.0f, 0.0f, -4.0f), osc (W::Saw, 0.8f, 0.0f, 4.0f))
            .lp (900, 1.2f, 0.5f).vib (10.0f, 4.8f, 0.35f).vel (0.5f, 0.8f).amp (0.12f, 0.0f, 1.0f, 0.3f).gain (0.4f));
    add (P ("Tremolo Strings").L (osc (W::Saw, 1.0f, 0.0f, -9.0f), osc (W::Saw, 1.0f, 0.0f, 9.0f))
            .lp (2800, 1.0f, 0.4f).trem (0.6f, 8.0f).amp (0.05f, 0.0f, 1.0f, 0.35f).gain (0.3f));
    add (P ("Pizzicato Strings").L (pluck (0.6f, 0.5f), pluck (0.6f, 0.5f, 0.7f, 0.0f, 8.0f))
            .lp (2500, 0.707f, 0.4f).amp (0.001f, 0.0f, 1.0f, 0.15f).keyScale (0.3f).gain (0.5f));
    add (P ("Orchestral Harp").L (pluck (5.0f, 0.55f)).lp (4000, 0.707f, 0.3f)
            .amp (0.001f, 0.0f, 1.0f, 1.2f).keyScale (0.3f).gain (0.6f));
    add (P ("Timpani").L (fm (1.5f, 2.0f, 0.15f, 0.1f), noise (0.3f, 0.08f))
            .lp (1200, 0.707f, 0.3f).pitchEnv (1.0f, 0.06f).amp (0.001f, 2.5f, 0.0f, 1.5f).keyScale (0.2f).gain (0.7f));

    // --- Ensemble (48-55)
    add (P ("String Ensemble 1").L (osc (W::Saw, 1.0f, 0.0f, -10.0f), osc (W::Saw, 1.0f, 0.0f, 10.0f))
            .lp (3500, 0.9f, 0.3f).vib (8.0f, 5.0f, 0.5f).vel (0.5f, 0.5f).amp (0.15f, 0.0f, 1.0f, 0.5f).gain (0.28f));
    add (P ("String Ensemble 2").L (osc (W::Saw, 1.0f, 0.0f, -12.0f), osc (W::Saw, 1.0f, 0.0f, 12.0f))
            .lp (2600, 0.9f, 0.3f).vib (8.0f, 4.8f, 0.7f).vel (0.4f, 0.4f).amp (0.5f, 0.0f, 1.0f, 0.8f).gain (0.28f));
    add (P ("SynthStrings 1").L (osc (W::Saw, 1.0f, 0.0f, -14.0f), osc (W::Saw, 1.0f, 0.0f, 14.0f))
            .lp (1800, 1.5f, 0.3f).fenv (1.2f, 0.3f, 1.0f, 0.5f, 0.5f).amp (0.1f, 0.0f, 1.0f, 0.5f).gain (0.28f));
    add (P ("SynthStrings 2").L (pulse (0.3f, 1.0f, 0.0f, -10.0f), pulse (0.4f, 1.0f, 0.0f, 10.0f))
            .lp (1800, 1.0f, 0.3f).fenv (1.0f, 0.5f, 1.0f, 0.6f, 0.7f).amp (0.3f, 0.0f, 1.0f, 0.7f).gain (0.28f));
    add (P ("Choir Aahs").L (osc (W::Saw, 1.0f, 0.0f, -8.0f), osc (W::Saw, 1.0f, 0.0f, 8.0f))
            .formant (0).vib (10.0f, 5.0f, 0.3f).vel (0.5f).amp (0.2f, 0.0f, 1.0f, 0.5f).gain (0.5f));
    add (P ("Voice Oohs").L (osc (W::Triangle, 1.0f, 0.0f, -6.0f), osc (W::Saw, 0.5f, 0.0f, 6.0f))
            .formant (1).vib (10.0f, 5.0f, 0.3f).vel (0.5f).amp (0.15f, 0.0f, 1.0f, 0.4f).gain (0.7f));
    add (P ("Synth Voice").L (osc (W::Saw, 1.0f, 0.0f, -7.0f), pulse (0.4f, 0.6f, 0.0f, 7.0f))
            .formant (2).vib (6.0f, 4.5f, 0.4f).amp (0.1f, 0.0f, 1.0f, 0.5f).gain (0.5f));
    add (P ("Orchestra Hit").L (osc (W::Saw, 1.0f, 0.0f, -8.0f), osc (W::Saw, 0.8f, 7.0f, 8.0f))
            .lp (2500, 1.0f, 0.3f).fenv (1.5f, 0.001f, 0.3f, 0.0f, 0.2f).pitchEnv (-0.5f, 0.1f)
            .amp (0.002f, 0.5f, 0.0f, 0.3f).gain (0.45f));

    // --- Brass (56-63)
    add (P ("Trumpet").L (osc (W::Saw)).lp (700, 1.0f, 0.5f).fenv (2.5f, 0.03f, 0.3f, 0.55f, 0.1f).vel (0.6f, 1.5f)
            .vib (12.0f, 5.5f, 0.4f).pitchEnv (-0.4f, 0.04f).amp (0.02f, 0.0f, 1.0f, 0.1f).gain (0.4f));
    add (P ("Trombone").L (osc (W::Saw)).lp (500, 1.0f, 0.5f).fenv (2.2f, 0.05f, 0.35f, 0.5f, 0.15f).vel (0.6f, 1.5f)
            .vib (8.0f, 5.0f, 0.5f).amp (0.04f, 0.0f, 1.0f, 0.15f).gain (0.45f));
    add (P ("Tuba").L (osc (W::Saw), osc (W::Sine, 0.6f)).lp (300, 0.9f, 0.5f).fenv (2.0f, 0.05f, 0.3f, 0.4f, 0.15f).vel (0.6f, 1.2f)
            .amp (0.05f, 0.0f, 1.0f, 0.15f).gain (0.55f));
    add (P ("Muted Trumpet").L (osc (W::Saw)).bp (1400, 3.0f, 0.4f).vel (0.6f).vib (8.0f, 5.5f, 0.4f)
            .amp (0.02f, 0.0f, 1.0f, 0.1f).gain (0.7f));
    add (P ("French Horn").L (osc (W::Saw), osc (W::Triangle, 0.5f)).lp (450, 0.7f, 0.5f).fenv (1.5f, 0.07f, 0.4f, 0.5f, 0.2f).vel (0.6f, 1.0f)
            .vib (5.0f, 5.0f, 0.5f).amp (0.06f, 0.0f, 1.0f, 0.2f).gain (0.5f));
    add (P ("Brass Section").L (osc (W::Saw, 1.0f, 0.0f, -8.0f), osc (W::Saw, 1.0f, 0.0f, 8.0f))
            .lp (800, 1.0f, 0.4f).fenv (2.5f, 0.04f, 0.4f, 0.5f, 0.15f).vel (0.6f, 1.2f).amp (0.04f, 0.0f, 1.0f, 0.15f).gain (0.3f));
    add (P ("SynthBrass 1").L (osc (W::Saw, 1.0f, 0.0f, -10.0f), osc (W::Saw, 1.0f, 0.0f, 10.0f))
            .lp (500, 2.0f, 0.4f).fenv (3.5f, 0.005f, 0.4f, 0.4f, 0.15f).vel (0.6f, 1.0f).amp (0.01f, 0.0f, 1.0f, 0.15f).gain (0.3f));
    add (P ("SynthBrass 2").L (pulse (0.4f), osc (W::Saw, 0.8f, 0.0f, 9.0f))
            .lp (450, 1.5f, 0.4f).fenv (3.0f, 0.2f, 0.6f, 0.5f, 0.3f).vel (0.5f, 1.0f).amp (0.08f, 0.0f, 1.0f, 0.25f).gain (0.3f));

    // --- Reed (64-71)
    add (P ("Soprano Sax").L (pulse (0.4f), noise (0.04f)).lp (1800, 2.0f, 0.6f).fenv (1.5f, 0.03f, 0.3f, 0.5f, 0.1f).vel (0.6f, 1.2f)
            .vib (14.0f, 5.5f, 0.3f).amp (0.025f, 0.0f, 1.0f, 0.1f).gain (0.4f));
    add (P ("Alto Sax").L (pulse (0.35f), noise (0.04f)).lp (1400, 2.2f, 0.6f).fenv (1.5f, 0.03f, 0.3f, 0.5f, 0.1f).vel (0.6f, 1.2f)
            .vib (14.0f, 5.3f, 0.3f).amp (0.025f, 0.0f, 1.0f, 0.1f).gain (0.42f));
    add (P ("Tenor Sax").L (osc (W::Saw), noise (0.04f)).lp (1000, 2.5f, 0.6f).fenv (1.5f, 0.03f, 0.3f, 0.5f, 0.1f).vel (0.6f, 1.2f)
            .vib (12.0f, 5.0f, 0.3f).amp (0.03f, 0.0f, 1.0f, 0.12f).gain (0.42f));
    add (P ("Baritone Sax").L (osc (W::Saw), noise (0.03f)).lp (700, 2.5f, 0.6f).fenv (1.5f, 0.03f, 0.3f, 0.5f, 0.1f).vel (0.6f, 1.2f)
            .vib (10.0f, 4.8f, 0.35f).amp (0.03f, 0.0f, 1.0f, 0.12f).gain (0.45f));
    add (P ("Oboe").L (pulse (0.14f)).lp (2800, 1.8f, 0.4f).vel (0.5f, 0.5f).vib (10.0f, 5.5f, 0.3f)
            .amp (0.03f, 0.0f, 1.0f, 0.08f).gain (0.4f));
    add (P ("English Horn").L (pulse (0.2f)).lp (1800, 1.8f, 0.4f).vel (0.5f, 0.5f).vib (9.0f, 5.2f, 0.3f)
            .amp (0.04f, 0.0f, 1.0f, 0.1f).gain (0.42f));
    add (P ("Bassoon").L (pulse (0.25f)).lp (800, 2.0f, 0.4f).vel (0.5f, 0.5f).vib (6.0f, 5.0f, 0.4f)
            .amp (0.04f, 0.0f, 1.0f, 0.1f).gain (0.5f));
    add (P ("Clarinet").L (osc (W::Square), noise (0.02f)).lp (1600, 1.0f, 0.6f).fenv (0.8f, 0.04f, 0.3f, 0.4f, 0.1f).vel (0.5f, 0.6f)
            .vib (4.0f, 5.0f, 0.4f).amp (0.03f, 0.0f, 1.0f, 0.08f).gain (0.4f));

    // --- Pipe (72-79)
    add (P ("Piccolo").L (additive ({ 1.0f, 0.1f, 0.05f }, 1.0f, 12.0f), noise (0.05f))
            .lp (8000).vib (12.0f, 5.5f, 0.2f).vel (0.4f).amp (0.04f, 0.0f, 1.0f, 0.1f).gain (0.45f));
    add (P ("Flute").L (additive ({ 1.0f, 0.15f, 0.08f, 0.03f }), noise (0.06f))
            .lp (5000).vib (10.0f, 5.0f, 0.25f).vel (0.4f).amp (0.05f, 0.0f, 1.0f, 0.12f).gain (0.5f));
    add (P ("Recorder").L (additive ({ 1.0f, 0.2f, 0.12f, 0.0f, 0.05f }), noise (0.03f, 0.1f))
            .lp (5000).vel (0.4f).amp (0.03f, 0.0f, 1.0f, 0.08f).gain (0.5f));
    add (P ("Pan Flute").L (additive ({ 1.0f, 0.1f, 0.05f }), noise (0.15f))
            .lp (3500, 0.707f, 0.5f).vib (8.0f, 4.5f, 0.3f).pitchEnv (-0.3f, 0.08f).vel (0.4f)
            .amp (0.06f, 0.0f, 1.0f, 0.15f).gain (0.5f));
    add (P ("Blown Bottle").L (osc (W::Sine, 0.7f), noise (0.6f))
            .bp (261.6f, 3.0f, 1.0f).vel (0.4f).amp (0.08f, 0.0f, 1.0f, 0.15f).gain (0.6f));
    add (P ("Shakuhachi").L (additive ({ 1.0f, 0.2f, 0.1f }), noise (0.2f))
            .lp (3000, 0.707f, 0.5f).vib (22.0f, 5.0f, 0.3f).pitchEnv (-1.0f, 0.15f).vel (0.5f)
            .amp (0.1f, 0.0f, 1.0f, 0.2f).gain (0.5f));
    add (P ("Whistle").L (osc (W::Sine, 1.0f, 12.0f)).vib (15.0f, 6.0f, 0.15f).vel (0.4f)
            .amp (0.03f, 0.0f, 1.0f, 0.08f).gain (0.45f));
    add (P ("Ocarina").L (additive ({ 1.0f, 0.0f, 0.05f })).vib (6.0f, 5.0f, 0.3f).vel (0.4f)
            .amp (0.03f, 0.0f, 1.0f, 0.08f).gain (0.5f));

    // --- Synth Lead (80-87)
    add (P ("Lead 1 (square)").L (osc (W::Square), osc (W::Square, 0.5f, 0.0f, 6.0f))
            .lp (4000, 1.0f, 0.3f).vel (0.4f).amp (0.002f, 0.0f, 1.0f, 0.08f).gain (0.25f));
    add (P ("Lead 2 (sawtooth)").L (osc (W::Saw, 1.0f, 0.0f, -6.0f), osc (W::Saw, 1.0f, 0.0f, 6.0f))
            .lp (4500, 1.2f, 0.3f).vel (0.4f).amp (0.002f, 0.0f, 1.0f, 0.08f).gain (0.22f));
    add (P ("Lead 3 (calliope)").L (additive ({ 1.0f, 0.4f, 0.15f, 0.05f }), noise (0.08f))
            .lp (5000).vib (10.0f, 6.0f, 0.2f).vel (0.4f).amp (0.02f, 0.0f, 1.0f, 0.08f).gain (0.4f));
    add (P ("Lead 4 (chiff)").L (osc (W::Triangle), noise (0.5f, 0.06f))
            .lp (3500, 0.707f, 0.4f).vel (0.4f).amp (0.005f, 0.0f, 1.0f, 0.1f).gain (0.45f));
    add (P ("Lead 5 (charang)").L (osc (W::Saw), pulse (0.3f, 0.7f, 0.0f, 7.0f))
            .drive (3.0f).lp (3000, 1.5f, 0.3f).fenv (1.0f, 0.001f, 0.3f, 0.4f, 0.1f).vel (0.4f)
            .amp (0.002f, 0.0f, 1.0f, 0.1f).gain (0.25f));
    add (P ("Lead 6 (voice)").L (osc (W::Saw), osc (W::Triangle, 0.6f))
            .formant (0).vib (8.0f, 5.0f, 0.3f).vel (0.4f).amp (0.05f, 0.0f, 1.0f, 0.15f).gain (0.55f));
    add (P ("Lead 7 (fifths)").L (osc (W::Saw), osc (W::Saw, 0.8f, 7.0f))
            .lp (3000, 1.2f, 0.3f).vel (0.4f).amp (0.002f, 0.0f, 1.0f, 0.08f).gain (0.22f));
    add (P ("Lead 8 (bass + lead)").L (osc (W::Saw), osc (W::Square, 0.8f, -12.0f))
            .lp (1500, 2.0f, 0.4f).fenv (2.0f, 0.001f, 0.3f, 0.4f, 0.1f).vel (0.5f, 0.8f)
            .amp (0.002f, 0.0f, 1.0f, 0.08f).gain (0.22f));

    // --- Synth Pad (88-95)
    add (P ("Pad 1 (new age)").L (faded (fm (3.5f, 1.5f, 0.6f, 0.2f, 0.7f), 2.0f), osc (W::Saw, 0.6f, 0.0f, 8.0f))
            .lp (1500, 1.0f, 0.3f).amp (0.05f, 0.0f, 1.0f, 1.2f).gain (0.35f));
    add (P ("Pad 2 (warm)").L (osc (W::Saw, 1.0f, 0.0f, -12.0f), osc (W::Saw, 1.0f, 0.0f, 12.0f))
            .lp (1000, 0.9f, 0.3f).vib (5.0f, 4.0f, 0.6f).amp (0.4f, 0.0f, 1.0f, 1.0f).gain (0.3f));
    add (P ("Pad 3 (polysynth)").L (osc (W::Saw, 1.0f, 0.0f, -8.0f), osc (W::Saw, 1.0f, 0.0f, 8.0f))
            .lp (1200, 1.3f, 0.3f).fenv (2.0f, 0.01f, 0.8f, 0.3f, 0.5f).amp (0.03f, 0.0f, 1.0f, 0.6f).gain (0.28f));
    add (P ("Pad 4 (choir)").L (osc (W::Saw, 1.0f, 0.0f, -10.0f), osc (W::Saw, 1.0f, 0.0f, 10.0f))
            .formant (1).vib (6.0f, 4.5f, 0.5f).amp (0.6f, 0.0f, 1.0f, 1.2f).gain (0.6f));
    add (P ("Pad 5 (bowed)").L (osc (W::Saw), osc (W::Triangle, 0.8f, 0.0f, 6.0f))
            .lp (700, 1.6f, 0.4f).fenv (1.5f, 1.0f, 1.0f, 0.6f, 1.0f).amp (0.6f, 0.0f, 1.0f, 1.0f).gain (0.35f));
    add (P ("Pad 6 (metallic)").L (fm (3.5f, 1.5f, 1.5f, 0.5f), osc (W::Saw, 0.5f, 0.0f, 7.0f))
            .lp (3000, 1.0f, 0.3f).amp (0.3f, 0.0f, 1.0f, 1.0f).gain (0.35f));
    add (P ("Pad 7 (halo)").L (additive ({ 1.0f, 0.5f, 0.3f, 0.2f, 0.1f }), osc (W::Saw, 0.6f, 0.0f, 9.0f))
            .formant (2).amp (0.8f, 0.0f, 1.0f, 1.5f).gain (0.55f));
    add (P ("Pad 8 (sweep)").L (osc (W::Saw, 1.0f, 0.0f, -9.0f), osc (W::Saw, 1.0f, 0.0f, 9.0f))
            .lp (500, 4.0f, 0.3f).lfoFilter (2.2f, 0.2f).amp (0.4f, 0.0f, 1.0f, 1.0f).gain (0.28f));

    // --- Synth Effects (96-103)
    add (P ("FX 1 (rain)").L (faded (fm (2.7f, 3.0f, 0.5f, 0.1f), 1.0f), noise (0.12f))
            .lp (6000).amp (0.001f, 4.0f, 0.3f, 1.0f).gain (0.4f));
    add (P ("FX 2 (soundtrack)").L (osc (W::Saw, 1.0f, 0.0f, -10.0f), osc (W::Saw, 1.0f, 0.0f, 10.0f))
            .lp (400, 1.2f, 0.3f).fenv (3.0f, 2.0f, 2.0f, 0.6f, 2.0f).amp (1.0f, 0.0f, 1.0f, 2.0f).gain (0.28f));
    add (P ("FX 3 (crystal)").L (fm (3.5f, 2.5f, 1.0f, 0.2f), faded (fm (7.0f, 2.0f, 0.3f, 0.1f, 0.4f), 0.5f))
            .amp (0.001f, 3.0f, 0.2f, 1.5f).gain (0.4f));
    add (P ("FX 4 (atmosphere)").L (pluck (3.0f, 0.6f, 0.8f), osc (W::Saw, 0.5f, 0.0f, 7.0f))
            .lp (2000, 1.0f, 0.3f).amp (0.01f, 0.0f, 1.0f, 1.2f).gain (0.4f));
    add (P ("FX 5 (brightness)").L (osc (W::Saw, 1.0f, 0.0f, -15.0f), osc (W::Saw, 1.0f, 12.0f, 15.0f))
            .lp (6000, 1.0f).amp (0.3f, 0.0f, 1.0f, 1.5f).gain (0.22f));
    add (P ("FX 6 (goblins)").L (osc (W::Saw), pulse (0.2f, 0.7f, -12.0f))
            .lp (500, 6.0f, 0.3f).lfoFilter (2.5f, 0.4f).pitchEnv (-2.0f, 1.5f).amp (1.0f, 0.0f, 1.0f, 1.5f).gain (0.28f));
    add (P ("FX 7 (echoes)").L (fm (2.0f, 1.5f, 0.5f, 0.3f), osc (W::Saw, 0.4f, 0.0f, 6.0f))
            .lp (2500, 1.0f, 0.3f).trem (0.45f, 3.0f).amp (0.01f, 0.0f, 1.0f, 2.0f).gain (0.35f));
    add (P ("FX 8 (sci-fi)").L (osc (W::Saw), osc (W::Square, 0.5f, 12.0f))
            .lp (1000, 5.0f, 0.3f).lfoFilter (2.0f, 6.0f).vib (40.0f, 6.0f, 0.0f).amp (0.05f, 0.0f, 1.0f, 1.0f).gain (0.25f));

    // --- Ethnic (104-111)
    add (P ("Sitar").L (pluck (3.0f, 0.95f), faded (osc (W::Saw, 0.25f), 1.5f))
            .lp (4500, 3.0f, 0.3f).pitchEnv (0.3f, 0.15f).amp (0.001f, 0.0f, 1.0f, 0.3f).keyScale (0.2f).gain (0.45f));
    add (P ("Banjo").L (pluck (1.2f, 0.85f)).hp (150).amp (0.001f, 0.0f, 1.0f, 0.15f).keyScale (0.3f).gain (0.6f));
    add (P ("Shamisen").L (pluck (0.9f, 0.9f)).lp (4000, 1.5f).amp (0.001f, 0.0f, 1.0f, 0.12f).keyScale (0.3f).gain (0.6f));
    add (P ("Koto").L (pluck (2.5f, 0.7f)).lp (3500).pitchEnv (0.2f, 0.1f).amp (0.001f, 0.0f, 1.0f, 0.3f).keyScale (0.3f).gain (0.6f));
    add (P ("Kalimba").L (osc (W::Sine), faded (fm (6.0f, 1.5f, 0.03f, 0.0f, 0.5f), 0.05f))
            .amp (0.001f, 1.2f, 0.0f, 0.3f).keyScale (0.4f).gain (0.6f));
    add (P ("Bag pipe").L (pulse (0.15f), osc (W::Saw, 0.5f, -12.0f))
            .lp (3000, 2.0f, 0.3f).vel (0.0f).amp (0.05f, 0.0f, 1.0f, 0.1f).gain (0.3f));
    add (P ("Fiddle").L (osc (W::Saw, 1.0f, 0.0f, -4.0f), osc (W::Saw, 0.7f, 0.0f, 4.0f))
            .lp (3500, 2.0f, 0.5f).vib (20.0f, 6.0f, 0.15f).vel (0.5f, 0.8f).amp (0.04f, 0.0f, 1.0f, 0.15f).gain (0.3f));
    add (P ("Shanai").L (pulse (0.15f)).lp (3000, 3.0f, 0.4f).vib (25.0f, 6.0f, 0.2f).vel (0.5f)
            .amp (0.03f, 0.0f, 1.0f, 0.08f).gain (0.35f));

    // --- Percussive (112-119)
    add (P ("Tinkle Bell").L (shifted (fm (3.5f, 2.0f, 0.4f, 0.1f), 12), faded (fm (7.1f, 1.5f, 0.0f, 1.0f, 0.4f), 0.4f))
            .amp (0.001f, 1.5f, 0.0f, 0.5f).keyScale (0.3f).gain (0.4f));
    add (P ("Agogo").L (fm (2.76f, 1.5f, 0.1f, 0.2f)).amp (0.001f, 0.6f, 0.0f, 0.3f).keyScale (0.3f).gain (0.5f));
    add (P ("Steel Drums").L (fm (1.0f, 1.5f, 0.1f, 0.3f), faded (osc (W::Sine, 0.35f, 12.0f), 0.4f))
            .pitchEnv (0.2f, 0.03f).amp (0.001f, 1.3f, 0.0f, 0.4f).keyScale (0.3f).gain (0.5f));
    add (P ("Woodblock").L (fm (1.5f, 1.2f, 0.02f, 0.0f), osc (W::Sine, 0.5f, 12.0f))
            .amp (0.001f, 0.12f, 0.0f, 0.1f).gain (0.6f));
    add (P ("Taiko Drum").L (osc (W::Sine), noise (0.4f, 0.1f))
            .lp (900, 0.707f, 0.3f).pitchEnv (5.0f, 0.08f).amp (0.001f, 0.9f, 0.0f, 0.6f).gain (0.8f));
    add (P ("Melodic Tom").L (osc (W::Sine), noise (0.2f, 0.05f))
            .lp (2000, 0.707f, 0.3f).pitchEnv (7.0f, 0.2f).amp (0.001f, 0.7f, 0.0f, 0.5f).gain (0.7f));
    add (P ("Synth Drum").L (osc (W::Sine), osc (W::Triangle, 0.3f))
            .pitchEnv (12.0f, 0.15f).amp (0.001f, 0.5f, 0.0f, 0.4f).gain (0.7f));
    add (P ("Reverse Cymbal").L (noise (1.0f))
            .hp (3000, 0.9f).vel (0.3f).amp (1.8f, 0.0f, 1.0f, 0.08f).gain (0.35f));

    // --- Sound Effects (120-127)
    add (P ("Guitar Fret Noise").L (noise (1.0f), pluck (0.15f, 0.9f, 0.5f, 12.0f))
            .bp (2000, 3.0f, 0.5f).amp (0.001f, 0.15f, 0.0f, 0.05f).gain (0.6f));
    add (P ("Breath Noise").L (noise (1.0f)).bp (1200, 1.5f, 0.5f).trem (0.15f, 3.0f)
            .amp (0.1f, 0.0f, 1.0f, 0.3f).gain (0.6f));
    add (P ("Seashore").L (noise (1.0f)).lp (800, 0.707f).lfoFilter (2.5f, 0.12f).trem (0.7f, 0.12f)
            .vel (0.3f).amp (1.5f, 0.0f, 1.0f, 2.5f).gain (0.45f));
    add (P ("Bird Tweet").L (osc (W::Sine, 1.0f, 24.0f)).vib (400.0f, 12.0f, 0.0f).trem (0.8f, 12.0f)
            .pitchEnv (12.0f, 0.1f).amp (0.01f, 0.0f, 1.0f, 0.1f).gain (0.35f));
    add (P ("Telephone Ring").L (osc (W::Square, 1.0f, 12.0f), osc (W::Square, 0.8f, 16.0f))
            .lp (3000).trem (1.0f, 20.0f).vel (0.2f).amp (0.005f, 0.0f, 1.0f, 0.05f).gain (0.2f));
    add (P ("Helicopter").L (noise (1.0f), osc (W::Saw, 0.5f, -24.0f))
            .lp (400, 3.0f).trem (0.9f, 12.0f).amp (0.5f, 0.0f, 1.0f, 1.0f).gain (0.6f));
    add (P ("Applause").L (noise (1.0f)).bp (2500, 0.8f).trem (0.5f, 18.0f).vel (0.3f)
            .amp (0.5f, 0.0f, 1.0f, 1.5f).gain (0.5f));
    add (P ("Gunshot").L (noise (1.0f), faded (osc (W::Sine, 1.0f, -24.0f), 0.2f))
            .lp (3000).fenv (2.0f, 0.001f, 0.15f, 0.0f, 0.1f).pitchEnv (12.0f, 0.05f)
            .amp (0.001f, 0.45f, 0.0f, 0.3f).gain (0.8f));

    return t;
}

const std::array<Patch, 128>& patches()
{
    static const std::array<Patch, 128> table = buildPatches();
    return table;
}

const char* const familyNames[16] = {
    "Piano", "Chromatic Percussion", "Organ", "Guitar", "Bass", "Strings", "Ensemble", "Brass",
    "Reed", "Pipe", "Synth Lead", "Synth Pad", "Synth Effects", "Ethnic", "Percussive", "Sound Effects"
};

} // namespace

const Patch& getMelodicPatch (int program) { return patches()[(size_t) (program & 127)]; }
const char* getProgramName (int program)   { return patches()[(size_t) (program & 127)].name; }
const char* getFamilyName (int program)    { return familyNames[(program & 127) / 8]; }

} // namespace gm
