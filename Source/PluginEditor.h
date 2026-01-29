#pragma once
#include "PluginProcessor.h"
#include "UI/MainComponent.h"

class AudioLoopStationEditor : public juce::AudioProcessorEditor
{
public:
    explicit AudioLoopStationEditor(AudioLoopStationAudioProcessor&);

    void resized() override { mainComponent.setBounds(getLocalBounds()); }

private:
    AudioLoopStationAudioProcessor& audioProcessor;
    MainComponent mainComponent;
};