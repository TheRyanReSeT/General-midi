#include "Drums.h"

#include <array>

namespace gm
{
namespace
{

struct D
{
    DrumRecipe r;

    explicit D (const char* name) { r.name = name; }

    D& tone (float level, float startHz, float endHz, float sweep, float decay, Wave w = Wave::Sine)
    {
        r.toneLevel = level; r.toneStartHz = startHz; r.toneEndHz = endHz;
        r.toneSweep = sweep; r.toneDecay = decay; r.toneWave = w;
        return *this;
    }
    D& tone2 (float level, float hz, float decay, Wave w = Wave::Sine)
    {
        r.tone2Level = level; r.tone2Hz = hz; r.tone2Decay = decay; r.tone2Wave = w;
        return *this;
    }
    D& vib (float hz, float depth) { r.toneVibHz = hz; r.toneVibDepth = depth; return *this; }
    D& noise (float level, float decay, FilterType f = FilterType::Off, float cutoff = 5000.0f, float q = 0.707f)
    {
        r.noiseLevel = level; r.noiseDecay = decay; r.noiseFilter = f; r.noiseCutoff = cutoff; r.noiseQ = q;
        return *this;
    }
    D& metal (float level, float decay, float scale = 1.0f, float highPass = 7000.0f)
    {
        r.metalLevel = level; r.metalDecay = decay; r.metalScale = scale; r.metalHighPass = highPass;
        return *this;
    }
    D& attack (float a) { r.attack = a; return *this; }
    D& bursts (int n, float spacing) { r.bursts = n; r.burstSpacing = spacing; return *this; }
    D& am (float rate) { r.amRate = rate; return *this; }
    D& pan (float p) { r.pan = p; return *this; }
    D& group (int g) { r.exclusiveGroup = g; return *this; }
    D& gain (float g) { r.gain = g; return *this; }

