#include "MainComponent.h"

//==============================================================================
MainComponent::MainComponent(AudioLoopStationAudioProcessor& processor)
    : audioProcessor(processor),
     trackControlPanel(processor.getApvts())
{
    addAndMakeVisible(transportComponent);
    addAndMakeVisible(trackControlPanel);
}

MainComponent::~MainComponent()
{
}

//==============================================================================
void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
}

void MainComponent::resized()
{
    auto bounds = getLocalBounds();

    // Transport controls at the bottom
    auto transportHeight = 80;
    transportComponent.setBounds(bounds.removeFromBottom(transportHeight));

    // Track controls take up the remaining space
    trackControlPanel.setBounds(bounds);
}