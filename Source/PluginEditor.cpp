#include "PluginEditor.h"

#include <GemiUI.h>

namespace
{
const juce::Identifier eventId ("gemi");

juce::String mimeTypeFor (const juce::String& fileName)
{
    const auto ext = fileName.fromLastOccurrenceOf (".", false, false).toLowerCase();
    if (ext == "html")  return "text/html";
    if (ext == "js")    return "text/javascript";
    if (ext == "css")   return "text/css";
    if (ext == "jpg")   return "image/jpeg";
    if (ext == "png")   return "image/png";
    if (ext == "webp")  return "image/webp";
    if (ext == "woff2") return "font/woff2";
    return "application/octet-stream";
}

// Settings shared by every gemi instance on this computer (not saved with projects).
juce::PropertiesFile& globalSettings()
{
    static juce::PropertiesFile file ([]
    {
        juce::PropertiesFile::Options o;
        o.applicationName = "gemi";
        o.folderName = "Opalite Audio/gemi";
        o.filenameSuffix = ".settings";
        o.osxLibrarySubFolder = "Application Support";
        return o;
    }());
    return file;
}
}

GemiEditor::GemiEditor (GemiProcessor& p)
    : AudioProcessorEditor (p),
      processor (p),
      browser (juce::WebBrowserComponent::Options {}
                   .withNativeIntegrationEnabled()
                   .withKeepPageLoadedWhenBrowserIsHidden()
                   .withResourceProvider ([] (const juce::String& url) { return getResource (url); })
                   .withEventListener (eventId, [this] (const juce::var& message) { handleMessage (message); }))
{
    addAndMakeVisible (browser);

    currentSize = juce::jlimit (0.75, 1.5, (double) processor.getUiState()["size"]);
    setSize (juce::roundToInt (baseWidth * currentSize), juce::roundToInt (baseHeight * currentSize));

    // A combo was loaded or saved, or the host restored a session (possibly off the message thread).
    processor.onPresetsChanged = [safe = juce::Component::SafePointer<GemiEditor> (this)]
    {
        juce::MessageManager::callAsync ([safe]
        {
            if (safe == nullptr || ! safe->pageReady) return;
            safe->sendPresets();
            safe->sendAllParams();
            safe->sendState();
        });
    };

    browser.goToURL (juce::WebBrowserComponent::getResourceProviderRoot());
    startTimerHz (30);
}

GemiEditor::~GemiEditor()
{
    processor.onPresetsChanged = nullptr;
    for (const auto& id : gestures)
        if (auto* param = processor.parameters.getParameter (id))
            param->endChangeGesture();
}

void GemiEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff241d62));
}

void GemiEditor::resized()
{
    browser.setBounds (getLocalBounds());
}

void GemiEditor::applySize (double size)
{
    size = juce::jlimit (0.75, 1.5, size);
    if (std::abs (size - currentSize) < 0.001) return;
    currentSize = size;
    setSize (juce::roundToInt (baseWidth * size), juce::roundToInt (baseHeight * size));
}

//==============================================================================
// Pages and pictures come from the plugin binary: "/" is index.html, everything else is looked up by file name.
std::optional<juce::WebBrowserComponent::Resource> GemiEditor::getResource (const juce::String& url)
{
    auto path = url.upToFirstOccurrenceOf ("?", false, false);
    auto name = path == "/" || path.isEmpty() ? juce::String ("index.html") : path.fromLastOccurrenceOf ("/", false, false);

    for (int i = 0; i < GemiUI::namedResourceListSize; ++i)
    {
        if (name != GemiUI::originalFilenames[i]) continue;
        int size = 0;
        if (const auto* data = GemiUI::getNamedResource (GemiUI::namedResourceList[i], size))
        {
            const auto* bytes = reinterpret_cast<const std::byte*> (data);
            return juce::WebBrowserComponent::Resource { std::vector<std::byte> (bytes, bytes + size), mimeTypeFor (name) };
        }
    }
    return std::nullopt;
}

void GemiEditor::send (const juce::String& type, juce::DynamicObject* payload)
{
    juce::var message (payload != nullptr ? payload : new juce::DynamicObject());
    message.getDynamicObject()->setProperty ("type", type);
    browser.emitEventIfBrowserIsVisible (eventId, message);
}

void GemiEditor::sendState()
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("state", processor.getUiState());
    send ("state", o);
}

void GemiEditor::addPresetList (juce::DynamicObject& o) const
{
    juce::Array<juce::var> list;
    for (const auto& entry : processor.presets.all())
    {
        auto* e = new juce::DynamicObject();
        e->setProperty ("name", entry.name);
        e->setProperty ("kind", entry.factory ? "factory" : "user");
        list.add (juce::var (e));
    }
    o.setProperty ("presets", list);
    o.setProperty ("presetIndex", processor.getPresetIndex());
    o.setProperty ("presetName", processor.getPresetName());
}

void GemiEditor::sendPresets()
{
    auto* o = new juce::DynamicObject();
    addPresetList (*o);
    send ("presets", o);
}

