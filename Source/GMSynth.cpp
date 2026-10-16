#include "GMSynth.h"

#include <fluidsynth.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace gm
{
namespace
{

// FluidSynth's output level at unity gain. Its own default (0.2) is cautious; this
// matches the loudness of hardware GM modules without clipping on dense songs.
constexpr float baseGain = 0.4f;

//==============================================================================
// SoundFont loader callbacks that read from memory instead of a file.
// The "file name" encodes the buffer address and size: "mem:<address>:<size>".

struct MemoryFile
{
    const char* data;
    fluid_long_long_t size;
    fluid_long_long_t pos;
};

std::string makeMemoryFileName (const void* data, size_t size)
{
    char name[64];
    std::snprintf (name, sizeof (name), "mem:%p:%zu", data, size);
    return name;
}

void* memOpen (const char* filename)
{
    void* address = nullptr;
    size_t size = 0;

    if (filename == nullptr || std::sscanf (filename, "mem:%p:%zu", &address, &size) != 2 || address == nullptr)
        return nullptr;

    return new MemoryFile { static_cast<const char*> (address), (fluid_long_long_t) size, 0 };
}

int memRead (void* buf, fluid_long_long_t count, void* handle)
{
    auto* f = static_cast<MemoryFile*> (handle);
    if (count < 0 || f->pos + count > f->size)
        return FLUID_FAILED;

    std::memcpy (buf, f->data + f->pos, (size_t) count);
    f->pos += count;
    return FLUID_OK;
}

int memSeek (void* handle, fluid_long_long_t offset, int origin)
{
    auto* f = static_cast<MemoryFile*> (handle);
    fluid_long_long_t newPos;

    switch (origin)
    {
        case SEEK_SET: newPos = offset; break;
        case SEEK_CUR: newPos = f->pos + offset; break;
        case SEEK_END: newPos = f->size + offset; break;
        default: return FLUID_FAILED;
    }

    if (newPos < 0 || newPos > f->size)
        return FLUID_FAILED;

    f->pos = newPos;
    return FLUID_OK;
}

int memClose (void* handle)
{
    delete static_cast<MemoryFile*> (handle);
    return FLUID_OK;
}

fluid_long_long_t memTell (void* handle)
{
    return static_cast<MemoryFile*> (handle)->pos;
}

void silenceFluidLogging()
{
    fluid_set_log_function (FLUID_DBG, nullptr, nullptr);
    fluid_set_log_function (FLUID_INFO, nullptr, nullptr);
    fluid_set_log_function (FLUID_WARN, nullptr, nullptr);
}

} // namespace

//==============================================================================
GMSynth::GMSynth (const void* soundFontData, size_t soundFontSize)
    : sfData (soundFontData), sfSize (soundFontSize)
{
    silenceFluidLogging();
}

GMSynth::~GMSynth()
{
    destroy();
}

void GMSynth::destroy()
{
    if (synth != nullptr)   { delete_fluid_synth (synth); synth = nullptr; }
    if (settings != nullptr) { delete_fluid_settings (settings); settings = nullptr; }
    sfontId = -1;
    currentRate = 0.0;
}

bool GMSynth::prepare (double sampleRate)
{
    if (synth != nullptr && sampleRate == currentRate)
        return true;

    // Keep the channel programs across a sample-rate change.
    int savedBank[numChannels], savedProgram[numChannels];
    const bool hadSynth = synth != nullptr;
    for (int ch = 0; ch < numChannels; ++ch)
        if (! (hadSynth && getProgram (ch, savedBank[ch], savedProgram[ch])))
            savedBank[ch] = savedProgram[ch] = -1;

    destroy();

    settings = new_fluid_settings();
    fluid_settings_setnum (settings, "synth.sample-rate", sampleRate);
    fluid_settings_setint (settings, "synth.polyphony", 256);
    fluid_settings_setint (settings, "synth.midi-channels", numChannels);
    fluid_settings_setint (settings, "synth.cpu-cores", 1);
    fluid_settings_setint (settings, "synth.reverb.active", 1);
    fluid_settings_setint (settings, "synth.chorus.active", 1);
    fluid_settings_setstr (settings, "synth.midi-bank-select", "gs");
    fluid_settings_setint (settings, "synth.dynamic-sample-loading", 0);

    synth = new_fluid_synth (settings);
    if (synth == nullptr)
    {
        destroy();
        return false;
    }

    fluid_sfloader_t* loader = new_fluid_defsfloader (settings);
    fluid_sfloader_set_callbacks (loader, memOpen, memRead, memSeek, memTell, memClose);
    fluid_synth_add_sfloader (synth, loader);

    sfontId = fluid_synth_sfload (synth, makeMemoryFileName (sfData, sfSize).c_str(), 1);
    if (sfontId == FLUID_FAILED)
    {
        destroy();
        return false;
    }

    currentRate = sampleRate;

    // Catalogue the presets once.
    if (presets.empty())
    {
        if (fluid_sfont_t* sfont = fluid_synth_get_sfont_by_id (synth, sfontId))
        {
            fluid_sfont_iteration_start (sfont);
            while (fluid_preset_t* p = fluid_sfont_iteration_next (sfont))
                presets.push_back ({ fluid_preset_get_banknum (p), fluid_preset_get_num (p), fluid_preset_get_name (p) });

            std::sort (presets.begin(), presets.end(), [] (const PresetInfo& a, const PresetInfo& b)
            {
                return a.bank != b.bank ? a.bank < b.bank : a.program < b.program;
            });
        }
    }

    applyMixSettings();

    for (int ch = 0; ch < numChannels; ++ch)
        if (savedBank[ch] >= 0)
            selectProgram (ch, savedBank[ch], savedProgram[ch]);

    return true;
}

void GMSynth::applyMixSettings()
{
    if (synth == nullptr)
        return;

    fluid_synth_set_gain (synth, baseGain * gain);
    fluid_synth_set_reverb_group_level (synth, -1, reverbLevel);
    fluid_synth_set_chorus_group_level (synth, -1, chorusLevel * 4.0);
}

void GMSynth::setGain (float linearGain)
{
    if (linearGain == gain) return;
    gain = linearGain;
    if (synth != nullptr) fluid_synth_set_gain (synth, baseGain * gain);
}

void GMSynth::setReverbLevel (float level)
{
    if (level == reverbLevel) return;
    reverbLevel = level;
    if (synth != nullptr) fluid_synth_set_reverb_group_level (synth, -1, reverbLevel);
}

void GMSynth::setChorusLevel (float level)
{
    if (level == chorusLevel) return;
    chorusLevel = level;
    if (synth != nullptr) fluid_synth_set_chorus_group_level (synth, -1, chorusLevel * 4.0);
}

//==============================================================================
void GMSynth::handleMidi (const uint8_t* data, int size)
{
    if (synth == nullptr || size < 1)
        return;

    const uint8_t status = data[0];

    if (status == 0xF0)
    {
        // System exclusive (GM / GS / XG resets, master volume, tuning...).
        int len = size - 1;
        if (len > 0 && data[size - 1] == 0xF7)
            --len;
        if (len > 0)
            fluid_synth_sysex (synth, reinterpret_cast<const char*> (data + 1), len, nullptr, nullptr, nullptr, 0);
        return;
    }

    if (status == 0xFF && size == 1) // MIDI system reset
    {
        resetToGM();
        return;
    }

    if (status < 0x80 || status >= 0xF0)
        return;

    const int channel = status & 0x0F;
    const int d1 = size > 1 ? (data[1] & 0x7F) : 0;
    const int d2 = size > 2 ? (data[2] & 0x7F) : 0;

    switch (status & 0xF0)
    {
        case 0x80: fluid_synth_noteoff (synth, channel, d1); break;
        case 0x90:
            if (d2 == 0) fluid_synth_noteoff (synth, channel, d1);
            else         fluid_synth_noteon (synth, channel, d1, d2);
            break;
        case 0xA0: fluid_synth_key_pressure (synth, channel, d1, d2); break;
        case 0xB0: fluid_synth_cc (synth, channel, d1, d2); break;
        case 0xC0: fluid_synth_program_change (synth, channel, d1); break;
        case 0xD0: fluid_synth_channel_pressure (synth, channel, d1); break;
        case 0xE0: fluid_synth_pitch_bend (synth, channel, d1 | (d2 << 7)); break;
        default: break;
    }
}

void GMSynth::render (float* left, float* right, int numSamples)
{
    if (synth == nullptr)
    {
        std::fill (left, left + numSamples, 0.0f);
        std::fill (right, right + numSamples, 0.0f);
        return;
    }

    fluid_synth_write_float (synth, numSamples, left, 0, 1, right, 0, 1);
}

void GMSynth::selectProgram (int channel, int bank, int program)
{
    if (synth == nullptr || channel < 0 || channel >= numChannels)
        return;

    if (channel != drumChannel)
        fluid_synth_bank_select (synth, channel, bank);

    fluid_synth_program_change (synth, channel, program & 127);
}

bool GMSynth::getProgram (int channel, int& bank, int& program) const
{
    if (synth == nullptr)
        return false;

    int sfId = 0;
    return fluid_synth_get_program (synth, channel, &sfId, &bank, &program) == FLUID_OK;
}

void GMSynth::allSoundsOff()
{
    if (synth == nullptr) return;
    for (int ch = 0; ch < numChannels; ++ch)
        fluid_synth_all_sounds_off (synth, ch);
}

void GMSynth::resetToGM()
{
    if (synth == nullptr) return;
    static const char gmOn[] = { 0x7E, 0x7F, 0x09, 0x01 };
    fluid_synth_sysex (synth, gmOn, sizeof (gmOn), nullptr, nullptr, nullptr, 0);
}

int GMSynth::getActiveVoiceCount() const
{
    return synth != nullptr ? fluid_synth_get_active_voice_count (synth) : 0;
}

std::string GMSynth::getPresetName (int bank, int program) const
{
    for (const auto& p : presets)
        if (p.bank == bank && p.program == program)
            return p.name;
    return {};
}

} // namespace gm
