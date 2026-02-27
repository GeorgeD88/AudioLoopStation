#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "juce_audio_formats/juce_audio_formats.h"
#include "juce_audio_devices/juce_audio_devices.h"
#include "gin/gin.h"
#include "Audio/SyncEngine.h"
#include "Audio/LoopManager.h"
#include "Audio/MixerEngine.h"
#include "Audio/LoopFileHandler.h"
#include "Utils/TrackConfig.h"

//==============================================================================
class AudioLoopStationAudioProcessor final : public juce::AudioProcessor,
                                             public juce::AudioProcessorValueTreeState::Listener
{
public:
    //==============================================================================
    AudioLoopStationAudioProcessor();
    ~AudioLoopStationAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // === Listener callback ===
    void parameterChanged(const juce::String& parameterID, float newValue) override;

    juce::AudioFormatManager& getFormatManager() { return formatManager; }

    // === Accessors for UI and components ===
    LoopManager& getLoopManager() { return loopManager; }
    MixerEngine& getMixerEngine() { return mixerEngine; }
    juce::AudioProcessorValueTreeState& getApvts() { return apvts; }

    // === Transport control methods ===
    void loadFileToTrack(const juce::File& audioFile, int trackIndex);
    void startPlayback();
    void stopPlayback();
    bool isPlaying() const { return isPlaying_; }

private:
    // === Core components ===
    SyncEngine syncEngine;                          // 1. Global timekeeper
    LoopManager loopManager;                        // 2. Manages tracks, uses the SyncEngine
    MixerEngine mixerEngine;                        // 3. Mixes tracks
    std::unique_ptr<LoopFileHandler> fileHandler;   // 4. File loading

    // === Parameter management ===
    juce::AudioProcessorValueTreeState apvts;
    juce::AudioFormatManager formatManager;

    // === State ===
    std::atomic<bool> isPlaying_ {false};

    // === Parameter layout creation ===
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioLoopStationAudioProcessor)
};