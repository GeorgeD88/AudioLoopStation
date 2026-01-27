#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

//==============================================================================
class TransportComponent final : public juce::Component
{
public:
    TransportComponent();
    ~TransportComponent() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void setupButtons();

    juce::TextButton recordButton { "RECORD" };
    juce::TextButton playButton { "PLAY" };
    juce::TextButton stopButton { "STOP" };
    juce::TextButton undoButton { "UNDO" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TransportComponent)
};