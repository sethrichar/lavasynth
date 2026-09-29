cmake_minimum_required(VERSION 3.22)

project(Rotor VERSION 0.1.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(ROTOR_BUILD_TESTS "Build DSP unit tests" ON)
option(ROTOR_BUILD_DEVTOOLS "Build the offline render/CPU check tool" OFF)

include(FetchContent)

# JUCE pinned to a release tag (recorded in CHANGELOG.md). Never depend on a local JUCE path.
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.15
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

# Plain C++ DSP library — no JUCE dependency, so it can be unit-tested on its own.
add_library(rotor_dsp INTERFACE)
target_include_directories(rotor_dsp INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src)

set(ROTOR_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND ROTOR_FORMATS AU)
endif()

juce_add_plugin(Rotor
    COMPANY_NAME "Lavasynth"
    BUNDLE_ID com.lavasynth.rotor
    PLUGIN_MANUFACTURER_CODE Lvsy
    PLUGIN_CODE Rtr1
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Instrument Synth
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    FORMATS ${ROTOR_FORMATS}
    PRODUCT_NAME "Rotor")

target_sources(Rotor PRIVATE
    src/plugin/PluginProcessor.cpp
    src/plugin/Parameters.cpp
    src/plugin/PresetManager.cpp)

target_compile_definitions(Rotor PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(Rotor
    PRIVATE
        rotor_dsp
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

if(ROTOR_BUILD_DEVTOOLS)
    juce_add_console_app(RotorRender PRODUCT_NAME "RotorRender")
    target_sources(RotorRender PRIVATE
        devtools/RenderCheck.cpp
        src/plugin/PluginProcessor.cpp
        src/plugin/Parameters.cpp
        src/plugin/PresetManager.cpp)
    target_include_directories(RotorRender PRIVATE src)
    target_compile_definitions(RotorRender PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JucePlugin_Name="Rotor")
    target_link_libraries(RotorRender PRIVATE
        rotor_dsp
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_recommended_config_flags)
endif()

if(ROTOR_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
