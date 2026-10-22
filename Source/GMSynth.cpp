#include "GMSynth.h"

#include <fluidsynth.h>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <cstdio>
#include <cstring>

#if defined (_WIN32) && defined (GM_FLUIDSYNTH_DLL)
 #define WIN32_LEAN_AND_MEAN
 #define NOMINMAX
 #include <windows.h>
 #include <string>
#endif

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

// FluidSynth is a shared library shipped next to the plugin binary. On Windows it is
// delay-loaded, because the system would not search the plugin's folder for it; load
// it here by full path so the delay-load stubs bind to that copy.
bool loadFluidSynthLibrary()
{
   #if defined (_WIN32) && defined (GM_FLUIDSYNTH_DLL)
    static const bool loaded = []
    {
        static const int anchor = 0;
        HMODULE self = nullptr;
        if (! GetModuleHandleExW (GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                  reinterpret_cast<LPCWSTR> (&anchor), &self))
            return false;

        wchar_t buffer[32768];
        const DWORD len = GetModuleFileNameW (self, buffer, (DWORD) (sizeof (buffer) / sizeof (buffer[0])));
        if (len == 0 || len >= sizeof (buffer) / sizeof (buffer[0]))
            return false;

        std::wstring path (buffer, len);
        path = path.substr (0, path.find_last_of (L"\\/") + 1) + GM_FLUIDSYNTH_DLL;
        return LoadLibraryExW (path.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH) != nullptr;
    }();
    return loaded;
   #else
    return true;
   #endif
}

void silenceFluidLogging()
{
    fluid_set_log_function (FLUID_DBG, nullptr, nullptr);
    fluid_set_log_function (FLUID_INFO, nullptr, nullptr);
    fluid_set_log_function (FLUID_WARN, nullptr, nullptr);
    fluid_set_log_function (FLUID_ERR, nullptr, nullptr);   // e.g. each loader that turns down a DLS file before the DLS loader takes it
}

} // namespace

//==============================================================================
GMSynth::GMSynth (const void* soundFontData, size_t soundFontSize)
    : sfData (soundFontData), sfSize (soundFontSize),
      libraryLoaded (loadFluidSynthLibrary())
{
    if (libraryLoaded)
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
    if (! libraryLoaded)
        return false;

    if (synth != nullptr && sampleRate == currentRate)
        return true;

    const std::lock_guard<std::mutex> lock (probeLock);
    ownEnvelopes.clear();

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
    fluid_settings_setint (settings, "synth.midi-channels", totalChannels);
    fluid_settings_setint (settings, "synth.audio-groups", numGroups);
    fluid_settings_setint (settings, "synth.audio-channels", numGroups);
    fluid_settings_setint (settings, "synth.effects-groups", numGroups);
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

    // The preferred file (a path) goes past the memory loader to FluidSynth's file loaders, DLS included.
    usingPreferred = false;
    if (! preferredPath.empty() && std::ifstream (preferredPath).good())
    {
        sfontId = fluid_synth_sfload (synth, preferredPath.c_str(), 1);
        usingPreferred = sfontId != FLUID_FAILED;
    }
    if (! usingPreferred)
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

    fxPointers.assign ((size_t) numGroups * 4, nullptr);
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
    fluid_synth_set_chorus_group_nr (synth, -1, 3);
    fluid_synth_set_chorus_group_speed (synth, -1, 0.4);
    fluid_synth_set_chorus_group_depth (synth, -1, 12.0);
    applyReverb();
    applyChorus();
}

// The Reverb knob grows the room as well as the level, so low settings stay close and high ones open up.
void GMSynth::applyReverb()
{
    fluid_synth_set_reverb_group_roomsize (synth, -1, 0.25 + 0.65 * reverbLevel);
    fluid_synth_set_reverb_group_damp (synth, -1, 0.3);
    fluid_synth_set_reverb_group_width (synth, -1, 1.0);
    fluid_synth_set_reverb_group_level (synth, -1, std::min (1.0, reverbLevel * 1.3));
}

// Only the chorus level follows the knob: changing its depth or voice count while it runs restarts the
// delay lines, which clicks. Those are set once, in applyMixSettings().
void GMSynth::applyChorus()
{
    fluid_synth_set_chorus_group_level (synth, -1, chorusLevel * 6.0);
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
    if (synth != nullptr) applyReverb();
}

void GMSynth::setChorusLevel (float level)
{
    if (level == chorusLevel) return;
    chorusLevel = level;
    if (synth != nullptr) applyChorus();
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
        case 0x80: noteOff (channel, d1); break;
        case 0x90:
            if (d2 == 0) noteOff (channel, d1);
            else         noteOn (channel, d1, d2);
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

    std::fill (left, left + numSamples, 0.0f);
    std::fill (right, right + numSamples, 0.0f);
    float* l[numGroups];
    float* r[numGroups];
    std::fill (l, l + numGroups, left);
    std::fill (r, r + numGroups, right);
    renderGroups (l, r, numSamples);
}

