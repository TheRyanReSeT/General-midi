# gemi by Opalite Audio

A VST3 / AU instrument with the complete General MIDI sound set built in.
No SoundFont files to find, install or point at: the samples are compiled into the plugin.

Every one of the classic 128 GM instruments is there, from Acoustic Grand Piano to
Seashore, Helicopter, Applause and Gunshot, plus 13 drum kits (Standard, Room, Power,
Electronic, 808/909, Jazz, Brush, Orchestral, SFX and more) and 146 GS variation tones.

The product name, company, bundle ID and plugin codes are set at the top of
`CMakeLists.txt` (see [Before you sell it](#before-you-sell-it)).

![screenshot](docs/screenshot.png)

## What it does

gemi opens in **Synth** mode; the switch in the header flips to **GM Player**.

**Synth**

- Starts with **one layer**; the tab on the right edge slides out into a plus that adds up to
  **three**, all played together from one key.
  MIDI on any channel plays them. Each layer can be switched off or removed.
- Each layer is any of the 128 GM sounds: step through them with the arrows, or open the full list,
  with a tag to jump to each family. Every sound starts exactly as designed: its own envelope
  (shown on the envelope display) and its own filter (cutoff open at 20k, resonance 0 %). Touch an
  ADSR value to set your own; "Sound's own" goes back.
- Per layer: **level, pan, cutoff, resonance**, an **ADSR envelope** (drag the points or
  the A/D/S/R pills), and **octave, semitone and fine** tuning. The filter and envelope replace each
  sound's own, so the values shown are the values you hear.
- **MPE**: channel 1 is the master channel; channels 2 to 16 each play one note with its own pitch
  bend (48 semitones), pressure (opens the filter, swells the level) and slide (CC74, sweeps the
  filter).
- Global **reverb, chorus, vibrato** (the mod wheel adds to it), **glide** and **volume**, and
  **Poly / Mono**.
- **Combos**: eleven factory combos, plus your own. Save writes them to
  `~/Library/Audio/Presets/Opalite Audio/gemi` (macOS).
- Every knob, switch and stepper is a host parameter: automatable, and saved with the project.
  Knobs snap to a notch at their default; double-click resets, Shift-drag skips the notch,
  Cmd/Ctrl-drag is fine control.

**GM Player**

- **16 MIDI channels**, multitimbral, channel 10 is drums, like a hardware GM module. Route a
  `.mid` file's parts to their channels in your DAW and pick each channel's sound (or let the
  song's program changes pick it). Instruments are colour coded by GM family.
- Per-channel **mute and solo**, activity lights, **Follow keyboard** (a controller on channel 1
  plays the selected channel), **GM Reset** and **All Sound Off**. Drum kits on channel 10.
- Program change, bank select (GS variations), pitch bend, mod wheel, volume, pan, expression,
  sustain, reverb/chorus sends (CC91/CC93), RPNs and GM/GS SysEx resets all work.
- **Multi-output**: besides the main stereo output, each channel has its own stereo output
  ("Ch 1" to "Ch 16", with its own reverb and chorus). Everything plays on the main output until
  **Outputs** is switched to *One per channel*; then each channel plays on its own output wherever
  the host has that output in use.

**Everywhere**: a playable on-screen keyboard that also lights up with incoming notes; light
(Cloud Meadow) and dark (Starfield Dusk) themes; window sizes 75 to 150 %; Reduce motion; About;
a short tutorial on first open (again any time from the gemi menu).

## How it works

| Piece | What it is | License |
| --- | --- | --- |
| [JUCE 9](https://juce.com) | Plugin framework (VST3, AU) | AGPLv3 or JUCE commercial licence |
| VST3 SDK (bundled with JUCE) | VST3 plugin interface | MIT |
| [FluidSynth 2.5.7](https://www.fluidsynth.org) | The SoundFont engine: the reference SF2 player, with reverb, chorus and full modulator support | LGPL-2.1 |
| [GeneralUser GS 2.0.3](https://www.schristiancollins.com/generaluser) by S. Christian Collins | The 32 MB GM/GS sound bank, embedded in the plugin binary, so gemi sounds the same on every computer | Free for commercial use, may be bundled in software |

FluidSynth is built without glib or audio drivers, as a **shared library shipped inside
each plugin bundle**. Linking it dynamically is what lets you sell the plugin as closed
source while meeting the LGPL. The SoundFont is fed to it straight from memory.

The window is a web page (`UI/index.html`, with its paintings in `UI/img` and fonts in `UI/fonts`)
compiled into the plugin and shown in the system web view (WebKit on macOS). It talks to the
plugin through JUCE's web view events; see the bridge at the end of the page and
`Source/PluginEditor.cpp`. Open `UI/index.html` in a browser to see it without the plugin.

Source layout:

- `Source/GMSynth.*`: the sound module (FluidSynth + embedded SoundFont, MIDI handling, per-voice
  filter and envelope overrides). No JUCE dependency.
- `Source/Parameters.h`: parameter IDs, defaults and the knob-to-sound formulas.
- `Source/PluginProcessor.*`: Synth layers and GM Player routing, combos, state saving.
- `Source/Presets.*`: factory combos and saved combos.
- `Source/PluginEditor.*`: hosts the web view and the bridge to the page.
- `UI/`: the window.
- `tools/gm_render.cpp`: renders a tour of all 128 instruments and every drum kit to a WAV and checks levels.
- `tools/gemi_selftest.cpp`: drives the real processor offline and checks synth layers, GM Player,
  combos and saved state.
- `Licenses/`, `THIRD_PARTY_NOTICES.md`: copied into every bundle by the build.

## Building

Requirements: CMake 3.22+, a C++17 compiler, Git. JUCE and FluidSynth are downloaded
automatically by CMake on the first configure.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
```

macOS (needed for the AU; this makes a universal Intel + Apple Silicon binary):

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
cmake --build build --parallel
```

Linux needs the usual JUCE packages:

```sh
sudo apt install libasound2-dev libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev \
  libxext-dev libxi-dev libxcomposite-dev libxrender-dev libfreetype-dev libfontconfig1-dev \
  libwebkit2gtk-4.1-dev libgtk-3-dev
```

The first build takes a few minutes, mostly compiling the 32 MB SoundFont into the binary.

Built plugins land in `build/GMSynth_artefacts/Release/` (with the default name):

- `VST3/gemi.vst3`: copy to `~/Library/Audio/Plug-Ins/VST3` (macOS),
  `C:\Program Files\Common Files\VST3` (Windows) or `~/.vst3` (Linux)
- `AU/gemi.component` (macOS): copy to `~/Library/Audio/Plug-Ins/Components`

Each plugin is a self-contained bundle (about 39 MB), FluidSynth library included.

The GitHub Actions workflow (`.github/workflows/build.yml`) builds the macOS AU and VST3
on every push, runs the engine tests, and uploads the plugins as an artifact.

## Testing the engine

```sh
cmake --build build --target gm_render
./build/gm_render tour.wav               # every instrument, then every drum kit
./build/gm_render gunshot.wav 127 127    # just Gunshot (program numbers are 0-based here)
```

It prints the peak and RMS level of each instrument and exits non-zero if any is silent,
clipping or produces NaNs.

```sh
cmake --build build --target gemi_selftest
./build/gemi_selftest played.wav         # plugin behaviour checks; writes what it played
```

## Before you sell it

This is not legal advice; read each license yourself, and talk to a lawyer if the
product matters to you.

1. **Your brand.** Set in `CMakeLists.txt`: product `gemi`, company `Opalite Audio`,
   bundle ID `com.opaliteaudio.gemi`, manufacturer code `Opal`, plugin code `Gemi`.
   Add your website to `GM_COMPANY_WEBSITE` when you have one. Never change the two
   codes after release, or hosts will treat updates as a different plugin. Don't use other
   companies' trademarks in the name (for example Roland, Sound Canvas, or the General MIDI
   logo); describing it as "General MIDI compatible" is the usual approach.
2. **JUCE licence.** Selling closed-source software built with JUCE needs a JUCE
   commercial licence, unless you release your own source under the AGPLv3. JUCE offers
   tiers based on your revenue, including a free one for small businesses: check the
   current terms at https://juce.com/get-juce before you release.
3. **FluidSynth (LGPL-2.1).** Already handled by the build: it ships as a separate,
   replaceable library, and the license and notice files go in every bundle. You also need to:
   - keep `Licenses/` and `THIRD_PARTY_NOTICES.md` with every copy (installer included),
   - make the FluidSynth 2.5.7 source available (linking to the upstream release, plus
     keeping a copy of the tarball yourself, is the safe way),
   - not forbid reverse engineering for the purpose of debugging changes to FluidSynth in
     your EULA, and allow users to swap the library.
4. **GeneralUser GS.** Its license allows commercial use and bundling in software. The
   author notes that some samples came from other freely available banks and that he can't
   be 100% certain of every sample's origin, though nobody has complained since 2000. If
   that risk matters for your product, plan to replace the bank later with samples you own
   or license; the engine plays any SF2.
5. **VST3.** The SDK bundled with JUCE 9 is MIT licensed, so no Steinberg agreement is
   needed. Follow Steinberg's logo/trademark usage guidelines if you show the VST logo.
6. **Code signing.** macOS: sign the bundles (`codesign --deep` with a Developer ID
   Application certificate, after the build has copied FluidSynth in), then notarize with
   `notarytool`. This needs an Apple Developer account. Windows: sign the `.vst3` and
   `.exe` with an Authenticode certificate to avoid SmartScreen warnings.
7. **Pro Tools (optional).** AAX needs Avid's AAX SDK and PACE signing, which JUCE
   supports; add `AAX` to `GM_FORMATS` once you have them.
