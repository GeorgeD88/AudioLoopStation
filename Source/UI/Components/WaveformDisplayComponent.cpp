#include "WaveformDisplayComponent.h"

//==============================================================================
WaveformDisplayComponent::WaveformDisplayComponent(juce::AudioFormatManager& fmtManager)
    : formatManager(fmtManager),
      thumbnail(512, formatManager, thumbnailCache)
{
    thumbnail.addChangeListener(this);
}

WaveformDisplayComponent::~WaveformDisplayComponent()
{
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
    repaint();
}

void WaveformDisplayComponent::setPlaybackPosition(double position)
{
    if (std::abs(playbackPosition - position) > 0.001)
    {
        playbackPosition = position;
        repaint();
    }
}

void WaveformDisplayComponent::paint(juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId).darker(0.2f));

    auto bounds = getLocalBounds().reduced(2);

    if (thumbnail.getTotalLength() > 0.0)
    {
        g.setColour(juce::Colours::white.withAlpha(0.3f));
        thumbnail.drawChannels(g, bounds, 0.0, thumbnail.getTotalLength(), 1.0f);

        // Draw playhead if position is set
        if (playbackPosition >= 0.0 && playbackPosition <= 1.0)
        {
            auto playheadX = static_cast<float>(bounds.getX() + bounds.getWidth() * playbackPosition);
            g.setColour(juce::Colours::red.withAlpha(0.8f));
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
