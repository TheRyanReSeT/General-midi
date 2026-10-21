// Drives the real gemi processor offline and checks the behaviour the UI depends on.
// Exits non-zero on the first failed check. Optionally writes what it played to a WAV file.
#include "PluginProcessor.h"

#include <juce_audio_formats/juce_audio_formats.h>

#include <cstdio>

namespace
{
constexpr double rate = 44100.0;
constexpr int block = 512;
int failures = 0;

void check (bool ok, const char* what)
{
    std::printf ("%s  %s\n", ok ? "ok  " : "FAIL", what);
    if (! ok) ++failures;
}

struct Rig
{
    GemiProcessor p;
    juce::AudioBuffer<float> recording { 2, 0 };
    std::vector<float> last;   // the left channel of the latest run()

    Rig() { p.setPlayConfigDetails (0, 2, rate, block); p.prepareToPlay (rate, block); }

    // Renders `seconds` of audio with `events` (sample offset from the start, message) and returns its peak level.
    float run (double seconds, std::vector<std::pair<int, juce::MidiMessage>> events = {})
    {
        const int total = (int) (seconds * rate);
        float peak = 0.0f;
        last.clear();
        for (int start = 0; start < total; start += block)
        {
            juce::AudioBuffer<float> buffer (2, block);
            juce::MidiBuffer midi;
            for (auto& [when, msg] : events)
                if (when >= start && when < start + block)
                    midi.addEvent (msg, when - start);
            p.processBlock (buffer, midi);
            peak = std::max (peak, buffer.getMagnitude (0, block));
            last.insert (last.end(), buffer.getReadPointer (0), buffer.getReadPointer (0) + block);
            const int at = recording.getNumSamples();
            recording.setSize (2, at + block, true);
            for (int c = 0; c < 2; ++c) recording.copyFrom (c, at, buffer, c, 0, block);
        }
        return peak;
    }

    void set (const char* id, float value)
    {
        auto* param = p.parameters.getParameter (id);
        param->setValueNotifyingHost (param->convertTo0to1 (value));
    }

    void state (const char* json) { p.setUiState (juce::JSON::parse (json)); }

    // Pitch of the latest run: the lag (40 Hz .. 2 kHz) where the signal best matches itself.
    float pitch() const
    {
        const int n = std::min ((int) last.size() - 1200, 8192);
        if (n <= 0) return 0.0f;
        int best = 0;
        double bestScore = 0.0;
        for (int lag = 22; lag < 1100; ++lag)
        {
            double xy = 0.0, xx = 0.0, yy = 0.0;
            for (int i = 0; i < n; ++i)
            {
                const double x = last[(size_t) i], y = last[(size_t) (i + lag)];
                xy += x * y; xx += x * x; yy += y * y;
            }
            const double score = xy / std::sqrt (xx * yy + 1.0e-12);
            if (score > bestScore * 1.02) { bestScore = score; best = lag; }   // prefer the shortest period
        }
        return best > 0 ? (float) (rate / best) : 0.0f;
    }
};
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juce;
    Rig rig;
    auto& p = rig.p;

