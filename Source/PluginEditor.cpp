#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
AudioLoopStationEditor::AudioLoopStationEditor (AudioLoopStationAudioProcessor& p)
        : AudioProcessorEditor (p), audioProcessor (p)
{
    addAndMakeVisible (&openButton);
    openButton.setButtonText ("Open...");
    openButton.onClick = [this] { openButtonClicked(); };

    addAndMakeVisible (&playButton);
    playButton.setButtonText ("Play");
    playButton.onClick = [this] { playButtonClicked(); };
    playButton.setColour (juce::TextButton::buttonColourId, juce::Colours::green);
    playButton.setEnabled (false);

    addAndMakeVisible (&stopButton);
    stopButton.setButtonText ("Stop");
    stopButton.onClick = [this] { stopButtonClicked(); };
    stopButton.setColour (juce::TextButton::buttonColourId, juce::Colours::red);
    stopButton.setEnabled (false);

    addAndMakeVisible (&loopingToggle);
    loopingToggle.setButtonText ("Loop");
    loopingToggle.onClick = [this] { loopButtonChanged(); };

    addAndMakeVisible (&currentPositionLabel);
    currentPositionLabel.setText ("Stopped", juce::dontSendNotification);

    setSize (400, 200);
    startTimer (50); // Update every 50ms for smoother display
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
    openButton          .setBounds (10, 10,  getWidth() - 20, 20);
    playButton          .setBounds (10, 40,  getWidth() - 20, 20);
    stopButton          .setBounds (10, 70,  getWidth() - 20, 20);
    loopingToggle       .setBounds (10, 100, getWidth() - 20, 20);
    currentPositionLabel.setBounds (10, 130, getWidth() - 20, 20);
}

void AudioLoopStationEditor::timerCallback()
{
    auto& transport = audioProcessor.getTransportSource();

    if (transport.isPlaying())
    {
        juce::RelativeTime position (transport.getCurrentPosition());
        auto minutes = ((int) position.inMinutes()) % 60;
        auto seconds = ((int) position.inSeconds()) % 60;
        auto millis  = ((int) position.inMilliseconds()) % 1000;
        auto positionString = juce::String::formatted ("%02d:%02d:%03d", minutes, seconds, millis);
        currentPositionLabel.setText (positionString, juce::dontSendNotification);
    }
    else
    {
        currentPositionLabel.setText ("Stopped", juce::dontSendNotification);
    }

    // Update button states based on transport state
    updateTransportButtons();
}

void AudioLoopStationEditor::updateTransportButtons()
{
    bool isPlaying = audioProcessor.isPlaying();
    bool hasFile = audioProcessor.getReaderSource() != nullptr;

    playButton.setEnabled(hasFile && !isPlaying);
    stopButton.setEnabled(hasFile && isPlaying);
}

void AudioLoopStationEditor::openButtonClicked()
{
    chooser = std::make_unique<juce::FileChooser> ("Select a Wave file to play...",
                                                        juce::File {},
                                                        "*.wav;*.aif;*.aiff"); // Support multiple formats

    auto chooserFlags = juce::FileBrowserComponent::openMode
                        | juce::FileBrowserComponent::canSelectFiles;

    chooser->launchAsync (chooserFlags, [this] (const juce::FileChooser& fc)
    {
        auto file = fc.getResult();

        if (file != juce::File{})
        {
            audioProcessor.loadFile(file);
            updateTransportButtons();

            // Set looping state if we have a file
            if (audioProcessor.getReaderSource() != nullptr)
            {
                audioProcessor.getReaderSource()->setLooping(loopingToggle.getToggleState());
            }
        }
    });
}

void AudioLoopStationEditor::playButtonClicked()
{
    audioProcessor.startPlayback();
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