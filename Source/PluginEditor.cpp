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
    mainComponent.setWaveformPlaybackPosition(0.0);
    updateTransportButtons();
}

void AudioLoopStationEditor::loopButtonChanged()
{
    // old audio player no longer in use
    // if (audioProcessor.getReaderSource() != nullptr)
    // {
    //     audioProcessor.getReaderSource()->setLooping(loopingToggle.getToggleState());
    // }
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
    // update waveform playhead position based on global sample position
    auto& syncEngine = audioProcessor.getLoopManager().getSyncEngine();
    double globalSeconds = syncEngine.getGlobalSeconds();

    // find the longest track length to normalize position
    double maxLength = 0.0;

    for (size_t i = 0; i < audioProcessor.getLoopManager().getNumTracks(); ++i){
        if (auto* track = audioProcessor.getLoopManager().getTrack(i))
        {
            if (track->hasLoop())
            {
                auto trackLength = static_cast<double>(track->getLoopLengthSamples()/
                        syncEngine.getSampleRate());
                maxLength = std::max(maxLength, trackLength);
            }
        }
    }
    // Only update if we have a valid loop length and playback is active
    if (maxLength > 0.0 && audioProcessor.isPlaying())
    {
        double normalizedPosition = std::fmod(globalSeconds, maxLength) / maxLength;
        mainComponent.setWaveformPlaybackPosition(normalizedPosition);
    }
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
                safeThis->audioProcessor.loadFileToTrack(file, 0);
                safeThis->mainComponent.setWaveformFile(file);
                safeThis->mainComponent.setWaveformPlaybackPosition(0.0);
            }
        });
}

void AudioLoopStationEditor::updateTransportButtons()
{
    // Logic for changing button colors/text will go here
}