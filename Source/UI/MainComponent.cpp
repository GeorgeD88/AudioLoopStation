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
    juce::FlexBox flexBox;
    flexBox.flexDirection = juce::FlexBox::Direction::column;
    flexBox.flexWrap = juce::FlexBox::Wrap::noWrap;
    flexBox.alignContent = juce::FlexBox::AlignContent::stretch;
    flexBox.alignItems = juce::FlexBox::AlignItems::stretch;
    flexBox.justifyContent = juce::FlexBox::JustifyContent::flexStart;

    // Track controls take up remaining space (flex: 1)
    flexBox.items.add(juce::FlexItem(trackControlPanel)
                          .withMinHeight(100.0f)
                          .withFlex(1.0f));

    // Transport controls at bottom (fixed height)
    flexBox.items.add(juce::FlexItem(transportComponent)
                          .withHeight(80.0f)
                          .withMinHeight(60.0f)
                          .withMaxHeight(100.0f));

    flexBox.performLayout(getLocalBounds());
}