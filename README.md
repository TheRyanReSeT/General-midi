# General MIDI Synth

A VST3 / AU / Standalone instrument with the complete General MIDI sound set built in.
No SoundFont files to find, install or point at: the samples are compiled into the plugin.

Every one of the classic 128 GM instruments is there, from Acoustic Grand Piano to
Seashore, Helicopter, Applause and Gunshot, plus 13 drum kits (Standard, Room, Power,
Electronic, 808/909, Jazz, Brush, Orchestral, SFX and more) and 146 GS variation tones.

![screenshot](docs/screenshot.png)

## What it does

- **16 MIDI channels**, multitimbral, channel 10 is drums, exactly like a hardware GM module.
  Drop it on a track, feed it a `.mid` file, and it plays back like a 90s sound card.
- **Program change, bank select (GS variations), pitch bend, mod wheel, volume, pan,
  expression, sustain, reverb/chorus send (CC91/CC93), RPNs and GM/GS SysEx resets** all work.
- **Reverb and chorus** built in, with master Reverb / Chorus / Volume knobs.
- **Per-channel instrument picker** in the UI, so you can choose sounds without sending
  program changes. The choices are saved with your project.
- **On-screen keyboard**: click a channel number to play that channel.
- **GM Reset** and **All Sound Off** buttons.

## How it works

| Piece | What it is | License |
| --- | --- | --- |
| [JUCE 8](https://juce.com) | Plugin framework (VST3, AU, Standalone) | AGPLv3 / commercial |
| [FluidSynth 2.5](https://www.fluidsynth.org) | The SoundFont engine (the reference SF2 player, with reverb, chorus and full modulator support) | LGPL-2.1 |
| [GeneralUser GS 2.0.3](https://www.schristiancollins.com/generaluser) by S. Christian Collins | The 32 MB GM/GS sound bank, embedded in the binary | See `Resources/GeneralUser-GS-LICENSE.txt` |

FluidSynth is built as a self-contained static library (no glib, no audio drivers), and
the SoundFont is fed to it straight from memory.

Source layout:

- `Source/GMSynth.*` : the sound module (FluidSynth + embedded SoundFont, MIDI handling). No JUCE dependency.
- `Source/PluginProcessor.*` : the plugin: sample-accurate MIDI, parameters, state saving.
- `Source/PluginEditor.*` : the UI.
- `tools/gm_render.cpp` : renders a tour of all 128 instruments and every drum kit to a WAV and checks levels.

## Building

Requirements: CMake 3.22+, a C++17 compiler, Git. JUCE and FluidSynth are downloaded
automatically by CMake on the first configure.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
```

macOS (to get the AU, a universal binary is recommended):

```sh
cmake -S . -B build -G Xcode -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
cmake --build build --config Release
```

Linux needs the usual JUCE packages:

```sh
sudo apt install libasound2-dev libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev \
  libxext-dev libfreetype-dev libfontconfig1-dev
```

The first build takes a few minutes, mostly compiling the 32 MB SoundFont into the binary.

Built plugins land in `build/GeneralMidiSynth_artefacts/Release/`:

- `VST3/General MIDI Synth.vst3`: copy to `~/Library/Audio/Plug-Ins/VST3` (macOS),
  `C:\Program Files\Common Files\VST3` (Windows) or `~/.vst3` (Linux)
- `AU/General MIDI Synth.component` (macOS): copy to `~/Library/Audio/Plug-Ins/Components`
- `Standalone/`: a standalone app that takes MIDI from any connected controller or virtual port

You don't have to build locally: the GitHub Actions workflow (`.github/workflows/build.yml`)
builds macOS, Windows and Linux on every push and uploads the plugins as downloadable artifacts.
On macOS, plugins downloaded from CI are quarantined; clear that with
`xattr -dr com.apple.quarantine "General MIDI Synth.component"` (and the same for the .vst3).

## Testing the engine

```sh
cmake --build build --target gm_render
./build/gm_render tour.wav          # every instrument, then every drum kit
./build/gm_render gunshot.wav 127 127   # just program 128 (Gunshot), 0-based arguments
```

It prints the peak and RMS level of each instrument and exits non-zero if any is silent,
clipping or produces NaNs.

## Distributing it

For personal use, build it and go. If you want to give it to other people:

- **JUCE** is AGPLv3 unless you hold a JUCE commercial license.
- **FluidSynth** is LGPL-2.1 and is statically linked, so recipients must be able to relink
  it (for example by publishing this source, which also satisfies the AGPL).
- **GeneralUser GS** may be bundled in software; keep its license file with it.
