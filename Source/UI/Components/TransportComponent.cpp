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
void TransportComponent::setAudioProcessor(AudioLoopStationAudioProcessor *processor) {
    audioProcessor = processor;
    updateButtonStates();
}

void TransportComponent::setupButtons()
{
    recordButton.setColour(juce::TextButton::buttonColourId, juce::Colours::red);
    recordButton.setColour(juce::TextButton::buttonOnColourId, juce::Colours::darkred);
    recordButton.onClick = [this]()
    {
        DBG("[UI] GLOBAL RECORD");
        if (!audioProcessor) return;

        // Find the armed track
        for (size_t i = 0; i < TrackConfig::MAX_TRACKS; ++i) {
            auto* track = audioProcessor->getLoopManager().getTrack(i);
            if (track && track->isArmed()) {
                audioProcessor->startRecordingOnArmedTrack();  // New method
                return;
            }
        }

        // If no track armed, find first empty track and arm+record it
        for (size_t i = 0; i < TrackConfig::MAX_TRACKS; ++i) {
            auto* track = audioProcessor->getLoopManager().getTrack(i);
            if (track && track->getState() == LoopTrack::State::Empty) {
                audioProcessor->requestTrackRecording(static_cast<int>(i));  // Arm it
                audioProcessor->startRecordingOnArmedTrack();  // Then start
                return;
            }
        }
    };
    addAndMakeVisible(recordButton);

    playButton.setColour(juce::TextButton::buttonColourId, juce::Colours::green);
    playButton.setColour(juce::TextButton::buttonOnColourId, juce::Colours::darkgreen);
    playButton.onClick = [this]()
    {
        DBG("[UI] PLAY");
        if (audioProcessor) {
            audioProcessor->startPlayback();
            updateButtonStates();
        }
    };
    addAndMakeVisible(playButton);

    stopButton.setColour(juce::TextButton::buttonColourId, juce::Colours::orange);
    stopButton.setColour(juce::TextButton::buttonOnColourId, juce::Colours::darkorange);
    stopButton.onClick = [this]()
    {
        DBG("[UI] STOP");
        if (audioProcessor) {
            // Stops playback and recording and sets isPlaying to false
            audioProcessor->stopAll();
            updateButtonStates();
        }
    };
    addAndMakeVisible(stopButton);

    undoButton.setColour(juce::TextButton::buttonColourId, juce::Colours::blue);
    undoButton.setColour(juce::TextButton::buttonOnColourId, juce::Colours::darkblue);
    undoButton.onClick = [this]()
    {
        DBG("[UI] UNDO (not implemented)");
    };
    addAndMakeVisible(undoButton);
}

void TransportComponent::updateButtonStates() {
    if (!audioProcessor) return;

    // Update play button state based on transport
    bool isPlaying = audioProcessor->isPlaying();
    playButton.setToggleState(isPlaying, juce::dontSendNotification);
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

    // Changed these from float (10.0f) to int (10)
    constexpr int margin = 10;
    constexpr float padding = 2.0f;

    flexBox.items.add(juce::FlexItem(recordButton).withFlex(1.0f).withMargin(padding));
    flexBox.items.add(juce::FlexItem(playButton).withFlex(1.0f).withMargin(padding));
    flexBox.items.add(juce::FlexItem(stopButton).withFlex(1.0f).withMargin(padding));
    flexBox.items.add(juce::FlexItem(undoButton).withFlex(1.0f).withMargin(padding));

    // By making 'margin' an int, getLocalBounds().reduced(margin) stays an integer calculation.
    flexBox.performLayout(getLocalBounds().reduced(margin).toFloat());
}