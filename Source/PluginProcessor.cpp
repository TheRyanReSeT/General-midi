#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <BinaryData.h>

namespace
{
constexpr double defaultSampleRate = 44100.0;
constexpr int uiSource = 16;   // layerNote source slot for the on-screen keyboard

// Sound for the GM Player's default song lineup (1-based GM programs, channel 10 is drums).
constexpr int defaultGmPrograms[16] = { 1, 34, 49, 26, 57, 74, 5, 82, 89, 1, 12, 66, 53, 123, 128, 99 };

int toTimecents (float seconds) { return juce::roundToInt (1200.0 * std::log2 (std::max (0.001, (double) seconds))); }
int toCC (float v)              { return juce::jlimit (0, 127, juce::roundToInt (v * 127.0f)); }
}

GemiProcessor::GemiProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      parameters (*this, nullptr, "gemi", createParameters()),
      synth (GMData::GeneralUserGS_sf2, (size_t) GMData::GeneralUserGS_sf2Size)
{
    for (int l = 0; l < gemi::numLayers; ++l)
    {
        auto p = [&] (const char* suffix) { return parameters.getRawParameterValue (gemi::layerId (l, suffix)); };
        layerParams[(size_t) l] = { p ("on"), p ("level"), p ("pan"), p ("cutoff"), p ("reso"), p ("attack"),
                                    p ("decay"), p ("sustain"), p ("release"), p ("oct"), p ("semi"), p ("fine") };
        layerProgram[(size_t) l] = gemi::layerDefaults (l).program;
    }
    reverbParam  = parameters.getRawParameterValue ("reverb");
    chorusParam  = parameters.getRawParameterValue ("chorus");
    vibratoParam = parameters.getRawParameterValue ("vibrato");
    glideParam   = parameters.getRawParameterValue ("glide");
    volumeParam  = parameters.getRawParameterValue ("volume");
    monoParam    = parameters.getRawParameterValue ("mono");

    for (int ch = 0; ch < numChannels; ++ch)
        gmProgram[(size_t) ch] = defaultGmPrograms[ch];
    std::memset (layerNote, -1, sizeof (layerNote));

    auto* prefs = new juce::DynamicObject();
    prefs->setProperty ("theme", "light");
    prefs->setProperty ("reduceMotion", false);
    prefs->setProperty ("size", 1.0);
    prefs->setProperty ("gmTip", true);
    uiPrefs = juce::var (prefs);
    presetName = presets.all().getFirst().name;

    // Load the SoundFont straight away; prepareToPlay() reloads only if the rate differs.
    synth.prepare (defaultSampleRate);
}

juce::AudioProcessorValueTreeState::ParameterLayout GemiProcessor::createParameters()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    for (const auto& spec : gemi::paramSpecs())
    {
        const String id (spec.id);
        switch (spec.kind)
        {
            case gemi::Kind::knob:
                layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, spec.name, NormalisableRange<float> (0.0f, 1.0f), spec.defaultValue,
                                AudioParameterFloatAttributes().withStringFromValueFunction ([id] (float v, int) { return gemi::displayText (id, v); })));
                break;
            case gemi::Kind::toggle:
                layout.add (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, spec.name, spec.defaultValue >= 0.5f));
                break;
            case gemi::Kind::stepper:
                layout.add (std::make_unique<AudioParameterInt> (ParameterID { id, 1 }, spec.name, spec.minValue, spec.maxValue, (int) spec.defaultValue,
                                AudioParameterIntAttributes().withStringFromValueFunction ([] (int v, int) { return (v > 0 ? "+" : "") + String (v); })));
                break;
        }
    }
    return layout;
}

void GemiProcessor::prepareToPlay (double sampleRate, int)
{
    synth.prepare (sampleRate);
    engineFresh = true;   // a new synth instance: apply everything again
}

bool GemiProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

