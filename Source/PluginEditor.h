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

    // Keyboard shortcut handler:
    //   SPACE       → play / stop toggle
    //   1–4         → arm / disarm track N
    //   SHIFT+1–4   → mute / unmute track N
    bool keyPressed(const juce::KeyPress& key) override;

    // Grab keyboard focus when the editor is clicked so shortcuts work in hosts
    void mouseDown(const juce::MouseEvent& event) override;

    // Custom functions
    void openButtonClicked();

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