#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../PluginProcessor.h"
#include "Components/TransportComponent.h"
#include "Components/TrackControlPanel.h"
#include "../PluginProcessor.h"

//==============================================================================
class MainComponent final : public juce::Component
{
public:
    explicit MainComponent(AudioLoopStationAudioProcessor& processor);
    ~MainComponent() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    AudioLoopStationAudioProcessor& audioProcessor;
    TransportComponent transportComponent;
    TrackControlPanel trackControlPanel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};