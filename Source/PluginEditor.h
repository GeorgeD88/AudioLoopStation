#pragma once

#include "PluginProcessor.h"

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

    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioLoopStationEditor)
};