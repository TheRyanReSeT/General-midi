# Third-party notices

This product includes the following third-party components. Full license texts are in
the `Licenses` folder, which the build copies into every plugin bundle.
Ship that folder with every copy you distribute.

## GeneralUser GS 2.0.3

The instrument and drum samples. Copyright S. Christian Collins.
https://www.schristiancollins.com/generaluser

Licensed for use in music production and in software products, private or commercial.
License: `Licenses/GeneralUser-GS.txt`.

## FluidSynth 2.5.7

The SoundFont synthesis engine. Copyright the FluidSynth authors.
https://www.fluidsynth.org

Licensed under the GNU Lesser General Public License version 2.1
(`Licenses/FluidSynth-LGPL-2.1.txt`). FluidSynth is used unmodified and is shipped as a
separate shared library inside the plugin:

| Platform | Location |
| --- | --- |
| macOS | `<plugin bundle>/Contents/Frameworks/libfluidsynth.3.dylib` |
| Windows | `<plugin>.vst3/Contents/x86_64-win/libfluidsynth-3.dll` |
| Linux | `<plugin>.vst3/Contents/x86_64-linux/libfluidsynth.so.3` |

You may replace that library with your own build of FluidSynth 2.5.x (with
`osal=cpp11`). The complete corresponding source code is available at
https://github.com/FluidSynth/fluidsynth/releases/tag/v2.5.7, and on request from the
publisher of this product for at least three years from the date of purchase.

FluidSynth uses gcem (Apache License 2.0, `Licenses/gcem-Apache-2.0.txt`) at build time.

## Gaegu and Nunito fonts

The title font, Gaegu (Copyright 2018 The Gaegu Project Authors), and the interface font,
Nunito (Copyright 2014 The Nunito Project Authors), embedded in the plugin with their character
sets reduced to Latin. SIL Open Font License 1.1 (`Licenses/OFL-Gaegu.txt`,
`Licenses/OFL-Nunito.txt`).

## VST3 SDK

Copyright Steinberg Media Technologies GmbH. MIT License (`Licenses/VST3-SDK-MIT.txt`).
VST is a registered trademark of Steinberg Media Technologies GmbH.

## JUCE 9

Used under the JUCE commercial licence (https://juce.com/legal/juce-9-licence/).
JUCE embeds further open-source components (zlib, libpng, jpeglib, FLAC, Ogg Vorbis,
HarfBuzz and others), listed with their licenses in `JUCE.spdx.json` in the JUCE source
tree. Include their notices with your product as those licenses require.
