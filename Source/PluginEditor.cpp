#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
AudioLoopStationEditor::AudioLoopStationEditor (AudioLoopStationAudioProcessor& p)
        : AudioProcessorEditor (p), audioProcessor (p)
{
    addAndMakeVisible (&openButton);
    openButton.setButtonText ("Open...");
    openButton.onClick = [this] { openButtonClicked(); };

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
    mainComponent.setBounds(getLocalBounds());
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