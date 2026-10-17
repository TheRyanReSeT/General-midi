cmake_minimum_required(VERSION 3.22)

project(GMSynth VERSION 1.0.0 LANGUAGES C CXX)

#==============================================================================
# Product identity. Set these to your own brand before you ship.
#   - Manufacturer code: 4 characters, at least one upper-case letter, unique to you.
#   - Plugin code: 4 characters, unique among your products, at least one upper-case letter.
# Changing either code after release makes hosts treat it as a different plugin.
set(GM_PRODUCT_NAME      "gemi"                     CACHE STRING "Plugin name shown in hosts")
set(GM_COMPANY_NAME      "Opalite Audio"            CACHE STRING "Manufacturer name shown in hosts")
set(GM_COMPANY_WEBSITE   ""                         CACHE STRING "Manufacturer website")
set(GM_BUNDLE_ID         "com.opaliteaudio.gemi"    CACHE STRING "Reverse-DNS bundle identifier")
set(GM_MANUFACTURER_CODE "Opal"                     CACHE STRING "4-character manufacturer code")
set(GM_PLUGIN_CODE       "Gemi"                     CACHE STRING "4-character plugin code")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
endif()

include(FetchContent)

#==============================================================================
# JUCE (plugin framework: VST3 / AU)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        9.0.4
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(JUCE)

#==============================================================================
# FluidSynth (SoundFont engine, LGPL-2.1).
#
# It is built as a SHARED library and shipped inside each plugin bundle, so the
# closed-source plugin only links to it dynamically and users can swap in their
# own build of FluidSynth, as the LGPL requires. No glib, no audio/MIDI drivers.
set(BUILD_SHARED_LIBS ON CACHE BOOL "" FORCE)
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

if(CMAKE_GENERATOR STREQUAL "Xcode")
    target_sources(libfluidsynth PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/cmake/xcode_placeholder.c")
endif()

if(APPLE)
    # Let each bundle find its own copy in Contents/Frameworks.
    set_target_properties(libfluidsynth PROPERTIES
        INSTALL_NAME_DIR "@rpath"
        BUILD_WITH_INSTALL_NAME_DIR TRUE)
endif()

# Copies the FluidSynth library and the license notices next to (or inside) a built
# binary, and makes the binary look for the library there.
function(gm_bundle_fluidsynth target)
    if(APPLE)
        get_target_property(isBundle ${target} BUNDLE)
        if(isBundle)
            set(dest "$<TARGET_BUNDLE_CONTENT_DIR:${target}>/Frameworks")
            set(licenseDest "$<TARGET_BUNDLE_CONTENT_DIR:${target}>/Resources")
            set(rpath "@loader_path/../Frameworks")
        else()
            set(dest "$<TARGET_FILE_DIR:${target}>")
            set(licenseDest "${dest}")
            set(rpath "@loader_path")
        endif()
        set(lib "$<TARGET_SONAME_FILE:libfluidsynth>")
    elseif(WIN32)
        set(dest "$<TARGET_FILE_DIR:${target}>")
        set(licenseDest "${dest}")
        set(lib "$<TARGET_FILE:libfluidsynth>")
    else()
        set(dest "$<TARGET_FILE_DIR:${target}>")
        set(licenseDest "${dest}")
        set(lib "$<TARGET_SONAME_FILE:libfluidsynth>")
        set(rpath "$ORIGIN")
    endif()

    if(NOT APPLE AND target MATCHES "_VST3$")
        set(licenseDest "${dest}/../Resources")
    endif()

    if(NOT WIN32)
        # Only ever look in the shipped location, never in the build tree.
        set_target_properties(${target} PROPERTIES
            BUILD_WITH_INSTALL_RPATH TRUE
            INSTALL_RPATH "${rpath}")
    endif()

    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${dest}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${lib}" "${dest}"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/Licenses" "${licenseDest}/Licenses"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/THIRD_PARTY_NOTICES.md" "${licenseDest}"
        VERBATIM)
endfunction()

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

if(MSVC)
    # Plugins are loaded from arbitrary folders, where Windows would not look for
    # the FluidSynth DLL. Delay-load it and load it by full path from the plugin's
    # own folder before first use (see GMSynth.cpp).
    target_compile_definitions(gm_engine PRIVATE
        GM_FLUIDSYNTH_DLL=L"$<TARGET_FILE_NAME:libfluidsynth>")
endif()

#==============================================================================
# The plugin
set(GM_FORMATS VST3)
if(APPLE)
    list(APPEND GM_FORMATS AU)
endif()

juce_add_plugin(GMSynth
    PRODUCT_NAME "${GM_PRODUCT_NAME}"
    COMPANY_NAME "${GM_COMPANY_NAME}"
    COMPANY_WEBSITE "${GM_COMPANY_WEBSITE}"
    BUNDLE_ID "${GM_BUNDLE_ID}"
    PLUGIN_MANUFACTURER_CODE ${GM_MANUFACTURER_CODE}
    PLUGIN_CODE ${GM_PLUGIN_CODE}
    FORMATS ${GM_FORMATS}
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Instrument Synth
    AU_MAIN_TYPE kAudioUnitType_MusicDevice)

target_sources(GMSynth PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h)

target_compile_definitions(GMSynth PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(GMSynth
    PRIVATE
        gm_engine
        GMSoundFontData
        juce::juce_audio_utils
        juce::juce_audio_processors
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

foreach(format ${GM_FORMATS})
    gm_bundle_fluidsynth(GMSynth_${format})
    if(MSVC)
        target_link_options(GMSynth_${format} PRIVATE "/DELAYLOAD:$<TARGET_FILE_NAME:libfluidsynth>")
        target_link_libraries(GMSynth_${format} PRIVATE delayimp)
    endif()
endforeach()

#==============================================================================
# Command-line renderer: plays a demo of every instrument and drum kit to a WAV
# file, for checking the engine without a DAW.
option(GM_BUILD_TOOLS "Build the gm_render test tool" ON)
if(GM_BUILD_TOOLS)
    add_executable(gm_render tools/gm_render.cpp)
    target_link_libraries(gm_render PRIVATE gm_engine)
    target_compile_definitions(gm_render PRIVATE
        GM_SOUNDFONT_PATH="${CMAKE_CURRENT_SOURCE_DIR}/Resources/GeneralUser-GS.sf2")
    gm_bundle_fluidsynth(gm_render)
    if(MSVC)
        target_link_options(gm_render PRIVATE "/DELAYLOAD:$<TARGET_FILE_NAME:libfluidsynth>")
        target_link_libraries(gm_render PRIVATE delayimp)
    endif()
endif()
