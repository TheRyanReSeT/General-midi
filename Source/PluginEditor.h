#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>

#include "PluginProcessor.h"

#include <set>

// gemi's window: the painted UI in UI/index.html, shown in a web view and served from the plugin
// binary. The page and the plugin talk through "gemi" events (see the bridge at the end of the page).
class GemiEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    static constexpr int baseWidth = 960, baseHeight = 640;

    explicit GemiEditor (GemiProcessor&);
    ~GemiEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void handleMessage (const juce::var& message);
    void send (const juce::String& type, juce::DynamicObject* payload);
    void sendInit();
    void sendPresets();
    void sendState();
    void addPresetList (juce::DynamicObject&) const;
    void sendAllParams();
    void applySize (double size);

    static std::optional<juce::WebBrowserComponent::Resource> getResource (const juce::String& url);

    GemiProcessor& processor;
    juce::WebBrowserComponent browser;
    bool pageReady = false;
    double currentSize = 1.0;

    std::map<juce::String, float> lastSent;   // parameter values the page already has
    std::set<juce::String> gestures;          // parameters the page is dragging
    juce::String lastHeld, lastProgs;
    uint32_t lastActive = 0xffffffff;
    int lastVoices = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GemiEditor)
};
