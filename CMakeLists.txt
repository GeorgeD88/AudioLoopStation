cmake_minimum_required(VERSION 3.24)

# Change to your project name
project(AudioLoopStation VERSION 0.0.1)


set(CMAKE_CXX_STANDARD 17)
set(CMAKE_XCODE_GENERATE_SCHEME OFF)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# We're going to use CPM as our package manager to bring in JUCE
# Check to see if we have CPM installed already.  Bring it in if we don't.
set(CPM_DOWNLOAD_VERSION 0.34.0)
set(CPM_DOWNLOAD_LOCATION "${CMAKE_BINARY_DIR}/cmake/CPM_${CPM_DOWNLOAD_VERSION}.cmake")

if (NOT EXISTS ${CPM_DOWNLOAD_LOCATION})
    message(STATUS "Downloading CPM.cmake to ${CPM_DOWNLOAD_LOCATION}")
    file(DOWNLOAD https://github.com/cpm-cmake/CPM.cmake/releases/download/v${CPM_DOWNLOAD_VERSION}/CPM.cmake ${CPM_DOWNLOAD_LOCATION})
endif ()

include(${CPM_DOWNLOAD_LOCATION})

# Bring in JUCE locally
CPMAddPackage(
        NAME juce
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG origin/master
)

# Make sure you include any new source files here
set(SourceFiles
        Source/PluginEditor.cpp
        Source/PluginEditor.h
        Source/PluginProcessor.cpp
        Source/PluginProcessor.h

        # Audio - Seperated audio data here (PluginProcessor Related)
        Source/Audio/CircularBuffer.h
        Source/Audio/CircularBuffer.cpp
        Source/Audio/AudioEngine.cpp
        Source/Audio/AudioEngine.h
        Source/Audio/LoopManager.cpp
        Source/Audio/LoopManager.h
        Source/Audio/LoopTrack.cpp
        Source/Audio/LoopManager.h
        Source/Audio/MixerEngine.cpp
        Source/Audio/MixerEngine.h

        # UI - separate graphics data here (PluginEditor related)
        Source/UI/MainComponent.cpp
        Source/UI/MainComponent.h
        # UI - Subcomponents
        Source/UI/Components/TransportComponent.cpp
        Source/UI/Components/TransportComponent.h
        Source/UI/Components/TrackStripComponent.cpp
        Source/UI/Components/TrackStripComponent.h
        Source/UI/Components/TrackControlPanel.cpp
        Source/UI/Components/TrackControlPanel.h

        # Utiities - helpers, configs, and constants in /Utils
        Source/Utils/TrackConfig.h

)

# Change these to your own preferences
juce_add_plugin(${PROJECT_NAME}
        COMPANY_NAME CS461_F25
        IS_SYNTH TRUE
        NEEDS_MIDI_INPUT TRUE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        JUCE_VST3_CAN_REPLACE_VST2 FALSE
        COPY_PLUGIN_AFTER_BUILD TRUE
        PLUGIN_MANUFACTURER_CODE Tap1
        PLUGIN_CODE Reg0
        FORMATS VST3 AU Standalone
        PRODUCT_NAME "AudioLoopStation"
)

# How we want our SourceFiles to appear in our IDE
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SourceFiles})

# Make the SourceFiles buildable
target_sources(${PROJECT_NAME} PRIVATE ${SourceFiles})

# These are some toggleable options from the JUCE CMake API
target_compile_definitions(${PROJECT_NAME}
        PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

# JUCE libraries to bring into our project
target_link_libraries(${PROJECT_NAME}
        PUBLIC
        juce::juce_analytics
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_core
        juce::juce_data_structures
        juce::juce_graphics
        juce::juce_gui_basics
        juce::juce_gui_extra
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# tiny console test runner (separate from the plugin)
juce_add_console_app(AudioLoopStation_Tests
        PRODUCT_NAME "AudioLoopStation_Tests"
)

target_sources(AudioLoopStation_Tests
        PRIVATE
        Source/Tests/TestRunner.cpp
        Source/Tests/MixerTests.cpp
        Source/Audio/MixerEngine.cpp
        Source/Audio/MixerEngine.h
        Source/Utils/TrackConfig.h
)

target_link_libraries(AudioLoopStation_Tests
        PRIVATE
        juce::juce_core
        juce::juce_events
        juce::juce_audio_basics
        juce::juce_audio_processors
)
