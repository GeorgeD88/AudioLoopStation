#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Utils/Config.h"

//==============================================================================
AudioLoopStationEditor::AudioLoopStationEditor (AudioLoopStationAudioProcessor& p)
        : AudioProcessorEditor (p), audioProcessor (p), mainComponent(p)
{
    // Add back in after other features
    // openButton.setButtonText("Open...");
    // openButton.onClick = [this] { openButtonClicked(); };
    // addAndMakeVisible(openButton);

    // Add main component that contains tracks and meters
    addAndMakeVisible(mainComponent);

    setWantsKeyboardFocus(true);
    auto setupGlobalButtton = [&](juce::TextButton& textButton, juce::Colour buttonCol) {
        addAndMakeVisible(textButton);
        textButton.setClickingTogglesState(true);
        textButton.setColour(juce::TextButton::buttonColourId, buttonCol);
        textButton.setColour(juce::TextButton::textColourOnId, Colours_::textPrimary);
        textButton.setColour(juce::TextButton::textColourOffId, Colours_::textPrimary);
    };

    setupGlobalButtton(resetButton, Colours_::rec.darker(0.3f));
    setupGlobalButtton(bounceButton, juce::Colour(0xff7c3aed));

    mResetAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            audioProcessor.apvts, "ResetAll", resetButton);
    mBounceAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            audioProcessor.apvts, "BounceBack", bounceButton);

    addAndMakeVisible(bpmLabel);
    bpmLabel.setText("BPM: --", juce::dontSendNotification);
    bpmLabel.setColour(juce::Label::textColourId, Colours_::textPrimary);
    bpmLabel.setFont(juce::FontOptions(14.0f, juce::Font::bold));

    addAndMakeVisible(stateLabel);
    stateLabel.setText("WAITING", juce::dontSendNotification);
    stateLabel.setColour(juce::Label::textColourId, Colours_::textDim);
    stateLabel.setFont(juce::FontOptions(12.0f));

    addAndMakeVisible(midiSyncLabel);
    midiSyncLabel.setText("MIDI SYNC", juce::dontSendNotification);
    midiSyncLabel.setColour(juce::Label::textColourId, Colours_::textDim);
    midiSyncLabel.setFont(juce::FontOptions(11.0f));

    addAndMakeVisible(midiSyncChannelSelector);
    for (int ch = 1; ch <= 16; ++ch)
        midiSyncChannelSelector.addItem("CH " + juce::String(ch), ch);

    mMidiSyncChannelAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
            audioProcessor.apvts, "MidiSyncChannel", midiSyncChannelSelector);

    setSize (1200, 900);
    startTimerHz(30);
}

AudioLoopStationEditor::~AudioLoopStationEditor()
{
    setLookAndFeel(nullptr);
    stopTimer();
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

void AudioLoopStationEditor::paint (juce::Graphics& g)
{
    g.fillAll(Colours_::bg);

    // Title
    auto header = getLocalBounds().removeFromTop(48);
    g.setColour(Colours_::textPrimary);
    g.setFont(juce::FontOptions(22.0f, juce::Font::bold));
    g.drawText("Audio Loop Station", header.reduced(14, 0), juce::Justification::centredLeft);
}

void AudioLoopStationEditor::resized()
{
    auto bounds = getLocalBounds();

    // Header bar
    auto header = bounds.removeFromTop(48);
    auto headerRight = header.removeFromRight(360).reduced(8);
    resetButton.setBounds(headerRight.removeFromRight(70));
    headerRight.removeFromRight(4);
    bounceButton.setBounds(headerRight.removeFromRight(70));
    headerRight.removeFromRight(10);
    midiSyncChannelSelector.setBounds(headerRight.removeFromRight(76));
    headerRight.removeFromRight(6);
    midiSyncLabel.setBounds(headerRight.removeFromRight(70));

    auto headerLeft = header.reduced(14, 0);
    headerLeft.removeFromLeft(180); // skip title space
    bpmLabel.setBounds(headerLeft.removeFromLeft(100));
    stateLabel.setBounds(headerLeft.removeFromLeft(200));

    bounds.removeFromTop(4);
    mainComponent.setBounds(bounds);

    /* Move to MainComponent for resizing tracks
    if (trackComponents.empty()) return;

    int trackH = (bounds.getHeight() - 8) / static_cast<int>(trackComponents.size());
    auto tracksArea = bounds.reduced(8, 0);

    for (auto& comp : trackComponents)
    {
        comp->setBounds(tracksArea.removeFromTop(trackH).reduced(0, 2));
    }
    */
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

//==============================================================================
bool AudioLoopStationEditor::keyPressed(const juce::KeyPress& key)
{
    // SPACE — play/stop toggle
    if (key == juce::KeyPress(juce::KeyPress::spaceKey))
    {
       audioProcessor.bounceBack();
    }

    auto& tracks = audioProcessor.getTracks();
    auto& apvts       = audioProcessor.apvts;

    // Keys 1–4: Start recording on empty track, or play/overdub if it has a loop
    // SHIFT+1–4: mute / unmute the corresponding track
    for (int i = 0; i < Config::NUM_TRACKS; ++i)
    {
        const int digit = '1' + i;

        if (key == juce::KeyPress(digit))
        {
            if (i < static_cast<int>(tracks.size()))
            {
                auto* track = tracks[i].get();
                if (track)
                {
                    auto state = track->getState();
                    switch (state)
                    {
                        case LoopTrack::State::Empty:
                            track->setRecording();
                            break;
                        case LoopTrack::State::Recording:
                            track->setPlaying();
                            break;
                        case LoopTrack::State::Playing:
                            track->setOverdubbing();
                            break;
                        case LoopTrack::State::Overdubbing:
                            track->setPlaying();
                            break;
                        case LoopTrack::State::Stopped:
                            track->setPlaying();
                            break;
                    }
                }
            }
            return true;
        }
        if (key == juce::KeyPress(digit, juce::ModifierKeys::shiftModifier, 0))
        {
            if (i < static_cast<int>(tracks.size()))
            {
                auto* track = tracks[i].get();
                if (track)
                {
                    // Toggle mute via APVTS parameter
                    const juce::String paramId = "Track" + juce::String(i + 1) + "_Mute";
                    if (auto* param = apvts.getParameter(paramId))
                    {
                        const float toggled = param->getValue() > 0.5f ? 0.0f : 1.0f;
                        param->beginChangeGesture();
                        param->setValueNotifyingHost(toggled);
                        param->endChangeGesture();
                    }
                }
            }
            return true;
        }
        // CTRL+R = Reset ALl
        if (key == juce::KeyPress('r', juce::ModifierKeys::ctrlModifier, 0))
        {
            audioProcessor.resetAll();
            return true;
        }
    }
    // OPTIONAL 'b' for bounce back if space bar is implemented for a synced all playback
    return false;
}

void AudioLoopStationEditor::mouseDown(const juce::MouseEvent& /*event*/)
{
    grabKeyboardFocus();
}