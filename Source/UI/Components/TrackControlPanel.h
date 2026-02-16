#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "TrackStripComponent.h"
#include "../../Utils/TrackConfig.h"

//==============================================================================
class TrackControlPanel final : public juce::Component
{
public:
    explicit TrackControlPanel(juce::AudioProcessorValueTreeState& apvts);
    ~TrackControlPanel() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    std::array<std::unique_ptr<TrackStripComponent>, TrackConfig::MAX_TRACKS> trackStrips;
    juce::AudioProcessorValueTreeState& apvts;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrackControlPanel)
};
