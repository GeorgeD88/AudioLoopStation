#include "TrackControlPanel.h"

//==============================================================================
TrackControlPanel::TrackControlPanel()
{
    setupTrackControls();
}

TrackControlPanel::~TrackControlPanel()
{
}

//==============================================================================
void TrackControlPanel::setupTrackControls()
{
    for (int i = 0; i < TrackConfig::MAX_TRACKS; ++i)
    {
        auto& controls = trackControls[i];

        // Track label
        controls.trackLabel.setText("Track " + juce::String(i + 1), juce::dontSendNotification);
        controls.trackLabel.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(controls.trackLabel);

        // Volume slider
        controls.volumeSlider.setSliderStyle(juce::Slider::LinearVertical);
        controls.volumeSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        controls.volumeSlider.setRange(0.0, 1.0, 0.01);
        controls.volumeSlider.setValue(0.8);
        addAndMakeVisible(controls.volumeSlider);

        // Pan slider
        controls.panSlider.setSliderStyle(juce::Slider::LinearHorizontal);
        controls.panSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        controls.panSlider.setRange(-1.0, 1.0, 0.01);
        controls.panSlider.setValue(0.0);
        addAndMakeVisible(controls.panSlider);

        // Record arm button
        controls.recordArmButton.setButtonText("ARM");
        controls.recordArmButton.setColour(juce::TextButton::buttonColourId, juce::Colours::grey);
        controls.recordArmButton.setColour(juce::TextButton::buttonOnColourId, juce::Colours::red);
        controls.recordArmButton.setClickingTogglesState(true);
        addAndMakeVisible(controls.recordArmButton);

        // Clear button
        controls.clearButton.setButtonText("CLEAR");
        controls.clearButton.setColour(juce::TextButton::buttonColourId, juce::Colours::darkgrey);
        addAndMakeVisible(controls.clearButton);
    }
}

void TrackControlPanel::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    g.setColour (juce::Colours::white);
    g.drawRect (getLocalBounds(), 1);

    // Draw track separators
    auto bounds = getLocalBounds();
    auto trackWidth = bounds.getWidth() / TrackConfig::MAX_TRACKS;

    g.setColour(juce::Colours::darkgrey);
    for (int i = 1; i < TrackConfig::MAX_TRACKS; ++i)
    {
        auto x = i * trackWidth;
        g.drawLine(static_cast<float>(x), 0.0f, static_cast<float>(x), static_cast<float>(bounds.getHeight()), 1.0f);
    }
}

void TrackControlPanel::resized()
{
    auto bounds = getLocalBounds().reduced(10);
    auto trackWidth = bounds.getWidth() / TrackConfig::MAX_TRACKS;

    for (int i = 0; i < TrackConfig::MAX_TRACKS; ++i)
    {
        auto trackBounds = bounds.removeFromLeft(trackWidth).reduced(5);

        auto& controls = trackControls[i];

        // Track label at the top
        auto labelHeight = 20;
        controls.trackLabel.setBounds(trackBounds.removeFromTop(labelHeight));

        // Volume slider takes most of the height
        auto volumeHeight = trackBounds.getHeight() * 0.6f;
        controls.volumeSlider.setBounds(trackBounds.removeFromTop(static_cast<int>(volumeHeight)).reduced(2));

        // Pan slider below volume
        auto panHeight = 30;
        controls.panSlider.setBounds(trackBounds.removeFromTop(panHeight).reduced(2));

        // Buttons at the bottom
        auto buttonHeight = 25;
        auto buttonBounds = trackBounds.removeFromTop(buttonHeight);

        controls.recordArmButton.setBounds(buttonBounds.removeFromLeft(buttonBounds.getWidth() / 2).reduced(1));
        controls.clearButton.setBounds(buttonBounds.reduced(1));
    }
}