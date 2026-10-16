// Renders a tour of every General MIDI instrument and drum kit to a WAV file and
// prints the level of each one, as a quick check that the engine works.
//
//   gm_render [output.wav] [first-program] [last-program]

#include "GMSynth.h"
#include "GMNames.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <vector>

namespace
{

constexpr int sampleRate = 44100;

struct Renderer
{
    gm::GMSynth& synth;
    std::vector<float> left, right;

    void midi (std::initializer_list<int> bytes)
    {
        uint8_t data[3];
        int n = 0;
        for (int b : bytes) data[n++] = (uint8_t) b;
        synth.handleMidi (data, n);
    }

    // Renders `seconds` of audio, returns the peak and RMS of that span.
    void run (double seconds, float& peak, float& rms)
    {
        const int n = (int) (seconds * sampleRate);
        const size_t start = left.size();
        left.resize (start + (size_t) n);
        right.resize (start + (size_t) n);

        for (int pos = 0; pos < n; pos += 512)
        {
            const int len = std::min (512, n - pos);
            synth.render (left.data() + start + pos, right.data() + start + pos, len);
        }

        double sum = 0.0;
        peak = 0.0f;
        for (size_t i = start; i < left.size(); ++i)
        {
            peak = std::max ({ peak, std::abs (left[i]), std::abs (right[i]) });
            sum += left[i] * left[i] + right[i] * right[i];
            if (! std::isfinite (left[i]) || ! std::isfinite (right[i])) peak = INFINITY;
        }
        rms = (float) std::sqrt (sum / (2.0 * n));
    }

    void run (double seconds) { float p, r; run (seconds, p, r); }
};

void writeWav (const char* path, const std::vector<float>& l, const std::vector<float>& r)
{
    std::ofstream out (path, std::ios::binary);
    auto u32 = [&] (uint32_t v) { out.write ((const char*) &v, 4); };
    auto u16 = [&] (uint16_t v) { out.write ((const char*) &v, 2); };
    const uint32_t dataBytes = (uint32_t) (l.size() * 4);

    out.write ("RIFF", 4); u32 (36 + dataBytes); out.write ("WAVE", 4);
    out.write ("fmt ", 4); u32 (16); u16 (1); u16 (2); u32 (sampleRate); u32 (sampleRate * 4); u16 (4); u16 (16);
    out.write ("data", 4); u32 (dataBytes);

    for (size_t i = 0; i < l.size(); ++i)
    {
        const int16_t s[2] = { (int16_t) std::lround (std::clamp (l[i], -1.0f, 1.0f) * 32767.0f),
                               (int16_t) std::lround (std::clamp (r[i], -1.0f, 1.0f) * 32767.0f) };
        out.write ((const char*) s, 4);
    }
}

} // namespace

int main (int argc, char** argv)
{
    const char* outPath = argc > 1 ? argv[1] : "gm_tour.wav";
    const int first = argc > 2 ? std::atoi (argv[2]) : 0;
    const int last  = argc > 3 ? std::atoi (argv[3]) : 127;

    std::ifstream file (GM_SOUNDFONT_PATH, std::ios::binary);
    std::vector<char> sf2 ((std::istreambuf_iterator<char> (file)), std::istreambuf_iterator<char>());
    if (sf2.empty())
    {
        std::fprintf (stderr, "Could not read %s\n", GM_SOUNDFONT_PATH);
        return 1;
    }

    gm::GMSynth synth (sf2.data(), sf2.size());
    if (! synth.prepare (sampleRate))
    {
        std::fprintf (stderr, "FluidSynth failed to load the SoundFont\n");
        return 1;
    }

    std::printf ("Loaded %zu presets\n", synth.getPresets().size());

    Renderer r { synth, {}, {} };
    int failures = 0;

    for (int prog = first; prog <= last; ++prog)
    {
        r.midi ({ 0xC0, prog });
        // Up-and-down arpeggio, then a chord.
        const int notes[] = { 60, 64, 67, 72, 67, 64 };
        float peak = 0, rms = 0, p, q;
        for (int n : notes)
        {
            r.midi ({ 0x90, n, 100 });
            r.run (0.15, p, q); peak = std::max (peak, p); rms = std::max (rms, q);
            r.midi ({ 0x80, n, 0 });
        }
        for (int n : { 48, 60, 64, 67 }) r.midi ({ 0x90, n, 90 });
        r.run (0.7, p, q); peak = std::max (peak, p); rms = std::max (rms, q);
        for (int n : { 48, 60, 64, 67 }) r.midi ({ 0x80, n, 0 });
        r.run (0.3);

        const bool ok = std::isfinite (peak) && rms > 1.0e-4f && peak < 1.0f;
        failures += ok ? 0 : 1;
        std::printf ("%3d %-26s %-28s peak %5.2f  rms %6.4f %s\n", prog + 1, gm::programName (prog),
                     synth.getPresetName (0, prog).c_str(), peak, rms, ok ? "" : "  <-- CHECK");
    }

    // A drum groove on every kit in the SoundFont.
    for (const auto& kit : synth.getPresets())
    {
        if (kit.bank != gm::GMSynth::drumBank) continue;
        r.midi ({ 0xC9, kit.program });

        float peak = 0, rms = 0, p, q;
        for (int step = 0; step < 16; ++step)
        {
            if (step % 4 == 0)              r.midi ({ 0x99, 36, 110 });
            if (step % 8 == 4)              r.midi ({ 0x99, 38, 105 });
            r.midi ({ 0x99, step == 14 ? 46 : 42, 80 });
            if (step == 0)                  r.midi ({ 0x99, 49, 100 });
            if (step >= 12)                 r.midi ({ 0x99, 50 - (step - 12) * 2, 100 });
            r.run (0.125, p, q); peak = std::max (peak, p); rms = std::max (rms, q);
        }
        r.run (0.5);

        const bool ok = std::isfinite (peak) && rms > 1.0e-4f && peak < 1.0f;
        failures += ok ? 0 : 1;
        std::printf ("Drum kit %3d %-28s peak %5.2f  rms %6.4f %s\n", kit.program, kit.name.c_str(), peak, rms,
                     ok ? "" : "  <-- CHECK");
    }

    writeWav (outPath, r.left, r.right);
    std::printf ("Wrote %s (%.1f s), %d problem(s)\n", outPath, r.left.size() / (double) sampleRate, failures);
    return failures == 0 ? 0 : 2;
}
