#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../../PluginProcessor.h"

//==============================================================================
class TransportComponent final : public juce::Component
{
public:
    TransportComponent();
    ~TransportComponent() override;

    //==============================================================================
    void setAudioProcessor(AudioLoopStationAudioProcessor* processor);
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void setupButtons();
    void updateButtonStates();

    AudioLoopStationAudioProcessor* audioProcessor = nullptr;

    juce::TextButton recordButton { "RECORD" };
    juce::TextButton playButton { "PLAY" };
    juce::TextButton stopButton { "STOP" };
    juce::TextButton undoButton { "UNDO" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TransportComponent)
};