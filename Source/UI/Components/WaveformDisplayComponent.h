#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_utils/juce_audio_utils.h>

//==============================================================================
/** Timeline component: displays audio waveform with animated playhead */
class WaveformDisplayComponent final : public juce::Component,
                                       public juce::ChangeListener,
                                       public juce::Timer
{
public:
    explicit WaveformDisplayComponent(juce::AudioFormatManager& formatManager);
    ~WaveformDisplayComponent() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    /** Set the audio file to display. Pass juce::File() to clear. */
    void setSource(const juce::File& audioFile);

    /** Set transport position for animated playhead (0.0 to 1.0) */
    void setPlaybackPosition(double position);

private:
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;
    void timerCallback() override;

    juce::AudioFormatManager& formatManager;
    juce::AudioThumbnailCache thumbnailCache{4};
    juce::AudioThumbnail thumbnail{512, formatManager, thumbnailCache};

    double targetPlaybackPosition{-1.0};
    double displayedPlaybackPosition{-1.0};
    static constexpr int playheadAnimationHz = 30;
    static constexpr float playheadLerpFactor = 0.35f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WaveformDisplayComponent)
};