void GemiEditor::sendAllParams()
{
    auto* values = new juce::DynamicObject();
    for (const auto& spec : gemi::paramSpecs())
    {
        const float v = processor.parameters.getRawParameterValue (spec.id)->load();
        values->setProperty (spec.id, v);
        lastSent[spec.id] = v;
    }
    auto* o = new juce::DynamicObject();
    o->setProperty ("values", juce::var (values));
    send ("params", o);
}

void GemiEditor::sendInit()
{
    auto* values = new juce::DynamicObject();
    for (const auto& spec : gemi::paramSpecs())
    {
        const float v = processor.parameters.getRawParameterValue (spec.id)->load();
        values->setProperty (spec.id, v);
        lastSent[spec.id] = v;
    }
    auto* o = new juce::DynamicObject();
    o->setProperty ("params", juce::var (values));
    o->setProperty ("state", processor.getUiState());
    o->setProperty ("version", GEMI_DISPLAY_VERSION);
    o->setProperty ("tourSeen", globalSettings().getBoolValue ("tourSeen", false));   // the first-open tutorial
    o->setProperty ("ownEnv", processor.getOwnEnvelopes());
    lastProgs = juce::JSON::toString (processor.getUiState()["layerProg"]);
    addPresetList (*o);
    send ("init", o);
    pageReady = true;
}

//==============================================================================
void GemiEditor::handleMessage (const juce::var& m)
{
    const auto type = m["type"].toString();

    if (type == "ready")
    {
        sendInit();
    }
    else if (type == "params")
    {
        if (auto* values = m["values"].getDynamicObject())
        {
            for (const auto& prop : values->getProperties())
            {
                const auto id = prop.name.toString();
                auto* param = processor.parameters.getParameter (id);
                if (param == nullptr) continue;
                const float v = (float) prop.value;
                lastSent[id] = v;
                const bool inGesture = gestures.count (id) > 0;
                if (! inGesture) param->beginChangeGesture();
                param->setValueNotifyingHost (param->convertTo0to1 (v));
                if (! inGesture) param->endChangeGesture();
            }
        }
    }
    else if (type == "gesture")
    {
        const bool on = (bool) m["on"];
        if (auto* ids = m["ids"].getArray())
        {
            for (const auto& idVar : *ids)
            {
                const auto id = idVar.toString();
                auto* param = processor.parameters.getParameter (id);
                if (param == nullptr) continue;
                if (on && gestures.insert (id).second) param->beginChangeGesture();
                if (! on && gestures.erase (id) > 0) param->endChangeGesture();
            }
        }
    }
    else if (type == "state")
    {
        processor.setUiState (m["state"]);
        applySize ((double) m["state"]["size"]);
    }
    else if (type == "note")
    {
        processor.queueUiNote ((int) m["note"], (int) m.getProperty ("velocity", 100), (bool) m["on"]);
    }
    else if (type == "preset")
    {
        if (m.hasProperty ("step")) processor.stepPreset ((int) m["step"]);
        else                        processor.loadPreset ((int) m["index"]);
    }
    else if (type == "savePreset")
    {
        processor.savePreset (m["name"].toString());
    }
    else if (type == "tourSeen")
    {
        globalSettings().setValue ("tourSeen", true);
        globalSettings().saveIfNeeded();
    }
    else if (type == "gmReset")
    {
        processor.requestGMReset();
    }
    else if (type == "panic")
    {
        processor.requestPanic();
    }
}

// 30 times a second: parameter changes from the host (automation, generic editors), notes for the
// keyboard, channel activity lights and the voice count.
void GemiEditor::timerCallback()
{
    if (! pageReady) return;

    auto* changed = new juce::DynamicObject();
    bool any = false;
    for (const auto& spec : gemi::paramSpecs())
    {
        const float v = processor.parameters.getRawParameterValue (spec.id)->load();
        auto it = lastSent.find (spec.id);
        if (it == lastSent.end() || std::abs (it->second - v) > 1.0e-6f)
        {
            lastSent[spec.id] = v;
            changed->setProperty (spec.id, v);
            any = true;
        }
    }
    if (any)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("values", juce::var (changed));
        send ("params", o);
    }
    else
    {
        delete changed;
    }

    if (processor.takeProgramsChanged())
        sendState();

    // A layer changed sound (from the page, a combo or the host): tell the page that sound's own envelope.
    const auto progs = juce::JSON::toString (processor.getUiState()["layerProg"]);
    if (progs != lastProgs)
    {
        lastProgs = progs;
        auto* o = new juce::DynamicObject();
        o->setProperty ("envs", processor.getOwnEnvelopes());
        send ("ownEnv", o);
    }

    juce::Array<int> held;
    processor.getHeldNotes (held);
    juce::String heldKey;
    for (int n : held) heldKey << n << ',';
    const auto active = processor.getActiveChannels();
    const int voices = processor.getActiveVoices();

    if (heldKey != lastHeld || active != lastActive || voices != lastVoices)
    {
        lastHeld = heldKey; lastActive = active; lastVoices = voices;
        auto* o = new juce::DynamicObject();
        juce::Array<juce::var> notes, act;
        for (int n : held) notes.add (n);
        for (int ch = 0; ch < GemiProcessor::numChannels; ++ch) act.add ((active & (1u << ch)) != 0);
        o->setProperty ("held", notes);
        o->setProperty ("act", act);
        o->setProperty ("voices", voices);
        send ("tick", o);
    }
}
