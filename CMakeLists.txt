cmake_minimum_required(VERSION 3.22)

project(GeneralMidiSynth VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
endif()

include(FetchContent)

#==============================================================================
# JUCE (plugin framework: VST3 / AU / Standalone)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.9
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(JUCE)

#==============================================================================
# FluidSynth (SoundFont engine), built as a self-contained static library:
# no glib, no audio/MIDI drivers, no external dependencies.
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(osal "cpp11" CACHE STRING "" FORCE)
foreach(opt
        enable-alsa enable-aufile enable-dbus enable-ipv6 enable-jack enable-ladspa
        enable-libinstpatch enable-libsndfile enable-midishare enable-network enable-oss
        enable-dsound enable-wasapi enable-waveout enable-winmidi enable-sdl3
        enable-pulseaudio enable-pipewire enable-readline enable-threads enable-openmp
        enable-systemd enable-coreaudio enable-coremidi enable-framework enable-portaudio
        enable-native-dls)
    set(${opt} OFF CACHE BOOL "" FORCE)
endforeach()

FetchContent_Declare(fluidsynth
    GIT_REPOSITORY https://github.com/FluidSynth/fluidsynth.git
    GIT_TAG        v2.5.7
    GIT_SHALLOW    TRUE
    GIT_SUBMODULES gcem
    EXCLUDE_FROM_ALL)
FetchContent_MakeAvailable(fluidsynth)

#==============================================================================
# The General MIDI SoundFont, compiled into the plugin binary.
juce_add_binary_data(GMSoundFontData
    NAMESPACE GMData
    SOURCES Resources/GeneralUser-GS.sf2)

#==============================================================================
# Engine: FluidSynth wrapper with no JUCE dependency (also used by the test renderer).
add_library(gm_engine STATIC
    Source/GMSynth.cpp
    Source/GMSynth.h
    Source/GMNames.h)
target_include_directories(gm_engine PUBLIC Source)
target_link_libraries(gm_engine PUBLIC libfluidsynth)

#==============================================================================
# The plugin
set(GM_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND GM_FORMATS AU)
endif()

juce_add_plugin(GeneralMidiSynth
    COMPANY_NAME "General MIDI"
    PRODUCT_NAME "General MIDI Synth"
    BUNDLE_ID com.generalmidi.synth
    PLUGIN_MANUFACTURER_CODE GmSy
    PLUGIN_CODE GmS1
    FORMATS ${GM_FORMATS}
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Instrument Synth
    AU_MAIN_TYPE kAudioUnitType_MusicDevice)

target_sources(GeneralMidiSynth PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h)

target_compile_definitions(GeneralMidiSynth PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(GeneralMidiSynth
    PRIVATE
        gm_engine
        GMSoundFontData
        juce::juce_audio_utils
        juce::juce_audio_processors
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

#==============================================================================
# Command-line renderer: plays a demo of every instrument and drum kit to a WAV
# file, for checking the engine without a DAW.
option(GM_BUILD_TOOLS "Build the gm_render test tool" ON)
if(GM_BUILD_TOOLS)
    add_executable(gm_render tools/gm_render.cpp)
    target_link_libraries(gm_render PRIVATE gm_engine)
    target_compile_definitions(gm_render PRIVATE
        GM_SOUNDFONT_PATH="${CMAKE_CURRENT_SOURCE_DIR}/Resources/GeneralUser-GS.sf2")
endif()
