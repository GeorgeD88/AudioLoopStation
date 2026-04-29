#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "TrackStripComponent.h"
#include "../../Utils/Config.h"
#include "../../PluginProcessor.h"

//==============================================================================
class TrackControlPanel final : public juce::Component
{
public:
    explicit TrackControlPanel(AudioLoopStationAudioProcessor& processor,
                               juce::AudioProcessorValueTreeState& apvts);
    ~TrackControlPanel() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    AudioLoopStationAudioProcessor& audioProcessor;
    juce::AudioProcessorValueTreeState& apvts;
    std::array<std::unique_ptr<TrackStripComponent>, Config::NUM_TRACKS> trackStrips;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrackControlPanel)
};
