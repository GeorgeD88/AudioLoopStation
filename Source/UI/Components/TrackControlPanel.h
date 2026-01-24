#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../../Utils/TrackConfig.h"

//==============================================================================
class TrackControlPanel final : public juce::Component
{
public:
    TrackControlPanel();
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
        juce::TextButton clearButton;
    };

    std::array<TrackControls, TrackConfig::MAX_TRACKS> trackControls;

    void setupTrackControls();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrackControlPanel)
};