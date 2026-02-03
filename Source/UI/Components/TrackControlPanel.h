#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../../Utils/TrackConfig.h"

//==============================================================================
class TrackControlPanel final : public juce::Component
{
public:
    explicit TrackControlPanel(juce::AudioProcessorValueTreeState& apvts);
    ~TrackControlPanel() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct TrackControls
    {
        juce::Label trackLabel;
        juce::Slider volumeSlider;
        juce::Slider panSlider;
        juce::TextButton recordArmButton;
        juce::TextButton muteButton;
        juce::TextButton soloButton;
        juce::TextButton clearButton;
    };

    std::array<TrackControls, TrackConfig::MAX_TRACKS> trackControls;

    // APVTS attachments
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> volumeAttachments;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> panAttachments;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> muteAttachments;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> soloAttachments;

    juce::AudioProcessorValueTreeState& apvts;

    void setupTrackControls();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrackControlPanel)
};