#pragma once
#include "PluginProcessor.h"
#include "UI/MainComponent.h"

class AudioLoopStationEditor : public juce::AudioProcessorEditor
{
public:
    explicit AudioLoopStationEditor(AudioLoopStationAudioProcessor&);

    void resized() override { mainComponent.setBounds(getLocalBounds()); }

    // These have be declared here so the PluginEditor cpp file can define them
    void openButtonClicked();
    void playButtonClicked();
    void stopButtonClicked();
    void loopButtonChanged();
    void updateTransportButtons();

private:
    AudioLoopStationAudioProcessor& audioProcessor;

    // Our PluginEditor cpp file uses them!
    juce::TextButton openButton;
    juce::ToggleButton loopingToggle;

    MainComponent mainComponent;

    // Fixed the name here to match the class name
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioLoopStationEditor)
};