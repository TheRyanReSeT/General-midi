// Small, dependency-free DSP building blocks used by the GM engine.
#pragma once

#include <cmath>
#include <cstdint>
#include <algorithm>
#include <array>

namespace gm
{

constexpr float kPi    = 3.14159265358979323846f;
constexpr float kTwoPi = 6.28318530717958647692f;

inline float midiToHz (float note) { return 440.0f * std::exp2 ((note - 69.0f) / 12.0f); }
inline float clampf (float x, float lo, float hi) { return std::min (hi, std::max (lo, x)); }

//==============================================================================
// Shared sine lookup table (linear interpolation). Phase is in cycles [0, 1).
struct SineTable
{
    static constexpr int size = 4096;
    float data[size + 1];

    SineTable()
    {
        for (int i = 0; i <= size; ++i)
            data[i] = std::sin (kTwoPi * (float) i / (float) size);
    }

    static const SineTable& get()
    {
        static const SineTable table;
        return table;
    }
};

inline float fastSin (float phase)
{
    phase -= std::floor (phase);
    const float pos = phase * (float) SineTable::size;
    const int idx = (int) pos;
    const float frac = pos - (float) idx;
    const float* d = SineTable::get().data;
    return d[idx] + frac * (d[idx + 1] - d[idx]);
}

//==============================================================================
struct Noise
{
    uint32_t state = 0x12345678u;

    void seed (uint32_t s) { state = s ? s : 0x9e3779b9u; }

    float next()
    {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return (float) (int32_t) state * (1.0f / 2147483648.0f);
    }
};

//==============================================================================
enum class Wave : uint8_t { Sine, Saw, Square, Pulse, Triangle };

inline float polyBlep (float t, float dt)
{
    if (t < dt)        { t /= dt;          return t + t - t * t - 1.0f; }
    if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
    return 0.0f;
}

// Band-limited (PolyBLEP) oscillator.
struct Oscillator
{
    float phase = 0.0f;

    float process (Wave wave, float inc, float pulseWidth = 0.5f)
    {
        float out = 0.0f;
        const float dt = std::min (inc, 0.5f);

        switch (wave)
        {
            case Wave::Sine:
                out = fastSin (phase);
                break;
            case Wave::Saw:
                out = 2.0f * phase - 1.0f - polyBlep (phase, dt);
                break;
            case Wave::Square:
            case Wave::Pulse:
            {
                const float pw = wave == Wave::Square ? 0.5f : pulseWidth;
                out = phase < pw ? 1.0f : -1.0f;
                out += polyBlep (phase, dt);
                float t2 = phase - pw + 1.0f;
                t2 -= std::floor (t2);
                out -= polyBlep (t2, dt);
                break;
            }
            case Wave::Triangle:
                out = 4.0f * std::abs (phase - 0.5f) - 1.0f;
                break;
        }

        phase += inc;
        if (phase >= 1.0f) phase -= std::floor (phase);
        return out;
    }
};

//==============================================================================
// Topology-preserving-transform state variable filter (Zavalishin).
struct SVF
{
    float g = 0.0f, k = 1.0f, a1 = 0.0f, a2 = 0.0f, a3 = 0.0f;
    float ic1 = 0.0f, ic2 = 0.0f;
    float lp = 0.0f, bp = 0.0f, hp = 0.0f;

    void reset() { ic1 = ic2 = lp = bp = hp = 0.0f; }

    void set (float cutoffHz, float q, float sampleRate)
    {
        cutoffHz = clampf (cutoffHz, 10.0f, sampleRate * 0.45f);
        g = std::tan (kPi * cutoffHz / sampleRate);
        k = 1.0f / std::max (q, 0.1f);
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }

    void process (float x)
    {
        const float v3 = x - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        lp = v2;
        bp = v1;
        hp = x - k * v1 - v2;
    }
};

//==============================================================================
// Linear-attack, exponential decay/release ADSR. Times in seconds.
struct ADSR
{
    enum class Stage : uint8_t { Idle, Attack, Decay, Sustain, Release };

    Stage stage = Stage::Idle;
    float level = 0.0f;
    float attackInc = 0.0f, decayCoef = 0.0f, releaseCoef = 0.0f, sustain = 1.0f;

    static float coefFor (float seconds, float sampleRate)
    {
        // time to fall ~60 dB
        if (seconds <= 0.0005f) return 0.0f;
        return std::exp (-6.9f / (seconds * sampleRate));
    }

    void setup (float a, float d, float s, float r, float sampleRate)
    {
        attackInc = a <= 0.0005f ? 1.0f : 1.0f / (a * sampleRate);
        decayCoef = d <= 0.0f ? 1.0f : coefFor (d, sampleRate);  // d <= 0 means "hold"
        sustain = s;
        releaseCoef = coefFor (r, sampleRate);
    }

    void noteOn()  { stage = Stage::Attack; }
    void noteOff() { if (stage != Stage::Idle) stage = Stage::Release; }
    void kill()    { stage = Stage::Idle; level = 0.0f; }
    bool active() const { return stage != Stage::Idle; }

    float process()
    {
        switch (stage)
        {
            case Stage::Idle: return 0.0f;
            case Stage::Attack:
                level += attackInc;
                if (level >= 1.0f) { level = 1.0f; stage = Stage::Decay; }
                break;
            case Stage::Decay:
                level = sustain + (level - sustain) * decayCoef;
                if (std::abs (level - sustain) < 1.0e-4f)
                {
                    level = sustain;
                    stage = Stage::Sustain;
                }
                break;
            case Stage::Sustain:
                level = sustain;
                break;
            case Stage::Release:
                level *= releaseCoef;
                if (level < 1.0e-4f) kill();
                break;
        }
        return level;
    }
};

//==============================================================================
// Simple exponential decay (used for FM index, layer fades, pitch envelopes).
struct Decay
{
    float value = 0.0f, coef = 0.0f;

    void start (float startValue, float seconds, float sampleRate)
    {
        value = startValue;
        coef = seconds <= 0.0f ? 1.0f : std::exp (-6.9f / (seconds * sampleRate));
    }

    float process() { const float v = value; value *= coef; return v; }
};

//==============================================================================
// One-pole DC blocker.
struct DCBlocker
{
    float x1 = 0.0f, y1 = 0.0f;
    float process (float x)
    {
        const float y = x - x1 + 0.995f * y1;
        x1 = x;
        y1 = y;
        return y;
    }
    void reset() { x1 = y1 = 0.0f; }
};

} // namespace gm
