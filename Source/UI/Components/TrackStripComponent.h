#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../../PluginProcessor.h"

//==============================================================================
/** A single track's control strip - uses FlexBox for responsive layout */
class TrackStripComponent final : public juce::Component,
                            private juce::Timer
{
public:
    TrackStripComponent(int trackIndex,
                        AudioLoopStationAudioProcessor& processor,
                        juce::AudioProcessorValueTreeState& apvts);
    ~TrackStripComponent() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    static juce::Colour getTrackColour(int index);
    void setupControls();
    void timerCallback() override;
    void updateButtonColours();

    int trackIndex;
    AudioLoopStationAudioProcessor& audioProcessor;             // Store reference
    juce::AudioProcessorValueTreeState& apvts;

    juce::Label trackLabel;
    juce::Slider volumeSlider;
    juce::Slider panSlider;
    juce::TextButton recordArmButton;
    juce::TextButton muteButton;
    juce::TextButton soloButton;
    juce::TextButton clearButton;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> volumeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> panAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> muteAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> soloAttachment;

    bool blinkState = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrackStripComponent)
};