//==============================================================================
void GemiProcessor::enterMode (Mode m)
{
    synth.allSoundsOff();
    std::memset (layerNote, -1, sizeof (layerNote));
    channelNotes.fill (0);
    heldCount.fill (0);
    for (auto& b : heldBits) b = 0;

    synth.resetToGM();
    for (int ch = 0; ch < numChannels; ++ch)
    {
        synth.setVoiceShape (ch, {});
        synth.setFineTune (ch, 0.0f);
        synth.setPortamento (ch, 0);
    }
    synth.setMonoChannels (0);

    applied = {};
    appliedVibrato = appliedGlide = appliedMono = -1;
    appliedGmProgram.fill (-1);
    appliedKit = -1;
    appliedMute = appliedSolo = 0;

    if (m == Mode::synth)
    {
        for (int l = 0; l < gemi::numLayers; ++l)
        {
            synth.controlChange (l, 91, 64);   // reverb and chorus sends; the global knobs set the amount
            synth.controlChange (l, 93, 64);
        }
    }
    appliedMode = m;
}

// Pushes the layer and global parameters into the engine, touching only what changed.
void GemiProcessor::applySynthSettings (bool force)
{
    for (int l = 0; l < gemi::numLayers; ++l)
    {
        const auto& P = layerParams[(size_t) l];
        auto& A = applied[(size_t) l];

        const bool on = P.on->load() >= 0.5f;
        if (A.on && ! on)
            releaseLayer (l);
        A.on = on;

        const int program = layerProgram[(size_t) l].load();
        if (force || program != A.program) { synth.selectProgram (l, 0, program - 1); A.program = program; }

        const int level = toCC (P.level->load());
        if (force || level != A.level) { synth.controlChange (l, 7, level); A.level = level; }
        const int pan = toCC (P.pan->load());
        if (force || pan != A.pan) { synth.controlChange (l, 10, pan); A.pan = pan; }

        const int fine = juce::roundToInt (P.fine->load());
        if (force || fine != A.fine) { synth.setFineTune (l, (float) fine); A.fine = fine; }
        A.octave = juce::roundToInt (P.oct->load());
        A.semi = juce::roundToInt (P.semi->load());

        gm::VoiceShape shape;
        shape.enabled = true;
        shape.filterCents = (float) juce::jlimit (1500.0, 13500.0, 1200.0 * std::log2 (gemi::cutoffHz (P.cutoff->load()) / 8.176));
        shape.filterQ = P.reso->load() * 240.0f;
        shape.attack  = (float) toTimecents (gemi::attackSeconds (P.attack->load()));
        shape.decay   = (float) toTimecents (gemi::decaySeconds (P.decay->load()));
        const float sustain = P.sustain->load();
        shape.sustain = sustain <= 0.001f ? 1440.0f : (float) juce::jlimit (0.0, 1440.0, -200.0 * std::log10 ((double) sustain));
        shape.release = (float) toTimecents (gemi::releaseSeconds (P.release->load()));
        if (force || shape != A.shape) { synth.setVoiceShape (l, shape); A.shape = shape; }
    }

    const int vibrato = std::max (toCC (vibratoParam->load()), incomingMod);
    if (force || vibrato != appliedVibrato)
    {
        for (int l = 0; l < gemi::numLayers; ++l) synth.controlChange (l, 1, vibrato);
        appliedVibrato = vibrato;
    }
    const int glide = juce::roundToInt (gemi::glideMs (glideParam->load()));
    if (force || glide != appliedGlide)
    {
        for (int l = 0; l < gemi::numLayers; ++l) synth.setPortamento (l, glide);
        appliedGlide = glide;
    }
    const int mono = monoParam->load() >= 0.5f ? 1 : 0;
    if (force || mono != appliedMono)
    {
        synth.allSoundsOff();
        std::memset (layerNote, -1, sizeof (layerNote));
        synth.setMonoChannels (mono ? gemi::numLayers : 0);
        appliedMono = mono;
    }
}

