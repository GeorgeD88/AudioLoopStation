#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../../Audio/LoopManager.h"

//==============================================================================
/** A single track's control strip - uses FlexBox for responsive layout.
    Background/header colours reflect engine state: recording (red), muted (gray),
    playing (green accent), stopped with loop (orange), armed (orange border). */
class TrackStripComponent final : public juce::Component
{
public:
    TrackStripComponent(int trackIndex,
                        juce::AudioProcessorValueTreeState& apvts,
                        LoopManager& loopManagerRef);
    ~TrackStripComponent() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    /** Keep ARM toggle aligned with LoopTrack (e.g. after armAllTracks from transport). */
    void syncArmButtonWithEngine();

private:
    static juce::Colour getTrackColour(int index);
    juce::String trackParameterPrefix() const;

    void setupControls();

    int trackIndex;
    juce::AudioProcessorValueTreeState& apvts;
    LoopManager& loopManager;

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
