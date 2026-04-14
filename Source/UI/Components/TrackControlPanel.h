#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "TrackStripComponent.h"
#include "../../Utils/TrackConfig.h"

class AudioLoopStationAudioProcessor;

//==============================================================================
class TrackControlPanel final : public juce::Component,
                                 private juce::Timer
{
public:
    explicit TrackControlPanel(AudioLoopStationAudioProcessor& processor);
    ~TrackControlPanel() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    std::array<std::unique_ptr<TrackStripComponent>, TrackConfig::MAX_TRACKS> trackStrips;
    juce::AudioProcessorValueTreeState& apvts;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrackControlPanel)
};
