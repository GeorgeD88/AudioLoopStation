#include "WaveformDisplayComponent.h"

//==============================================================================
WaveformDisplayComponent::WaveformDisplayComponent(juce::AudioFormatManager& fmtManager)
    : formatManager(fmtManager),
      thumbnail(512, formatManager, thumbnailCache)
{
    thumbnail.addChangeListener(this);
    startTimerHz(playheadAnimationHz);
}

WaveformDisplayComponent::~WaveformDisplayComponent()
{
    stopTimer();
    thumbnail.removeChangeListener(this);
}

void WaveformDisplayComponent::setSource(const juce::File& audioFile)
{
    if (audioFile.existsAsFile())
    {
        thumbnail.setSource(new juce::FileInputSource(audioFile));
    }
    else
    {
        thumbnail.clear();
    }
    targetPlaybackPosition = -1.0;
    displayedPlaybackPosition = -1.0;
    repaint();
}

void WaveformDisplayComponent::setPlaybackPosition(double position)
{
    targetPlaybackPosition = juce::jlimit(0.0, 1.0, position);
}

void WaveformDisplayComponent::timerCallback()
{
    if (targetPlaybackPosition < 0.0)
        return;
    if (displayedPlaybackPosition < 0.0)
        displayedPlaybackPosition = targetPlaybackPosition;
    else
        displayedPlaybackPosition += (targetPlaybackPosition - displayedPlaybackPosition) * playheadLerpFactor;

    if (std::abs(displayedPlaybackPosition - targetPlaybackPosition) < 0.0001)
        displayedPlaybackPosition = targetPlaybackPosition;
    repaint();
}

void WaveformDisplayComponent::paint(juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId).darker(0.2f));

    auto bounds = getLocalBounds().reduced(2);

    if (thumbnail.getTotalLength() > 0.0)
    {
        g.setColour(juce::Colours::white.withAlpha(0.3f));
        thumbnail.drawChannels(g, bounds, 0.0, thumbnail.getTotalLength(), 1.0f);

        // Draw animated playhead with subtle glow
        if (displayedPlaybackPosition >= 0.0 && displayedPlaybackPosition <= 1.0)
        {
            auto playheadX = static_cast<float>(bounds.getX() + bounds.getWidth() * displayedPlaybackPosition);
            // Soft glow behind
            g.setColour(juce::Colours::red.withAlpha(0.25f));
            g.drawLine(playheadX, static_cast<float>(bounds.getY()),
                       playheadX, static_cast<float>(bounds.getBottom()), 6.0f);
            // Bright center line
            g.setColour(juce::Colours::red.withAlpha(0.95f));
            g.drawLine(playheadX, static_cast<float>(bounds.getY()),
                       playheadX, static_cast<float>(bounds.getBottom()), 2.0f);
        }
    }
    else
    {
        g.setColour(juce::Colours::white.withAlpha(0.5f));
        g.setFont(14.0f);
        g.drawFittedText("No audio loaded - Open a file to see waveform", bounds,
                         juce::Justification::centred, 2);
    }

    g.setColour(juce::Colours::white.withAlpha(0.2f));
    g.drawRect(getLocalBounds(), 1);
}

void WaveformDisplayComponent::resized()
{
}

void WaveformDisplayComponent::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    if (source == &thumbnail)
        repaint();
}
