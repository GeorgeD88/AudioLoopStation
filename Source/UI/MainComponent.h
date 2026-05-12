#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../PluginProcessor.h"
#include "Components/TransportComponent.h"
#include "Components/TrackControlPanel.h"
#include "Components/WaveformDisplayComponent.h"
#include "Components/VUMeterComponent.h"
#include "CustomLookAndFeel.h"
#include "Components/TrackComponent.h"

//==============================================================================
class MainComponent final : public juce::Component
{
public:
    explicit MainComponent(AudioLoopStationAudioProcessor& processor);
    ~MainComponent() override;

    void setWaveformFile(const juce::File& file);
    void setWaveformPlaybackPosition(double position);

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    AudioLoopStationAudioProcessor& audioProcessor;
    std::vector<std::unique_ptr<TrackComponent>> trackComponents;
    // WaveformDisplayComponent waveformDisplay;
    // VUMeterComponent vuMeter;
    // TransportComponent transportComponent;
    // TrackControlPanel trackControlPanel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};