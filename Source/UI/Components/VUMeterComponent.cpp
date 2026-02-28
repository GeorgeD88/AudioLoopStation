#include "VUMeterComponent.h"

//==============================================================================
VUMeterComponent::VUMeterComponent(LevelSupplier levelSupplier)
    : getLevel(std::move(levelSupplier))
{
    startTimerHz(timerHz);
}

VUMeterComponent::~VUMeterComponent()
{
    stopTimer();
}

void VUMeterComponent::timerCallback()
{
    currentLevel = getLevel();
    if (currentLevel > peakLevel)
        peakLevel = currentLevel;
    else
        peakLevel *= peakDecayPerFrame;

    repaint();
}

void VUMeterComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().reduced(2).toFloat();
    if (bounds.isEmpty())
        return;

    g.fillAll(juce::Colours::black);

    const float levelNorm = juce::jlimit(0.0f, 1.0f, currentLevel);
    const float peakNorm = juce::jlimit(0.0f, 1.0f, peakLevel);

    // Level bar (green->yellow->red)
    auto levelWidth = bounds.getWidth() * levelNorm;
    auto meterRect = juce::Rectangle<float>(bounds.getX(), bounds.getY(), levelWidth, bounds.getHeight());

    if (levelNorm <= 0.7f)
        g.setColour(juce::Colours::green.withAlpha(0.8f));
    else if (levelNorm <= 0.9f)
        g.setColour(juce::Colours::yellow.withAlpha(0.8f));
    else
        g.setColour(juce::Colours::red.withAlpha(0.8f));
    g.fillRect(meterRect);

    // Peak indicator line
    if (peakNorm > 0.001f)
    {
        auto peakX = bounds.getX() + bounds.getWidth() * peakNorm;
        g.setColour(juce::Colours::white);
        g.drawLine(peakX, bounds.getY(), peakX, bounds.getBottom(), 1.0f);
    }

    g.setColour(juce::Colours::white.withAlpha(0.3f));
    g.drawRect(getLocalBounds(), 1);
}

void VUMeterComponent::resized()
{
}
