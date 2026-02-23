#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_utils/juce_audio_utils.h>

//==============================================================================
/** Displays an audio waveform using AudioThumbnail */
class WaveformDisplayComponent final : public juce::Component,
                                      public juce::ChangeListener
{
public:
    explicit WaveformDisplayComponent(juce::AudioFormatManager& formatManager);
    ~WaveformDisplayComponent() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    /** Set the audio file to display. Pass juce::File() to clear. */
    void setSource(const juce::File& audioFile);

    /** Optional: set transport position for playhead overlay (0.0 to 1.0) */
    void setPlaybackPosition(double position);

private:
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;

    juce::AudioFormatManager& formatManager;
    juce::AudioThumbnailCache thumbnailCache{4};
    juce::AudioThumbnail thumbnail{512, formatManager, thumbnailCache};

    double playbackPosition{-1.0};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WaveformDisplayComponent)
};