void GemiProcessor::releaseLayer (int layer)
{
    for (auto& source : layerNote)
        for (auto& key : source)
            key[layer] = -1;
    synth.controlChange (layer, 123, 0);   // all notes off
}

void GemiProcessor::trackHeld (int note, bool on)
{
    if (on) ++heldCount[(size_t) note];
    else if (heldCount[(size_t) note] > 0) --heldCount[(size_t) note];
    const uint32_t bit = 1u << (note & 31);
    auto& word = heldBits[(size_t) (note >> 5)];
    if (heldCount[(size_t) note] > 0) word.fetch_or (bit); else word.fetch_and (~bit);
}

void GemiProcessor::synthNoteOn (int source, int note, int velocity)
{
    trackHeld (note, true);
    for (int l = 0; l < gemi::numLayers; ++l)
    {
        const auto& A = applied[(size_t) l];
        if (! A.on) continue;
        const int key = note + A.octave * 12 + A.semi;
        if (key < 0 || key > 127) continue;
        if (layerNote[source][note][l] >= 0)
            synth.noteOff (l, layerNote[source][note][l]);
        layerNote[source][note][l] = (int8_t) key;
        synth.noteOn (l, key, velocity);
    }
}

void GemiProcessor::synthNoteOff (int source, int note)
{
    trackHeld (note, false);
    for (int l = 0; l < gemi::numLayers; ++l)
    {
        auto& key = layerNote[source][note][l];
        if (key >= 0) { synth.noteOff (l, key); key = -1; }
    }
}

void GemiProcessor::handleSynthMidi (const juce::MidiMessage& m, int source)
{
    if (m.isNoteOn())        { synthNoteOn (source, m.getNoteNumber(), m.getVelocity()); return; }
    if (m.isNoteOff())       { synthNoteOff (source, m.getNoteNumber()); return; }

    if (m.isPitchWheel())
    {
        for (int l = 0; l < gemi::numLayers; ++l) synth.pitchBend (l, m.getPitchWheelValue());
        return;
    }
    if (m.isChannelPressure())
    {
        for (int l = 0; l < gemi::numLayers; ++l) synth.channelPressure (l, m.getChannelPressureValue());
        return;
    }
    if (m.isAftertouch())
    {
        for (int l = 0; l < gemi::numLayers; ++l)
            if (const int key = layerNote[source][m.getNoteNumber()][l]; key >= 0)
                synth.keyPressure (l, key, m.getAfterTouchValue());
        return;
    }
    if (m.isController())
    {
        const int cc = m.getControllerNumber(), value = m.getControllerValue();
        if (cc == 1)   // the mod wheel adds to the Vibrato knob
        {
            incomingMod = value;
            const int vibrato = std::max (toCC (vibratoParam->load()), incomingMod);
            if (vibrato != appliedVibrato)
            {
                for (int l = 0; l < gemi::numLayers; ++l) synth.controlChange (l, 1, vibrato);
                appliedVibrato = vibrato;
            }
            return;
        }
        // Performance controllers pass to every layer; the layers' own level, pan, glide and sends stay put.
        static constexpr int passThrough[] = { 2, 4, 11, 64, 66, 67, 68, 120, 123 };
        if (std::find (std::begin (passThrough), std::end (passThrough), cc) != std::end (passThrough))
            for (int l = 0; l < gemi::numLayers; ++l) synth.controlChange (l, cc, value);
        if (cc == 120 || cc == 123)
            std::memset (layerNote, -1, sizeof (layerNote));
    }
}

bool GemiProcessor::channelAudible (int channel) const
{
    const uint32_t bit = 1u << channel, solo = soloMask.load();
    return (muteMask.load() & bit) == 0 && (solo == 0 || (solo & bit) != 0);
}

