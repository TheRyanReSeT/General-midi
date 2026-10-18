// Combos: synth-mode presets. Factory combos are built in; saved combos are JSON files in the user's
// preset folder. Either kind is a var of the form { params: { id: value, ... }, layerProg: [p1, p2, p3] }.
#pragma once

#include <juce_core/juce_core.h>

namespace gemi
{

struct PresetEntry
{
    juce::String name;
    bool factory = true;
    juce::var data;
};

class PresetLibrary
{
public:
    PresetLibrary();

    // Rescans the saved combos folder.
    void refresh();
    const juce::Array<PresetEntry>& all() const { return entries; }
    int indexOf (const juce::String& name) const;

    // Writes (or overwrites) a saved combo; returns false if the file could not be written.
    bool save (const juce::String& name, const juce::var& data);

    static juce::File userFolder();

private:
    juce::Array<PresetEntry> factory, entries;
};

} // namespace gemi
