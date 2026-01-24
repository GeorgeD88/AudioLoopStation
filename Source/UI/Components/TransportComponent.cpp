#include "TransportComponent.h"

//==============================================================================
TransportComponent::TransportComponent()
{
    setupButtons();
}

TransportComponent::~TransportComponent()
{
}

//==============================================================================
void TransportComponent::setupButtons()
{
    recordButton.setColour(juce::TextButton::buttonColourId, juce::Colours::red);
    recordButton.setColour(juce::TextButton::buttonOnColourId, juce::Colours::darkred);
    addAndMakeVisible(recordButton);

    playButton.setColour(juce::TextButton::buttonColourId, juce::Colours::green);
    playButton.setColour(juce::TextButton::buttonOnColourId, juce::Colours::darkgreen);
    addAndMakeVisible(playButton);

    stopButton.setColour(juce::TextButton::buttonColourId, juce::Colours::orange);
    stopButton.setColour(juce::TextButton::buttonOnColourId, juce::Colours::darkorange);
    addAndMakeVisible(stopButton);

    undoButton.setColour(juce::TextButton::buttonColourId, juce::Colours::blue);
    undoButton.setColour(juce::TextButton::buttonOnColourId, juce::Colours::darkblue);
    addAndMakeVisible(undoButton);
}

void TransportComponent::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    g.setColour (juce::Colours::white);
    g.drawRect (getLocalBounds(), 1);
}

void TransportComponent::resized()
{
    auto bounds = getLocalBounds().reduced(10);
    auto buttonWidth = bounds.getWidth() / 4;
    auto buttonHeight = bounds.getHeight();

    recordButton.setBounds(bounds.removeFromLeft(buttonWidth).reduced(2));
    playButton.setBounds(bounds.removeFromLeft(buttonWidth).reduced(2));
    stopButton.setBounds(bounds.removeFromLeft(buttonWidth).reduced(2));
    undoButton.setBounds(bounds.removeFromLeft(buttonWidth).reduced(2));
}