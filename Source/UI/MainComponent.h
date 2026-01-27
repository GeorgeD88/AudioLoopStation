#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "Components/TransportComponent.h"
#include "Components/TrackControlPanel.h"

//==============================================================================
class MainComponent final : public juce::Component
{
public:
    MainComponent();
    ~MainComponent() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    TransportComponent transportComponent;
    TrackControlPanel trackControlPanel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};