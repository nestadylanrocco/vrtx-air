cmake_minimum_required(VERSION 3.22)
project(VRTX_AIR VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG master
    GIT_SHALLOW ON)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(VRTXAir
    COMPANY_NAME "VRTX"
    PLUGIN_MANUFACTURER_CODE Vrtx
    PLUGIN_CODE Vair
    FORMATS VST3
    PRODUCT_NAME "VRTX AIR"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx EQ)

juce_add_binary_data(VRTXAirFonts SOURCES
    Resources/Fonts/InterDisplayBold.otf
    Resources/Fonts/InterDisplayLight.otf
    Resources/Fonts/InterMedium.otf
    Resources/Fonts/InterSemiBold.otf)

target_sources(VRTXAir PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(VRTXAir PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(VRTXAir
    PRIVATE
        VRTXAirFonts
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