void GMSynth::renderGroups (float* const* left, float* const* right, int numSamples)
{
    if (synth == nullptr || numSamples <= 0)
        return;

    float* out[numGroups * 2];
    for (int g = 0; g < numGroups; ++g)
    {
        out[g * 2] = left[g];
        out[g * 2 + 1] = right[g];
        // Each group's reverb and chorus return to the same output as its dry sound.
        for (int k = 0; k < 2; ++k)
        {
            fxPointers[(size_t) (g * 4 + k * 2)] = left[g];
            fxPointers[(size_t) (g * 4 + k * 2 + 1)] = right[g];
        }
    }
    fluid_synth_process (synth, numSamples, numGroups * 4, fxPointers.data(), numGroups * 2, out);
}

void GMSynth::selectProgram (int channel, int bank, int program)
{
    if (synth == nullptr || channel < 0 || channel >= totalChannels)
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
    fluid_synth_all_sounds_off (synth, -1);
}

void GMSynth::resetToGM()
{
    if (synth == nullptr) return;
    static const char gmOn[] = { 0x7E, 0x7F, 0x09, 0x01 };
    fluid_synth_sysex (synth, gmOn, sizeof (gmOn), nullptr, nullptr, nullptr, 0);
}

//==============================================================================
void GMSynth::noteOn (int channel, int key, int velocity)
{
    if (synth == nullptr) return;
    fluid_synth_noteon (synth, channel, key, velocity);
    if (shapes[channel].enabled)
        applyShape (channel, key, true);
}

void GMSynth::noteOff (int channel, int key)                    { if (synth != nullptr) fluid_synth_noteoff (synth, channel, key); }
void GMSynth::controlChange (int channel, int cc, int value)    { if (synth != nullptr) fluid_synth_cc (synth, channel, cc, value); }
void GMSynth::pitchBend (int channel, int value14)              { if (synth != nullptr) fluid_synth_pitch_bend (synth, channel, value14); }
void GMSynth::channelPressure (int channel, int value)          { if (synth != nullptr) fluid_synth_channel_pressure (synth, channel, value); }
void GMSynth::keyPressure (int channel, int key, int value)     { if (synth != nullptr) fluid_synth_key_pressure (synth, channel, key, value); }

void GMSynth::setVoiceShape (int channel, const VoiceShape& shape)
{
    if (channel < 0 || channel >= totalChannels || shapes[channel] == shape)
        return;
    shapes[channel] = shape;
    if (synth != nullptr && shape.enabled)
        applyShape (channel, -1, false);
}

// Sets the shape's generators on the channel's voices: every sounding voice, or (right after a
// note-on) only the voices that note just started, which share the newest voice ID.
void GMSynth::applyShape (int channel, int key, bool newestOnly)
{
    constexpr int maxVoices = 256;
    fluid_voice_t* voices[maxVoices + 1] = {};
    fluid_synth_get_voicelist (synth, voices, maxVoices, -1);

    unsigned int newest = 0;
    if (newestOnly)
        for (int i = 0; voices[i] != nullptr; ++i)
            if (fluid_voice_get_channel (voices[i]) == channel && fluid_voice_get_key (voices[i]) == key)
                newest = std::max (newest, fluid_voice_get_id (voices[i]));

    const auto& s = shapes[channel];
    struct Gen { int gen; float value; };
    const Gen filterGens[] = { { GEN_FILTERFC, s.filterCents }, { GEN_FILTERQ, s.filterQ } };
    const Gen envelopeGens[] = {
        { GEN_VOLENVDELAY, -12000.0f }, { GEN_VOLENVHOLD, -12000.0f },
        { GEN_KEYTOVOLENVHOLD, 0.0f }, { GEN_KEYTOVOLENVDECAY, 0.0f },
        { GEN_VOLENVATTACK, s.attack }, { GEN_VOLENVDECAY, s.decay },
        { GEN_VOLENVSUSTAIN, s.sustain }, { GEN_VOLENVRELEASE, s.release } };

    for (int i = 0; voices[i] != nullptr; ++i)
    {
        fluid_voice_t* v = voices[i];
        if (fluid_voice_get_channel (v) != channel)
            continue;
        if (newestOnly && (fluid_voice_get_key (v) != key || fluid_voice_get_id (v) != newest))
            continue;
        auto set = [v] (const Gen& g) { fluid_voice_gen_set (v, g.gen, g.value); fluid_voice_update_param (v, g.gen); };
        if (s.filter)   for (const auto& g : filterGens) set (g);
        if (s.envelope) for (const auto& g : envelopeGens) set (g);
    }
}