void GemiProcessor::handleGMMidi (const juce::MidiMessage& m)
{
    if (m.isSysEx() || m.getRawDataSize() < 1)
    {
        synth.handleMidi (m.getRawData(), m.getRawDataSize());
        return;
    }
    const int inChannel = m.getChannel() - 1;
    if (inChannel < 0)
    {
        synth.handleMidi (m.getRawData(), m.getRawDataSize());
        return;
    }

    // Follow keyboard: a controller on MIDI channel 1 plays whichever channel is selected.
    int channel = inChannel;
    if (followKeyboard.load() && inChannel == 0)
        channel = selectedChannel.load() - 1;

    if (m.isNoteOn())
    {
        if (! channelAudible (channel)) return;
        ++channelNotes[(size_t) channel];
        trackHeld (m.getNoteNumber(), true);
    }
    else if (m.isNoteOff())
    {
        if (channelNotes[(size_t) channel] > 0) --channelNotes[(size_t) channel];
        trackHeld (m.getNoteNumber(), false);
    }
    else if (m.isProgramChange())
    {
        // The song chooses a sound: show it in the UI, as a hardware GM module would.
        const int program = m.getProgramChangeNumber() + 1;
        if (channel == gm::GMSynth::drumChannel) { drumKit = program; appliedKit = program; }
        else { gmProgram[(size_t) channel] = program; appliedGmProgram[(size_t) channel] = program; }
        programsChanged = true;
    }

    auto copy = m;
    copy.setChannel (channel + 1);
    synth.handleMidi (copy.getRawData(), copy.getRawDataSize());
}

//==============================================================================
void GemiProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    buffer.clear();

    if (! synth.isReady())
        return;

    const auto wanted = (Mode) mode.load();
    const bool force = engineFresh || wanted != appliedMode;
    if (force)
    {
        enterMode (wanted);
        engineFresh = false;
    }
    if (pendingPanic.exchange (false))
    {
        synth.allSoundsOff();
        std::memset (layerNote, -1, sizeof (layerNote));
        channelNotes.fill (0);
        heldCount.fill (0);
        for (auto& b : heldBits) b = 0;
    }

    synth.setReverbLevel (reverbParam->load());
    synth.setChorusLevel (chorusParam->load());
    synth.setGain (juce::Decibels::decibelsToGain (gemi::volumeDb (volumeParam->load()), -48.0f));

    if (appliedMode == Mode::synth)
    {
        applySynthSettings (force);
    }
    else
    {
        if (pendingReset.exchange (false))
        {
            enterMode (Mode::gmPlayer);
            programsChanged = true;
        }
        for (int ch = 0; ch < numChannels; ++ch)
        {
            if (ch == gm::GMSynth::drumChannel)
                continue;
            const int program = gmProgram[(size_t) ch].load();
            if (program != appliedGmProgram[(size_t) ch]) { synth.selectProgram (ch, 0, program - 1); appliedGmProgram[(size_t) ch] = program; }
        }
        const int kit = drumKit.load();
        if (kit != appliedKit) { synth.selectProgram (gm::GMSynth::drumChannel, gm::GMSynth::drumBank, kit - 1); appliedKit = kit; }

        // Newly muted channels stop sounding at once.
        const uint32_t mute = muteMask.load(), solo = soloMask.load();
        if (mute != appliedMute || solo != appliedSolo)
        {
            for (int ch = 0; ch < numChannels; ++ch)
                if (! channelAudible (ch)) { synth.controlChange (ch, 123, 0); channelNotes[(size_t) ch] = 0; }
            appliedMute = mute; appliedSolo = solo;
        }
    }

    // Notes from the on-screen keyboard.
    {
        const auto scope = uiNoteFifo.read (uiNoteFifo.getNumReady());
        auto play = [&] (int start, int size)
        {
            for (int i = start; i < start + size; ++i)
            {
                const auto& n = uiNotes[(size_t) i];
                if (appliedMode == Mode::synth)
                {
                    if (n.on) synthNoteOn (uiSource, n.note, n.velocity);
                    else      synthNoteOff (uiSource, n.note);
                }
                else
                {
                    const int channel = selectedChannel.load();
                    handleGMMidi (n.on ? juce::MidiMessage::noteOn (channel, n.note, (juce::uint8) n.velocity)
                                       : juce::MidiMessage::noteOff (channel, n.note));
                }
            }
        };
        play (scope.startIndex1, scope.blockSize1);
        play (scope.startIndex2, scope.blockSize2);
    }

    // Render between MIDI events so timing is sample accurate.
    auto* left = buffer.getWritePointer (0);
    auto* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;
    juce::HeapBlock<float> scratch;
    if (right == nullptr) { scratch.allocate ((size_t) numSamples, true); right = scratch.get(); }

    int position = 0;
    for (const auto metadata : midi)
    {
        const int when = juce::jlimit (0, numSamples, metadata.samplePosition);
        if (when > position) { synth.render (left + position, right + position, when - position); position = when; }

        const auto message = metadata.getMessage();
        if (appliedMode == Mode::synth)
        {
            if (message.getChannel() > 0) handleSynthMidi (message, message.getChannel() - 1);
        }
        else
        {
            handleGMMidi (message);
        }
    }
    if (position < numSamples)
        synth.render (left + position, right + position, numSamples - position);

    if (buffer.getNumChannels() == 1)   // mono output: fold both sides down
    {
        juce::FloatVectorOperations::add (left, right, numSamples);
        juce::FloatVectorOperations::multiply (left, 0.5f, numSamples);
    }

    uint32_t active = 0;
    for (int ch = 0; ch < numChannels; ++ch)
        if (channelNotes[(size_t) ch] > 0) active |= 1u << ch;
    activeChannelMask = active;
    activeVoices = synth.getActiveVoiceCount();
}

