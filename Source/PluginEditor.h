#pragma once

#include "PluginProcessor.h"

//==============================================================================
class AudioLoopStationEditor final : public juce::AudioProcessorEditor,
                                     public juce::Timer
{
public:
    explicit AudioLoopStationEditor (AudioLoopStationProcessor&);
    ~AudioLoopStationEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

private:
    AudioLoopStationProcessor& audioProcessor;

    juce::TextButton openButton;
    juce::TextButton playButton;
    juce::TextButton stopButton;
    juce::ToggleButton loopingToggle;
    juce::Label currentPositionLabel;

    void openButtonClicked();
    void playButtonClicked();
    void stopButtonClicked();
    void loopButtonChanged();
    void updateTransportButtons();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioLoopStationEditor)
};