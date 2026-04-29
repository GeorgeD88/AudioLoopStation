#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
AudioLoopStationEditor::AudioLoopStationEditor (AudioLoopStationAudioProcessor& p)
        : AudioProcessorEditor (p), audioProcessor (p), mainComponent(p)
{
    openButton.setButtonText("Open...");
    openButton.onClick = [this] { openButtonClicked(); };
    addAndMakeVisible(openButton);

    addAndMakeVisible(mainComponent);

    startTimerHz(30);
    setSize (1200, 900);
}

AudioLoopStationEditor::~AudioLoopStationEditor()
{
}

void AudioLoopStationEditor::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));
}

void AudioLoopStationEditor::resized()
{
    auto bounds = getLocalBounds();
    openButton.setBounds(bounds.removeFromTop(36).reduced(8, 4));
    mainComponent.setBounds(bounds);
}
/**
 * Calculates a normalized playback position (0.0 to 1.0) and update the waveform
 * - gets global time from the sync engine
 * - finds the longest loop length across all tracks
 * - checks if there's any valid loop and if playback is active
 * - calculates normalized position
 * - updates waveform with that position
 * @note Only updates if at least one track has a valid loop and playback is active
 * @note updated 2/24/26 by Vince
 */
void AudioLoopStationEditor::timerCallback()
{
    bool isFirst = audioProcessor.isFirstLoop();
    double bpm = audioProcessor.getBpm();

    stateLabel.setText(isFirst ? "WAITING FOR FIRST LOOP" : "LOOPING",
                       juce::dontSendNotification);
    stateLabel.setColour(juce::Label::textColourId,
                         isFirst ? Colours_::dub : Colours_::play);

    if (bpm > 0)
        bpmLabel.setText(juce::String(bpm, 1) + " BPM", juce::dontSendNotification);
    else
        bpmLabel.setText("-- BPM", juce::dontSendNotification);

    repaint();
}

void AudioLoopStationEditor::openButtonClicked()
{
    juce::Component::SafePointer<AudioLoopStationEditor> safeThis(this);

    // Use the file handler's default audio/music folder
    // auto defaultFolder = LoopFileHandler::getDefaultAudioFolder();

    // Get supported extensions
    auto validExtensions = LoopFileHandler::getSupportedExtString();

    auto chooser = std::make_shared<juce::FileChooser>("Select an audio file...",
                                                       juce::File(),
                                                       validExtensions);
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [safeThis, chooser](const juce::FileChooser& c)
        {
            if (safeThis == nullptr)
                return;
            auto file = c.getResult();
            if (file.existsAsFile())
            {
                // TODO: Refactor
                // safeThis->audioProcessor.loadFileToTrack(file, 0);
                // safeThis->mainComponent.setWaveformFile(file);
                // safeThis->mainComponent.setWaveformPlaybackPosition(0.0);
            }
        });
}

void AudioLoopStationEditor::updateTransportButtons()
{
    // Logic for changing button colors/text will go here
}