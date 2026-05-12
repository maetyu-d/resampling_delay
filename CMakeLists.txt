cmake_minimum_required(VERSION 3.22)

project(ResamplingDelay VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT DEFINED JUCE_DIR)
    set(JUCE_DIR "/Users/user/Documents/Granny/JUCE" CACHE PATH "Path to a JUCE checkout")
endif()

add_subdirectory(${JUCE_DIR} JUCE)

juce_add_plugin(ResamplingDelay
    COMPANY_NAME "matd.space"
    BUNDLE_ID "space.matd.resamplingdelay"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER_CODE MtdS
    PLUGIN_CODE RsDl
    FORMATS AU VST3 Standalone
    PRODUCT_NAME "Resampling Delay"
)

target_sources(ResamplingDelay
    PRIVATE
        Source/PluginEditor.cpp
        Source/PluginEditor.h
        Source/PluginProcessor.cpp
        Source/PluginProcessor.h
)

target_compile_definitions(ResamplingDelay
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(ResamplingDelay
    PRIVATE
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