    // ---- Synth mode: starts with one layer, more can be added
    rig.run (0.2);
    float peak = rig.run (0.3, { { 0, juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100) } });
    check (peak > 0.01f, "synth mode: a note on channel 1 makes sound");
    const int oneLayer = p.getActiveVoices();
    rig.run (0.1, { { 0, juce::MidiMessage::noteOff (1, 60) } });
    rig.run (1.5);
    rig.set ("layers", 3.0f);
    rig.run (0.6, { { 0, juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100) } });
    std::printf ("      voices: one layer %d, three layers %d\n", oneLayer, p.getActiveVoices());
    check (oneLayer >= 1 && p.getActiveVoices() > oneLayer, "synth mode: adding layers adds their voices to every key");

    juce::Array<int> held;
    p.getHeldNotes (held);
    check (held.contains (60), "synth mode: the held key is reported for the on-screen keyboard");

    rig.run (0.1, { { 0, juce::MidiMessage::noteOff (1, 60) } });
    p.getHeldNotes (held);
    check (held.isEmpty(), "synth mode: releasing the key clears it");

    // Any input channel plays the layers.
    peak = rig.run (0.4, { { 0, juce::MidiMessage::noteOn (7, 64, (juce::uint8) 100) } });
    check (peak > 0.01f, "synth mode: input on channel 7 also plays the layers");
    rig.run (0.1, { { 0, juce::MidiMessage::noteOff (7, 64) } });
    rig.run (4.0);
    check (p.getActiveVoices() == 0, "synth mode: everything is silent after the release times");

    // Release time comes from the layer envelope: long release keeps sounding, short release stops.
    // Reverb and chorus are off for these checks so only the notes themselves are measured.
    rig.set ("reverb", 0.0f);
    rig.set ("chorus", 0.0f);
    for (int l = 1; l <= 3; ++l)
    {
        rig.set (("l" + juce::String (l) + "_sustain").toRawUTF8(), 1.0f);
        rig.set (("l" + juce::String (l) + "_release").toRawUTF8(), 1.0f);   // 3 s
    }
    rig.run (0.5, { { 0, juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100) } });
    rig.run (0.05, { { 0, juce::MidiMessage::noteOff (1, 60) } });
    const float longTail = rig.run (1.0);
    for (int l = 1; l <= 3; ++l)
        rig.set (("l" + juce::String (l) + "_release").toRawUTF8(), 0.0f);   // instant
    rig.run (4.0);
    rig.run (0.5, { { 0, juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100) } });
    rig.run (0.05, { { 0, juce::MidiMessage::noteOff (1, 60) } });
    rig.run (0.1);
    const float shortTail = rig.run (1.0);
    std::printf ("      tails: long %.4f short %.4f\n", longTail, shortTail);
    check (longTail > 0.005f && shortTail < longTail * 0.2f, "synth mode: the layer release time shapes the tail");

    // Switching a layer off silences it; with all layers off, keys are silent.
    for (int l = 1; l <= 3; ++l) rig.set (("l" + juce::String (l) + "_on").toRawUTF8(), 0.0f);
    rig.run (0.1);
    peak = rig.run (0.4, { { 0, juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100) } });
    std::printf ("      layers off peak %.5f\n", peak);
    check (peak < 1.0e-4f, "synth mode: layers switched off play nothing");
    rig.run (0.1, { { 0, juce::MidiMessage::noteOff (1, 60) } });
    for (int l = 1; l <= 3; ++l) rig.set (("l" + juce::String (l) + "_on").toRawUTF8(), 1.0f);

    // ---- MPE: each note's channel bends on its own
    rig.set ("layers", 1.0f);
    rig.set ("vibrato", 0.0f);
    rig.set ("l1_semi", 0.0f);
    rig.set ("l1_oct", 0.0f);
    rig.set ("l1_fine", 0.0f);
    rig.state (R"({"layerProg":[81,49,90]})");   // Lead 1 (square): an easy pitch to measure
    rig.set ("mpe", 1.0f);
    rig.run (0.2);
    rig.run (0.2, { { 0, juce::MidiMessage::noteOn (2, 57, (juce::uint8) 100) } });
    const float plain = (rig.run (0.5), rig.pitch());
    rig.run (0.1, { { 0, juce::MidiMessage::pitchWheel (2, 8192 + 2048) } });   // +12 semitones at the MPE range of 48
    const float bent = (rig.run (0.5), rig.pitch());
    rig.run (0.3, { { 0, juce::MidiMessage::noteOff (2, 57) } });
    rig.run (0.2, { { 0, juce::MidiMessage::noteOn (3, 57, (juce::uint8) 100) } });
    const float other = (rig.run (0.5), rig.pitch());
    rig.run (0.3, { { 0, juce::MidiMessage::noteOff (3, 57) }, { 10, juce::MidiMessage::pitchWheel (2, 8192) } });
    std::printf ("      MPE pitch: plain %.0f Hz, bent %.0f Hz, other channel %.0f Hz\n", plain, bent, other);
    check (bent / plain > 1.85f && bent / plain < 2.15f, "MPE: a member channel's bend moves its note");
    check (other / plain > 0.93f && other / plain < 1.07f, "MPE: other channels' notes are not bent");
    rig.set ("mpe", 0.0f);
    rig.set ("layers", 3.0f);
    rig.set ("reverb", 0.7f);
    rig.set ("chorus", 0.35f);

    // Mono mode: one voice set at a time.
    rig.set ("mono", 1.0f);
    rig.run (0.1);
    rig.run (0.3, { { 0, juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100) }, { 2000, juce::MidiMessage::noteOn (1, 67, (juce::uint8) 100) } });
    check (p.getActiveVoices() <= 6, "synth mode: Mono keeps one note per layer");
    rig.run (0.1, { { 0, juce::MidiMessage::noteOff (1, 60) }, { 10, juce::MidiMessage::noteOff (1, 67) } });
    rig.set ("mono", 0.0f);

    // On-screen keyboard.
    p.queueUiNote (72, 100, true);
    peak = rig.run (0.3);
    check (peak > 0.01f, "synth mode: the on-screen keyboard plays");
    p.queueUiNote (72, 0, false);
    rig.run (0.2);

    // ---- Combos
    p.loadPreset (1);
    check (p.getPresetName() == "Aurora Choir", "combos: Aurora Choir is second in the list");
    p.stepPreset (1);
    check (p.getPresetIndex() == 2, "combos: next steps through the list");
    check (p.getUiState()["layerProg"][0] == juce::var (5), "combos: loading Glass Garden sets layer 1 to Electric Piano 1");
    p.stepPreset (-1);
    check (p.getPresetName() == "Aurora Choir", "combos: previous comes back to Aurora Choir");
    p.loadPreset (0);
    check (p.getPresetName() == "Grand Piano" && p.parameters.getRawParameterValue ("layers")->load() < 1.5f, "combos: Grand Piano uses one layer");

    // ---- GM Player
    rig.state (R"({"view":"gm"})");
    rig.run (0.1);
    peak = rig.run (0.4, { { 0, juce::MidiMessage::noteOn (2, 40, (juce::uint8) 100) } });
    check (peak > 0.01f, "GM Player: channel 2 plays its sound");
    rig.run (0.1, { { 0, juce::MidiMessage::noteOff (2, 40) } });

    rig.run (0.1, { { 0, juce::MidiMessage::noteOn (5, 60, (juce::uint8) 100) } });
    check ((p.getActiveChannels() & (1u << 4)) != 0, "GM Player: a held note lights its channel");
    rig.run (0.1, { { 0, juce::MidiMessage::noteOff (5, 60) } });
    check ((p.getActiveChannels() & (1u << 4)) == 0, "GM Player: the light goes out on release");

    rig.run (0.05, { { 0, juce::MidiMessage::programChange (3, 40) } });
    check (p.takeProgramsChanged() && p.getUiState()["gmProg"][2] == juce::var (41), "GM Player: a program change in the song shows in the UI state");

    rig.set ("reverb", 0.0f);
    rig.set ("chorus", 0.0f);
    rig.state (R"({"mute":2})");   // mute channel 2
    rig.run (1.0);
    peak = rig.run (0.3, { { 0, juce::MidiMessage::noteOn (2, 40, (juce::uint8) 100) } });
    std::printf ("      muted peak %.5f\n", peak);
    check (peak < 1.0e-4f, "GM Player: a muted channel is silent");
    rig.state (R"({"mute":0})");
    rig.set ("reverb", 0.7f);
    rig.set ("chorus", 0.35f);
    rig.run (0.1, { { 0, juce::MidiMessage::noteOff (2, 40) } });

    // Follow keyboard: channel 1 input plays the selected channel.
    rig.state (R"({"ch":10,"follow":true})");
    rig.run (0.05, { { 0, juce::MidiMessage::noteOn (1, 38, (juce::uint8) 110) } });
    check ((p.getActiveChannels() & (1u << 9)) != 0, "GM Player: Follow keyboard sends channel 1 to the selected channel");
    rig.run (0.3, { { 0, juce::MidiMessage::noteOff (1, 38) } });
    rig.state (R"({"follow":false})");
    rig.run (0.05, { { 0, juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100) } });
    check ((p.getActiveChannels() & 1u) != 0, "GM Player: with Follow off, channel 1 plays channel 1");
    rig.run (0.3, { { 0, juce::MidiMessage::noteOff (1, 60) } });

    // ---- GM Player: a channel plays on its own output when the host turns that output on
    {
        GemiProcessor multi;
        multi.enableAllBuses();
        multi.prepareToPlay (rate, block);
        multi.setUiState (juce::JSON::parse (R"({"view":"gm"})"));
        juce::AudioBuffer<float> buffer (multi.getTotalNumOutputChannels(), block);
        float mainPeak = 0.0f, ownPeak = 0.0f;
        const int own = multi.getChannelIndexInProcessBlockBuffer (false, 2, 0);   // "Ch 2"
        for (int b = 0; b < 40; ++b)
        {
            juce::MidiBuffer midi;
            if (b == 2) midi.addEvent (juce::MidiMessage::noteOn (2, 48, (juce::uint8) 110), 0);
            multi.processBlock (buffer, midi);
            mainPeak = std::max (mainPeak, buffer.getMagnitude (0, 0, block));
            ownPeak = std::max (ownPeak, buffer.getMagnitude (own, 0, block));
        }
        std::printf ("      outputs: %d channels, main %.4f, Ch 2 %.4f\n", multi.getTotalNumOutputChannels(), mainPeak, ownPeak);
        check (multi.getTotalNumOutputChannels() == 34 && ownPeak > 0.01f && mainPeak < 1.0e-4f, "GM Player: channel 2 plays on its own output");
    }

    // ---- Saved state survives a reload
    rig.set ("l2_cutoff", 0.25f);
    rig.state (R"({"theme":"light","size":1.25,"kit":26})");
    juce::MemoryBlock saved;
    p.getStateInformation (saved);
    GemiProcessor restored;
    restored.setStateInformation (saved.getData(), (int) saved.getSize());
    const auto ui = restored.getUiState();
    check (std::abs (restored.parameters.getRawParameterValue ("l2_cutoff")->load() - 0.25f) < 1.0e-4f, "state: parameters are restored");
    check (ui["view"] == juce::var ("gm") && ui["theme"] == juce::var ("light") && std::abs ((double) ui["size"] - 1.25) < 1.0e-9 && (int) ui["kit"] == 26,
           "state: mode, theme, window size and drum kit are restored");

    if (argc > 1)
    {
        juce::File out (juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]));
        out.deleteFile();
        std::unique_ptr<juce::OutputStream> stream = out.createOutputStream();
        juce::WavAudioFormat wav;
        if (stream != nullptr)
            if (auto writer = wav.createWriterFor (stream, juce::AudioFormatWriterOptions{}.withSampleRate (rate).withNumChannels (2).withBitsPerSample (16)))
                writer->writeFromAudioSampleBuffer (rig.recording, 0, rig.recording.getNumSamples());
    }

    std::printf ("\n%s (%d failed)\n", failures == 0 ? "All checks passed" : "Some checks failed", failures);
    return failures == 0 ? 0 : 1;
}