//==============================================================================
void GemiProcessor::queueUiNote (int note, int velocity, bool on)
{
    if (note < 0 || note > 127) return;
    const auto scope = uiNoteFifo.write (1);
    if (scope.blockSize1 > 0) uiNotes[(size_t) scope.startIndex1] = { note, velocity, on };
    else if (scope.blockSize2 > 0) uiNotes[(size_t) scope.startIndex2] = { note, velocity, on };
}

void GemiProcessor::getHeldNotes (juce::Array<int>& notes) const
{
    notes.clearQuick();
    for (int w = 0; w < 4; ++w)
    {
        const uint32_t bits = heldBits[(size_t) w].load();
        for (int b = 0; b < 32; ++b)
            if (bits & (1u << b)) notes.add (w * 32 + b);
    }
}

juce::var GemiProcessor::getUiState() const
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("view", mode.load() == (int) Mode::gmPlayer ? "gm" : "synth");
    juce::Array<juce::var> layers, gm;
    for (auto& p : layerProgram) layers.add (p.load());
    for (auto& p : gmProgram) gm.add (p.load());
    o->setProperty ("layerProg", layers);
    o->setProperty ("gmProg", gm);
    o->setProperty ("kit", drumKit.load());
    o->setProperty ("mute", (int) muteMask.load());
    o->setProperty ("solo", (int) soloMask.load());
    o->setProperty ("ch", selectedChannel.load());
    o->setProperty ("follow", followKeyboard.load());
    const juce::ScopedLock sl (prefsLock);
    for (const auto& key : { "theme", "reduceMotion", "size", "gmTip" })
        o->setProperty (key, uiPrefs[key]);
    return juce::var (o);
}

