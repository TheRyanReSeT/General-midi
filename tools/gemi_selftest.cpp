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

    Rig() { p.setPlayConfigDetails (0, 2, rate, block); p.prepareToPlay (rate, block); }

    // Renders `seconds` of audio with `events` (sample offset from the start, message) and returns its peak level.
    float run (double seconds, std::vector<std::pair<int, juce::MidiMessage>> events = {})
    {
        const int total = (int) (seconds * rate);
        float peak = 0.0f;
        for (int start = 0; start < total; start += block)
        {
            juce::AudioBuffer<float> buffer (2, block);
            juce::MidiBuffer midi;
            for (auto& [when, msg] : events)
                if (when >= start && when < start + block)
                    midi.addEvent (msg, when - start);
            p.processBlock (buffer, midi);
            peak = std::max (peak, buffer.getMagnitude (0, block));
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
};
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juce;
    Rig rig;
    auto& p = rig.p;

    // ---- Synth mode: three layers from one key
    rig.run (0.2);
    float peak = rig.run (0.6, { { 0, juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100) } });
    check (peak > 0.01f, "synth mode: a note on channel 1 makes sound");
    check (p.getActiveVoices() >= 3, "synth mode: one key starts voices on all three layers");

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
    rig.run (3.5);
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
    const int before = p.getPresetIndex();
    p.stepPreset (1);
    check (p.getPresetIndex() == (before + 1) % p.presets.all().size(), "combos: next steps through the list");
    check (p.getUiState()["layerProg"][0] == juce::var (5), "combos: loading Glass Garden sets layer 1 to Electric Piano 1");
    p.stepPreset (-1);
    check (p.getPresetName() == "Aurora Choir", "combos: previous comes back to Aurora Choir");

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

    // ---- Saved state survives a reload
    rig.set ("l2_cutoff", 0.25f);
    rig.state (R"({"theme":"light","size":1.25,"kit":26})");
    juce::MemoryBlock saved;
    p.getStateInformation (saved);
    GemiProcessor restored;
    restored.setStateInformation (saved.getData(), (int) saved.getSize());
    const auto ui = restored.getUiState();
    check (std::abs (restored.parameters.getRawParameterValue ("l2_cutoff")->load() - 0.25f) < 1.0e-4f, "state: parameters are restored");
    check (ui["view"] == juce::var ("gm") && ui["theme"] == juce::var ("light") && (double) ui["size"] == 1.25 && (int) ui["kit"] == 26,
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
