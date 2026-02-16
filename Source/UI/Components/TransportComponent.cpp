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
    juce::FlexBox flexBox;
    flexBox.flexDirection = juce::FlexBox::Direction::row;
    flexBox.flexWrap = juce::FlexBox::Wrap::noWrap;
    flexBox.alignContent = juce::FlexBox::AlignContent::stretch;
    flexBox.alignItems = juce::FlexBox::AlignItems::stretch;
    flexBox.justifyContent = juce::FlexBox::JustifyContent::spaceAround;

    constexpr float margin = 10.0f;
    constexpr float padding = 2.0f;

    flexBox.items.add(juce::FlexItem(recordButton).withFlex(1.0f).withMargin(padding));
    flexBox.items.add(juce::FlexItem(playButton).withFlex(1.0f).withMargin(padding));
    flexBox.items.add(juce::FlexItem(stopButton).withFlex(1.0f).withMargin(padding));
    flexBox.items.add(juce::FlexItem(undoButton).withFlex(1.0f).withMargin(padding));

    flexBox.performLayout(getLocalBounds().reduced(margin));
}