    operator DrumRecipe() const { return r; }
};

constexpr auto LP = FilterType::LowPass;
constexpr auto HP = FilterType::HighPass;
constexpr auto BP = FilterType::BandPass;

std::array<DrumRecipe, 128> buildKit()
{
    std::array<DrumRecipe, 128> k {};
    auto set = [&] (int note, const DrumRecipe& r) { k[(size_t) note] = r; };

    auto tom = [] (const char* name, float hz, float pan)
    {
        return D (name).tone (1.0f, hz * 1.8f, hz, 0.06f, 0.45f).noise (0.12f, 0.06f, LP, 3000).pan (pan).gain (0.9f);
    };

    set (35, D ("Acoustic Bass Drum").tone (1.0f, 140, 45, 0.05f, 0.45f).noise (0.15f, 0.01f, LP, 1500).gain (1.2f));
    set (36, D ("Bass Drum 1").tone (1.0f, 180, 52, 0.035f, 0.35f).noise (0.2f, 0.006f, LP, 3000).gain (1.2f));
    set (37, D ("Side Stick").tone (0.6f, 1800, 1700, 0.01f, 0.025f, Wave::Triangle).noise (0.5f, 0.02f, BP, 2500, 2.0f).gain (0.8f));
    set (38, D ("Acoustic Snare").tone (0.6f, 260, 185, 0.03f, 0.1f).noise (0.9f, 0.2f, HP, 1200).gain (0.85f));
    set (39, D ("Hand Clap").noise (1.0f, 0.12f, BP, 1300, 1.4f).bursts (4, 0.011f).pan (-0.15f).gain (1.0f));
    set (40, D ("Electric Snare").tone (0.5f, 320, 220, 0.02f, 0.07f).noise (1.0f, 0.14f, HP, 2000).gain (0.85f));
    set (41, tom ("Low Floor Tom", 73, -0.6f));
    set (42, D ("Closed Hi-Hat").metal (1.0f, 0.045f).noise (0.3f, 0.035f, HP, 8000).pan (0.35f).group (1).gain (0.55f));
    set (43, tom ("High Floor Tom", 87, -0.45f));
    set (44, D ("Pedal Hi-Hat").metal (0.8f, 0.03f, 1.0f, 6000).noise (0.2f, 0.03f, HP, 6000).pan (0.35f).group (1).gain (0.5f));
    set (45, tom ("Low Tom", 103, -0.25f));
    set (46, D ("Open Hi-Hat").metal (1.0f, 0.45f).noise (0.3f, 0.35f, HP, 8000).pan (0.35f).group (1).gain (0.5f));
    set (47, tom ("Low-Mid Tom", 123, -0.05f));
    set (48, tom ("Hi-Mid Tom", 146, 0.15f));
    set (49, D ("Crash Cymbal 1").metal (0.8f, 1.8f, 1.15f, 5000).noise (0.6f, 1.5f, HP, 5000).pan (-0.4f).gain (0.5f));
    set (50, tom ("High Tom", 174, 0.35f));
    set (51, D ("Ride Cymbal 1").metal (0.6f, 1.5f, 1.6f, 7000).noise (0.15f, 0.8f, HP, 9000).tone2 (0.15f, 3200, 1.0f).pan (0.4f).gain (0.45f));
    set (52, D ("Chinese Cymbal").metal (0.9f, 1.2f, 0.85f, 3500).noise (0.6f, 1.0f, BP, 4000, 0.8f).pan (-0.45f).gain (0.5f));
    set (53, D ("Ride Bell").tone2 (0.5f, 2100, 1.0f).metal (0.4f, 0.8f, 1.8f, 5000).tone (0.3f, 3050, 3050, 0.01f, 0.6f).pan (0.4f).gain (0.45f));
    set (54, D ("Tambourine").metal (0.6f, 0.2f, 2.2f, 6000).noise (0.6f, 0.18f, HP, 7000).am (24.0f).pan (0.55f).gain (0.5f));
    set (55, D ("Splash Cymbal").metal (0.8f, 0.7f, 1.4f, 6000).noise (0.6f, 0.6f, HP, 6000).pan (-0.3f).gain (0.5f));
    set (56, D ("Cowbell").tone (0.7f, 540, 540, 0.01f, 0.25f, Wave::Square).tone2 (0.7f, 800, 0.2f, Wave::Square)
                .noise (0.0f, 0.01f).pan (0.3f).gain (0.35f));
    set (57, D ("Crash Cymbal 2").metal (0.8f, 2.0f, 1.25f, 4500).noise (0.6f, 1.8f, HP, 4500).pan (0.45f).gain (0.5f));
    set (58, D ("Vibraslap").metal (0.5f, 0.9f, 1.5f, 2000).noise (0.4f, 0.9f, BP, 3000, 2.0f).am (28.0f).pan (0.4f).gain (0.6f));
    set (59, D ("Ride Cymbal 2").metal (0.6f, 1.6f, 1.5f, 6000).noise (0.2f, 1.0f, HP, 8000).tone2 (0.12f, 2900, 1.0f).pan (0.45f).gain (0.45f));
    set (60, D ("Hi Bongo").tone (1.0f, 520, 400, 0.02f, 0.12f).noise (0.15f, 0.015f, BP, 3000).pan (0.5f).gain (0.7f));
    set (61, D ("Low Bongo").tone (1.0f, 380, 290, 0.02f, 0.16f).noise (0.15f, 0.015f, BP, 2500).pan (0.4f).gain (0.7f));
    set (62, D ("Mute Hi Conga").tone (1.0f, 360, 310, 0.01f, 0.07f).noise (0.2f, 0.01f, BP, 2500).pan (-0.4f).gain (0.7f));
    set (63, D ("Open Hi Conga").tone (1.0f, 340, 310, 0.02f, 0.3f).noise (0.15f, 0.015f, BP, 2500).pan (-0.4f).gain (0.7f));
    set (64, D ("Low Conga").tone (1.0f, 240, 210, 0.02f, 0.35f).noise (0.15f, 0.015f, BP, 2000).pan (-0.3f).gain (0.7f));
    set (65, D ("High Timbale").tone (0.8f, 640, 560, 0.01f, 0.25f).tone2 (0.3f, 1420, 0.15f).noise (0.4f, 0.1f, HP, 3000).pan (0.15f).gain (0.6f));
    set (66, D ("Low Timbale").tone (0.8f, 440, 380, 0.01f, 0.3f).tone2 (0.3f, 960, 0.18f).noise (0.4f, 0.12f, HP, 2500).pan (0.05f).gain (0.6f));
    set (67, D ("High Agogo").tone (0.8f, 1050, 1050, 0.01f, 0.4f).tone2 (0.4f, 2900, 0.15f).pan (0.3f).gain (0.4f));
    set (68, D ("Low Agogo").tone (0.8f, 700, 700, 0.01f, 0.45f).tone2 (0.4f, 1930, 0.15f).pan (0.3f).gain (0.4f));
    set (69, D ("Cabasa").noise (1.0f, 0.08f, HP, 6000).attack (0.012f).pan (-0.5f).gain (0.4f));
    set (70, D ("Maracas").noise (1.0f, 0.05f, HP, 8000).attack (0.008f).pan (-0.55f).gain (0.4f));
    set (71, D ("Short Whistle").tone (0.6f, 2400, 2500, 0.02f, 0.12f).vib (35.0f, 0.03f).attack (0.01f).pan (0.55f).group (2).gain (0.4f));
    set (72, D ("Long Whistle").tone (0.6f, 2300, 2300, 0.02f, 0.45f).vib (35.0f, 0.03f).attack (0.02f).pan (0.55f).group (2).gain (0.4f));
    set (73, D ("Short Guiro").noise (1.0f, 0.1f, BP, 3200, 2.5f).am (45.0f).attack (0.01f).pan (0.55f).group (3).gain (0.7f));
    set (74, D ("Long Guiro").noise (1.0f, 0.35f, BP, 3200, 2.5f).am (32.0f).attack (0.02f).pan (0.55f).group (3).gain (0.7f));
    set (75, D ("Claves").tone (1.0f, 2500, 2500, 0.01f, 0.05f).pan (0.25f).gain (0.6f));
    set (76, D ("Hi Wood Block").tone (1.0f, 1000, 900, 0.005f, 0.06f).tone2 (0.25f, 2600, 0.03f).pan (0.5f).gain (0.6f));
    set (77, D ("Low Wood Block").tone (1.0f, 720, 650, 0.005f, 0.08f).tone2 (0.25f, 1900, 0.03f).pan (0.5f).gain (0.6f));
    set (78, D ("Mute Cuica").tone (0.8f, 900, 500, 0.06f, 0.1f).noise (0.1f, 0.05f, BP, 1000).pan (-0.5f).group (4).gain (0.6f));
    set (79, D ("Open Cuica").tone (0.8f, 350, 650, 0.12f, 0.35f).noise (0.1f, 0.1f, BP, 700).pan (-0.5f).group (4).gain (0.6f));
    set (80, D ("Mute Triangle").tone (0.6f, 4200, 4200, 0.01f, 0.08f).tone2 (0.4f, 6900, 0.06f).pan (-0.6f).group (5).gain (0.35f));
    set (81, D ("Open Triangle").tone (0.6f, 4200, 4200, 0.01f, 1.4f).tone2 (0.4f, 6900, 1.0f).pan (-0.6f).group (5).gain (0.35f));

    return k;
}

} // namespace

const DrumRecipe* getDrum (int note)
{
    static const std::array<DrumRecipe, 128> kit = buildKit();
    if (note < 0 || note > 127) return nullptr;
    const auto& r = kit[(size_t) note];
    return r.name != nullptr ? &r : nullptr;
}

} // namespace gm
