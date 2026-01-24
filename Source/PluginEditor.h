#pragma once

#include "PluginProcessor.h"
#include "UI/MainComponent.h"

//==============================================================================
class AudioLoopStationEditor final : public juce::AudioProcessorEditor,
                                     public juce::Timer
{
public:
    explicit AudioLoopStationEditor (AudioLoopStationAudioProcessor&);
    ~AudioLoopStationEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

private:
    AudioLoopStationAudioProcessor& audioProcessor;

    MainComponent mainComponent;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioPluginAudioProcessorEditor)
};
