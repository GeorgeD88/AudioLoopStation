#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
AudioLoopStationEditor::AudioLoopStationEditor (AudioLoopStationAudioProcessor& p)
        : AudioProcessorEditor (p), audioProcessor (p), mainComponent(p)
{
    addAndMakeVisible(mainComponent);

    // Make sure that before the constructor has finished, you've set the
    // editor's size to whatever you need it to be.
    setSize (1200, 900);
}

AudioLoopStationEditor::~AudioLoopStationEditor()
{
}

void AudioLoopStationEditor::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
}

void AudioLoopStationEditor::playButtonClicked()
{
    // TODO: Connect this to the AudioProcessor to start playback
    // Example: audioProcessor.startPlayback();

    // Optional: Update UI state (e.g. change button text/color)
    updateTransportButtons();
}

void AudioLoopStationEditor::stopButtonClicked()
{
    audioProcessor.stopPlayback();
    updateTransportButtons();
}

void AudioLoopStationEditor::loopButtonChanged()
{
    if (audioProcessor.getReaderSource() != nullptr)
    {
        audioProcessor.getReaderSource()->setLooping(loopingToggle.getToggleState());
    }
}

void AudioLoopStationEditor::resized()
{
    // Where we will lay out our buttons
    openButton.setBounds (10, 10, 100, 30);
    mainComponent.setBounds (getLocalBounds());
}

void AudioLoopStationEditor::timerCallback()
{
    // Logic
}

void AudioLoopStationEditor::openButtonClicked()
{
    // Logic for opening a file will go here
}

void AudioLoopStationEditor::updateTransportButtons()
{
    // Logic for changing button colors/text will go here
}