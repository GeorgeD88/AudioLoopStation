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

    setWantsKeyboardFocus(true);
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
    if (audioProcessor.getReaderSource() != nullptr)
    {
        audioProcessor.getReaderSource()->setLooping(loopingToggle.getToggleState());
    }
}

void AudioLoopStationEditor::resized()
{
    auto bounds = getLocalBounds();
    openButton.setBounds(bounds.removeFromTop(36).reduced(8, 4));
    mainComponent.setBounds(bounds);
}

void AudioLoopStationEditor::timerCallback()
{
    if (audioProcessor.getReaderSource() != nullptr && audioProcessor.isPlaying())
    {
        auto pos = audioProcessor.getCurrentPosition();
        auto len = audioProcessor.getTransportSource().getLengthInSeconds();
        if (len > 0.0)
            mainComponent.setWaveformPlaybackPosition(pos / len);
    }
}

void AudioLoopStationEditor::openButtonClicked()
{
    juce::Component::SafePointer<AudioLoopStationEditor> safeThis(this);
    auto chooser = std::make_shared<juce::FileChooser>("Select an audio file...",
                                                       juce::File(),
                                                       "*.wav;*.aiff;*.mp3;*.flac;*.ogg");
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [safeThis, chooser](const juce::FileChooser& c)
        {
            if (safeThis == nullptr)
                return;
            auto file = c.getResult();
            if (file.existsAsFile())
            {
                safeThis->audioProcessor.loadFile(file);
                safeThis->mainComponent.setWaveformFile(file);
                safeThis->mainComponent.setWaveformPlaybackPosition(0.0);
            }
        });
}

void AudioLoopStationEditor::updateTransportButtons()
{
    // Logic for changing button colors/text will go here
}

//==============================================================================
bool AudioLoopStationEditor::keyPressed(const juce::KeyPress& key)
{
    // SPACE — play/stop toggle
    if (key == juce::KeyPress(juce::KeyPress::spaceKey))
    {
        if (audioProcessor.isPlaying())
        {
            audioProcessor.stopPlayback();
            mainComponent.setWaveformPlaybackPosition(0.0);
        }
        else
        {
            audioProcessor.startPlayback();
        }
        updateTransportButtons();
        return true;
    }

    auto& loopManager = audioProcessor.getLoopManager();
    auto& apvts       = audioProcessor.getApvts();

    // Keys 1–4: arm / disarm the corresponding track
    // SHIFT+1–4: mute / unmute the corresponding track
    for (int i = 0; i < TrackConfig::MAX_TRACKS; ++i)
    {
        const int digit = '1' + i;

        if (key == juce::KeyPress(digit))
        {
            if (auto* track = loopManager.getTrack(static_cast<size_t>(i)))
            {
                const bool nowArmed = !track->isArmed();
                track->armForRecording(nowArmed);
            }
            return true;
        }

        if (key == juce::KeyPress(digit, juce::ModifierKeys::shiftModifier, 0))
        {
            const juce::String paramId = "Track" + juce::String(i + 1) + "_Mute";
            if (auto* param = apvts.getParameter(paramId))
            {
                const float toggled = param->getValue() > 0.5f ? 0.0f : 1.0f;
                param->beginChangeGesture();
                param->setValueNotifyingHost(toggled);
                param->endChangeGesture();
            }
            return true;
        }
    }

    return false;
}

void AudioLoopStationEditor::mouseDown(const juce::MouseEvent& /*event*/)
{
    grabKeyboardFocus();
}