void GemiProcessor::setUiState (const juce::var& v)
{
    if (! v.isObject()) return;
    if (v.hasProperty ("view")) mode = v["view"].toString() == "gm" ? (int) Mode::gmPlayer : (int) Mode::synth;
    if (auto* a = v["layerProg"].getArray())
        for (int l = 0; l < juce::jmin (gemi::numLayers, a->size()); ++l)
            layerProgram[(size_t) l] = juce::jlimit (1, 128, (int) (*a)[l]);
    if (auto* a = v["gmProg"].getArray())
        for (int ch = 0; ch < juce::jmin (numChannels, a->size()); ++ch)
            gmProgram[(size_t) ch] = juce::jlimit (1, 128, (int) (*a)[ch]);
    if (v.hasProperty ("kit"))    drumKit = juce::jlimit (1, 128, (int) v["kit"]);
    if (v.hasProperty ("mute"))   muteMask = (uint32_t) (int) v["mute"];
    if (v.hasProperty ("solo"))   soloMask = (uint32_t) (int) v["solo"];
    if (v.hasProperty ("ch"))     selectedChannel = juce::jlimit (1, 16, (int) v["ch"]);
    if (v.hasProperty ("follow")) followKeyboard = (bool) v["follow"];
    const juce::ScopedLock sl (prefsLock);
    if (auto* prefs = uiPrefs.getDynamicObject())
        for (const auto& key : { "theme", "reduceMotion", "size", "gmTip" })
            if (v.hasProperty (key)) prefs->setProperty (key, v[key]);
}

//==============================================================================
juce::var GemiProcessor::currentPresetData() const
{
    auto* params = new juce::DynamicObject();
    for (const auto& spec : gemi::paramSpecs())
        if (juce::String (spec.id) != "volume")
            params->setProperty (spec.id, parameters.getRawParameterValue (spec.id)->load());
    juce::Array<juce::var> progs;
    for (auto& p : layerProgram) progs.add (p.load());
    auto* o = new juce::DynamicObject();
    o->setProperty ("params", juce::var (params));
    o->setProperty ("layerProg", progs);
    return juce::var (o);
}

void GemiProcessor::applyPresetData (const juce::var& data)
{
    const auto params = data["params"];
    for (const auto& spec : gemi::paramSpecs())
    {
        if (! params.hasProperty (spec.id)) continue;
        if (auto* p = parameters.getParameter (spec.id))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->convertTo0to1 ((float) params[spec.id]));
            p->endChangeGesture();
        }
    }
    if (auto* a = data["layerProg"].getArray())
        for (int l = 0; l < juce::jmin (gemi::numLayers, a->size()); ++l)
            layerProgram[(size_t) l] = juce::jlimit (1, 128, (int) (*a)[l]);
}

void GemiProcessor::loadPreset (int index)
{
    const auto& all = presets.all();
    if (! juce::isPositiveAndBelow (index, all.size())) return;
    applyPresetData (all.getReference (index).data);
    {
        const juce::ScopedLock sl (prefsLock);
        presetIndex = index;
        presetName = all.getReference (index).name;
    }
    if (onPresetsChanged) onPresetsChanged();
}

void GemiProcessor::stepPreset (int delta)
{
    const int n = presets.all().size();
    if (n > 0) loadPreset (((getPresetIndex() + delta) % n + n) % n);
}

bool GemiProcessor::savePreset (const juce::String& name)
{
    if (! presets.save (name, currentPresetData()))
        return false;
    {
        const juce::ScopedLock sl (prefsLock);
        presetIndex = juce::jmax (0, presets.indexOf (name.trim()));
        presetName = name.trim();
    }
    if (onPresetsChanged) onPresetsChanged();
    return true;
}

//==============================================================================
void GemiProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    state.setProperty ("ui", juce::JSON::toString (getUiState(), true), nullptr);
    state.setProperty ("combo", getPresetName(), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void GemiProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (parameters.state.getType()))
        return;
    auto state = juce::ValueTree::fromXml (*xml);
    setUiState (juce::JSON::parse (state.getProperty ("ui").toString()));
    {
        const juce::ScopedLock sl (prefsLock);
        presetName = state.getProperty ("combo", presetName).toString();
        presetIndex = juce::jmax (0, presets.indexOf (presetName));
    }
    state.removeProperty ("ui", nullptr);
    state.removeProperty ("combo", nullptr);
    parameters.replaceState (state);
    if (onPresetsChanged) onPresetsChanged();
}

juce::AudioProcessorEditor* GemiProcessor::createEditor()
{
    return new GemiEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new GemiProcessor();
}
