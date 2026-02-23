#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>

//==============================================================================
/** A single track's control strip - uses FlexBox for responsive layout */
class TrackStripComponent final : public juce::Component
{
public:
    TrackStripComponent(int trackIndex, juce::AudioProcessorValueTreeState& apvts);
    ~TrackStripComponent() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    static juce::Colour getTrackColour(int index);

    void setupControls();

    int trackIndex;
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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrackStripComponent)
};
