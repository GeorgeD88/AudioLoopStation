#pragma once
#include "PluginProcessor.h"
#include "UI/MainComponent.h"

// Inherit from juce::Timer so we can use timerCallback
class AudioLoopStationEditor : public juce::AudioProcessorEditor,
                               public juce::Timer
{
public:
    explicit AudioLoopStationEditor(AudioLoopStationAudioProcessor&);

    // Declare the Destructor (don't define it here)
    ~AudioLoopStationEditor() override;

    // Declare Paint (don't define it here)
    void paint(juce::Graphics&) override;

    // Declare Resized
    void resized() override;

    // Declare TimerCallback
    void timerCallback() override;

    // Custom functions - may be refactored
    void openButtonClicked();
    void playButtonClicked();
    void stopButtonClicked();
    void loopButtonChanged();
    void updateTransportButtons();

private:
    AudioLoopStationAudioProcessor& audioProcessor;
    CustomLookAndFeel lookAndFeel;

    juce::TextButton resetButton { "RESET" };
    juce::TextButton bounceButton { "BOUNCE" };       // NOTE: sums active tracks into Track1 NOT a "PLAY ALL" button
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> mResetAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> mBounceAttachment;

    // track components and meters will go to -> MainComponent
    // Main component here
    MainComponent mainComponent;

    // File loader when refactor is fully implemented
    juce::TextButton openButton;
    juce::ToggleButton loopingToggle;

    juce::Label bpmLabel;
    juce::Label stateLabel;
    juce::Label midiSyncLabel;
    juce::ComboBox midiSyncChannelSelector;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> mMidiSyncChannelAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioLoopStationEditor)
};