#include "Presets.h"
#include "Parameters.h"

namespace gemi
{

namespace
{
constexpr const char* fileExtension = ".gemicombo";

// One layer of a factory combo, in the same units as the parameters.
struct Layer
{
    int program;
    float level, pan, cutoff, reso, attack, decay, sustain, release;
    int octave = 0, semi = 0, fine = 0;
    bool on = true;
};

struct Combo
{
    const char* name;
    Layer layers[numLayers];
    float reverb = 0.70f, chorus = 0.35f, vibrato = 0.15f, glide = 0.0f;
    bool mono = false;
    int layerCount = numLayers;
};

const Combo combos[] = {
    { "Grand Piano",
      { { 1,  0.80f, 0.50f, 0.90f, 0.10f, 0.00f, 0.90f, 0.25f, 0.15f },
        { 49, 0.70f, 0.50f, 0.90f, 0.10f, 0.12f, 0.30f, 0.90f, 0.20f },
        { 90, 0.60f, 0.50f, 0.85f, 0.10f, 0.35f, 0.50f, 0.85f, 0.50f } }, 0.45f, 0.20f, 0.0f, 0.0f, false, 1 },
    { "Aurora Choir",
      { { 53, 0.80f, 0.40f, 0.72f, 0.20f, 0.20f, 0.15f, 0.80f, 0.40f, 0, 0, 0 },
        { 95, 0.60f, 0.60f, 0.55f, 0.35f, 0.55f, 0.40f, 0.65f, 0.80f, 0, 7, 6 },
        { 9,  0.45f, 0.50f, 0.90f, 0.10f, 0.00f, 0.70f, 0.00f, 0.30f, 1, 0, -4 } } },
    { "Glass Garden",
      { { 5,  0.75f, 0.45f, 0.85f, 0.10f, 0.00f, 0.60f, 0.35f, 0.40f },
        { 89, 0.50f, 0.55f, 0.60f, 0.20f, 0.45f, 0.50f, 0.70f, 0.70f },
        { 11, 0.40f, 0.50f, 0.95f, 0.05f, 0.00f, 0.50f, 0.00f, 0.50f, 1 } }, 0.80f, 0.40f, 0.10f },
    { "Sunday Organ",
      { { 17, 0.70f, 0.40f, 0.80f, 0.10f, 0.02f, 0.10f, 1.00f, 0.15f },
        { 20, 0.55f, 0.60f, 0.75f, 0.10f, 0.05f, 0.10f, 1.00f, 0.30f, -1 },
        { 53, 0.35f, 0.50f, 0.70f, 0.10f, 0.40f, 0.30f, 0.80f, 0.50f } }, 0.75f, 0.20f, 0.25f },
    { "Midnight Brass",
      { { 62, 0.75f, 0.45f, 0.70f, 0.20f, 0.08f, 0.30f, 0.80f, 0.25f },
        { 63, 0.50f, 0.55f, 0.55f, 0.30f, 0.10f, 0.30f, 0.75f, 0.30f, 0, 0, 6 },
        { 49, 0.45f, 0.50f, 0.65f, 0.10f, 0.30f, 0.40f, 0.80f, 0.60f } }, 0.60f, 0.25f, 0.10f },
    { "Retro Lead",
      { { 81, 0.70f, 0.45f, 0.65f, 0.40f, 0.00f, 0.30f, 0.80f, 0.15f },
        { 82, 0.55f, 0.55f, 0.60f, 0.35f, 0.00f, 0.30f, 0.80f, 0.15f, 0, 0, 8 },
        { 39, 0.50f, 0.50f, 0.45f, 0.30f, 0.00f, 0.25f, 0.70f, 0.10f, -1 } }, 0.35f, 0.20f, 0.20f, 0.12f, true },
    { "Ocean Dream",
      { { 123, 0.55f, 0.50f, 0.70f, 0.10f, 0.60f, 0.50f, 0.80f, 0.80f },
        { 92,  0.60f, 0.40f, 0.60f, 0.20f, 0.50f, 0.50f, 0.75f, 0.85f },
        { 47,  0.50f, 0.60f, 0.90f, 0.05f, 0.00f, 0.60f, 0.00f, 0.60f } }, 0.85f, 0.45f, 0.10f },
    { "Toy Box",
      { { 11, 0.70f, 0.40f, 0.95f, 0.05f, 0.00f, 0.55f, 0.00f, 0.40f },
        { 14, 0.50f, 0.60f, 0.90f, 0.05f, 0.00f, 0.35f, 0.00f, 0.25f },
        { 10, 0.40f, 0.50f, 0.95f, 0.05f, 0.00f, 0.60f, 0.00f, 0.50f, 1 } }, 0.55f, 0.15f, 0.0f },
    { "Velvet Keys",
      { { 1,  0.80f, 0.45f, 0.80f, 0.10f, 0.00f, 0.90f, 0.20f, 0.30f },
        { 51, 0.40f, 0.55f, 0.55f, 0.15f, 0.40f, 0.50f, 0.75f, 0.70f },
        { 5,  0.35f, 0.50f, 0.75f, 0.10f, 0.00f, 0.70f, 0.30f, 0.35f } }, 0.65f, 0.30f, 0.05f },
    { "Space Bells",
      { { 15, 0.70f, 0.40f, 0.85f, 0.15f, 0.00f, 0.80f, 0.10f, 0.70f },
        { 96, 0.50f, 0.60f, 0.55f, 0.30f, 0.60f, 0.60f, 0.70f, 0.90f },
        { 99, 0.40f, 0.50f, 0.80f, 0.20f, 0.10f, 0.60f, 0.40f, 0.80f, 1 } }, 0.90f, 0.50f, 0.10f },
    { "Gunshot Disco",
      { { 128, 0.65f, 0.40f, 0.85f, 0.10f, 0.00f, 0.30f, 0.00f, 0.20f },
        { 82,  0.50f, 0.60f, 0.60f, 0.40f, 0.00f, 0.25f, 0.70f, 0.15f },
        { 117, 0.55f, 0.50f, 0.80f, 0.10f, 0.00f, 0.40f, 0.00f, 0.30f, -1 } }, 0.45f, 0.30f, 0.0f },
};

juce::var comboData (const Combo& c)
{
    auto* params = new juce::DynamicObject();
    juce::Array<juce::var> progs;
    for (int l = 0; l < numLayers; ++l)
    {
        const auto& L = c.layers[l];
        auto set = [&] (const char* suffix, float v) { params->setProperty (layerId (l, suffix), v); };
        set ("on", L.on ? 1.0f : 0.0f);
        set ("level", L.level); set ("pan", L.pan); set ("cutoff", L.cutoff); set ("reso", L.reso);
        set ("attack", L.attack); set ("decay", L.decay); set ("sustain", L.sustain); set ("release", L.release);
        set ("oct", (float) L.octave); set ("semi", (float) L.semi); set ("fine", (float) L.fine);
        progs.add (L.program);
    }
    params->setProperty ("reverb", c.reverb);
    params->setProperty ("chorus", c.chorus);
    params->setProperty ("vibrato", c.vibrato);
    params->setProperty ("glide", c.glide);
    params->setProperty ("mono", c.mono ? 1.0f : 0.0f);
    params->setProperty ("layers", (float) c.layerCount);

    auto* obj = new juce::DynamicObject();
    obj->setProperty ("params", juce::var (params));
    obj->setProperty ("layerProg", progs);
    return juce::var (obj);
}

juce::String safeFileName (const juce::String& name)
{
    return juce::File::createLegalFileName (name.trim()).substring (0, 60);
}
} // namespace

PresetLibrary::PresetLibrary()
{
    for (const auto& c : combos)
        factory.add ({ c.name, true, comboData (c) });
    refresh();
}

juce::File PresetLibrary::userFolder()
{
   #if JUCE_MAC
    return juce::File::getSpecialLocation (juce::File::userHomeDirectory).getChildFile ("Library/Audio/Presets/Opalite Audio/gemi");
   #else
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("Opalite Audio/gemi/Combos");
   #endif
}

void PresetLibrary::refresh()
{
    entries = factory;
    auto files = userFolder().findChildFiles (juce::File::findFiles, false, juce::String ("*") + fileExtension);
    files.sort();
    for (const auto& f : files)
    {
        const auto data = juce::JSON::parse (f);
        if (data.isObject() && data["params"].isObject())
            entries.add ({ data.getProperty ("name", f.getFileNameWithoutExtension()).toString(), false, data });
    }
}

int PresetLibrary::indexOf (const juce::String& name) const
{
    for (int i = 0; i < entries.size(); ++i)
        if (entries.getReference (i).name == name)
            return i;
    return -1;
}

bool PresetLibrary::save (const juce::String& name, const juce::var& data)
{
    const auto fileName = safeFileName (name);
    if (fileName.isEmpty())
        return false;

    auto folder = userFolder();
    if (! folder.createDirectory())
        return false;

    if (auto* obj = data.getDynamicObject())
        obj->setProperty ("name", name.trim());

    const auto ok = folder.getChildFile (fileName + fileExtension).replaceWithText (juce::JSON::toString (data));
    refresh();
    return ok;
}

} // namespace gemi
