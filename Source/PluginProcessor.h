#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "juce_audio_formats/juce_audio_formats.h"
#include "juce_audio_devices/juce_audio_devices.h"
#include "Utils/TrackConfig.h"

//==============================================================================
class AudioLoopStationAudioProcessor final : public juce::AudioProcessor
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

    // Transport control methods
    void loadFile(const juce::File& audioFile);
    void startPlayback();
    void stopPlayback();
    bool isPlaying() const { return transportSource.isPlaying(); }
    double getCurrentPosition() const { return transportSource.getCurrentPosition(); }

    juce::AudioTransportSource& getTransportSource() { return transportSource; }
    juce::AudioFormatReaderSource* getReaderSource() { return readerSource.get(); }
    juce::AudioFormatManager& getFormatManager() { return formatManager; }

    // APVTS access
    juce::AudioProcessorValueTreeState& getApvts() { return apvts; }

    // Parameter creation
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    juce::AudioFormatManager formatManager;
    std::unique_ptr<juce::AudioFormatReaderSource> readerSource;
    juce::AudioTransportSource transportSource;

    // APVTS for track parameters
    juce::AudioProcessorValueTreeState apvts;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioLoopStationAudioProcessor)
};