void GMSynth::setFilter (int channel, float cents, float q)
{
    if (channel < 0 || channel >= totalChannels) return;
    auto& s = shapes[channel];
    if (s.filterCents == cents && s.filterQ == q) return;
    s.filterCents = cents;
    s.filterQ = q;
    if (synth == nullptr || ! s.enabled || ! s.filter) return;

    constexpr int maxVoices = 256;
    fluid_voice_t* voices[maxVoices + 1] = {};
    fluid_synth_get_voicelist (synth, voices, maxVoices, -1);
    for (int i = 0; voices[i] != nullptr; ++i)
    {
        if (fluid_voice_get_channel (voices[i]) != channel) continue;
        fluid_voice_gen_set (voices[i], GEN_FILTERFC, cents);
        fluid_voice_update_param (voices[i], GEN_FILTERFC);
        fluid_voice_gen_set (voices[i], GEN_FILTERQ, q);
        fluid_voice_update_param (voices[i], GEN_FILTERQ);
    }
}

void GMSynth::setPitchBendRange (int channel, int semitones)
{
    if (synth != nullptr) fluid_synth_pitch_wheel_sens (synth, channel, semitones);
}

void GMSynth::setFineTune (int channel, float cents)
{
    if (synth != nullptr) fluid_synth_set_gen (synth, channel, GEN_FINETUNE, cents);
}

void GMSynth::setPortamento (int channel, int milliseconds)
{
    if (synth == nullptr) return;
    milliseconds = std::clamp (milliseconds, 0, 16383);
    fluid_synth_set_portamento_mode (synth, channel, FLUID_CHANNEL_PORTAMENTO_MODE_EACH_NOTE);
    // Sending the LSB switches FluidSynth to a linear time in milliseconds (MSB * 128 + LSB).
    fluid_synth_cc (synth, channel, 5, milliseconds >> 7);
    fluid_synth_cc (synth, channel, 37, milliseconds & 127);
    fluid_synth_cc (synth, channel, 65, milliseconds > 0 ? 127 : 0);
}

void GMSynth::setMonoChannels (int count)
{
    if (synth == nullptr) return;
    fluid_synth_reset_basic_channel (synth, -1);
    if (count <= 0)
    {
        fluid_synth_set_basic_channel (synth, 0, FLUID_CHANNEL_MODE_OMNION_POLY, 0);
        return;
    }
    fluid_synth_set_basic_channel (synth, 0, FLUID_CHANNEL_MODE_OMNIOFF_MONO, count);
    fluid_synth_set_basic_channel (synth, count, FLUID_CHANNEL_MODE_OMNION_POLY, 0);
    for (int ch = 0; ch < count; ++ch)
        fluid_synth_set_legato_mode (synth, ch, FLUID_CHANNEL_LEGATO_MODE_MULTI_RETRIGGER);
}

// Plays the program silently on the last engine channel (never used for notes) and reads the envelope
// its voices start with.
bool GMSynth::getOwnEnvelope (int program, float& attack, float& decay, float& sustain, float& release)
{
    const std::lock_guard<std::mutex> lock (probeLock);
    if (synth == nullptr) return false;

    auto found = ownEnvelopes.find (program);
    if (found == ownEnvelopes.end())
    {
        const int ch = totalChannels - 1;
        fluid_synth_cc (synth, ch, 7, 0);
        fluid_synth_bank_select (synth, ch, 0);
        fluid_synth_program_change (synth, ch, program & 127);
        fluid_synth_noteon (synth, ch, 60, 100);

        constexpr int maxVoices = 256;
        fluid_voice_t* voices[maxVoices + 1] = {};
        fluid_synth_get_voicelist (synth, voices, maxVoices, -1);
        std::array<float, 4> env { -12000.0f, -12000.0f, 0.0f, -12000.0f };
        bool any = false;
        for (int i = 0; voices[i] != nullptr; ++i)
        {
            if (fluid_voice_get_channel (voices[i]) != ch) continue;
            // Of the note's voices, keep the slowest of each stage: that is what you hear.
            const float a = fluid_voice_gen_get (voices[i], GEN_VOLENVATTACK);
            const float d = fluid_voice_gen_get (voices[i], GEN_VOLENVDECAY);
            const float s = fluid_voice_gen_get (voices[i], GEN_VOLENVSUSTAIN);
            const float r = fluid_voice_gen_get (voices[i], GEN_VOLENVRELEASE);
            env = any ? std::array<float, 4> { std::max (env[0], a), std::max (env[1], d), std::min (env[2], s), std::max (env[3], r) }
                      : std::array<float, 4> { a, d, s, r };
            any = true;
        }
        fluid_synth_all_sounds_off (synth, ch);
        if (! any) return false;
        found = ownEnvelopes.emplace (program, env).first;
    }

    const auto& e = found->second;
    auto seconds = [] (float timecents) { return timecents <= -11999.0f ? 0.0f : (float) std::pow (2.0, timecents / 1200.0); };
    attack = seconds (e[0]);
    decay = seconds (e[1]);
    sustain = (float) std::pow (10.0, -std::max (0.0f, e[2]) / 200.0);
    release = seconds (e[3]);
    return true;
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
