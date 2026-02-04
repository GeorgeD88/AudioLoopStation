#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout AudioLoopStationAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    for (int trackIndex = 0; trackIndex < TrackConfig::MAX_TRACKS; ++trackIndex)
    {
        juce::String trackPrefix = "Track" + juce::String(trackIndex + 1) + "_";

        // Volume parameter (0.0 to 1.0, default 0.8)
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(trackPrefix + "Volume", 1),
            trackPrefix + "Volume",
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f),
            0.8f,
            juce::String(),
            juce::AudioProcessorParameter::genericParameter,
            [](float value, int) { return juce::String(value * 100.0f, 1) + "%"; },
            nullptr
        ));

        // Pan parameter (-1.0 to 1.0, default 0.0)
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(trackPrefix + "Pan", 1),
            trackPrefix + "Pan",
            juce::NormalisableRange<float>(-1.0f, 1.0f, 0.01f),
            0.0f,
            juce::String(),
            juce::AudioProcessorParameter::genericParameter,
            [](float value, int) {
                if (value < -0.01f) return juce::String(value * 100.0f, 1) + "% L";
                if (value > 0.01f) return juce::String(value * 100.0f, 1) + "% R";
                return juce::String("Center");
            },
            nullptr
        ));

        // Mute parameter (bool, default false)
        layout.add(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID(trackPrefix + "Mute", 1),
            trackPrefix + "Mute",
            false,
            juce::String(),
            [](bool value, int) { return value ? "Muted" : "Unmuted"; },
            nullptr
        ));

        // Solo parameter (bool, default false)
        layout.add(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID(trackPrefix + "Solo", 1),
            trackPrefix + "Solo",
            false,
            juce::String(),
            [](bool value, int) { return value ? "Soloed" : "Not Soloed"; },
            nullptr
        ));
    }

    return layout;
}

//==============================================================================
AudioLoopStationAudioProcessor::AudioLoopStationAudioProcessor()
        : AudioProcessor (BusesProperties()
#if ! JucePlugin_IsMidiEffect
#if ! JucePlugin_IsSynth
                                  .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
#endif
                                  .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
#endif
),
        apvts(*this, nullptr, "PARAMETERS", createParameterLayout())
{
    formatManager.registerBasicFormats();
}

AudioLoopStationAudioProcessor::~AudioLoopStationAudioProcessor()
{
}

//==============================================================================
const juce::String AudioLoopStationAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool AudioLoopStationAudioProcessor::acceptsMidi() const
{
#if JucePlugin_WantsMidiInput
    return true;
#else
    return false;
#endif
}

bool AudioLoopStationAudioProcessor::producesMidi() const
{
#if JucePlugin_ProducesMidiOutput
    return true;
#else
    return false;
#endif
}

bool AudioLoopStationAudioProcessor::isMidiEffect() const
{
#if JucePlugin_IsMidiEffect
    return true;
#else
    return false;
#endif
}

double AudioLoopStationAudioProcessor::getTailLengthSeconds() const
{
    // Delay after audio is stopped
    return 0.0;
}

int AudioLoopStationAudioProcessor::getNumPrograms()
{
    return 1;
}

int AudioLoopStationAudioProcessor::getCurrentProgram()
{
    return 0;
}

void AudioLoopStationAudioProcessor::setCurrentProgram (int index)
{
    juce::ignoreUnused (index);
}

const juce::String AudioLoopStationAudioProcessor::getProgramName (int index)
{
    juce::ignoreUnused (index);
    return {};
}

void AudioLoopStationAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
    juce::ignoreUnused (index, newName);
}

//==============================================================================
void AudioLoopStationAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    transportSource.prepareToPlay(samplesPerBlock, sampleRate);
}

void AudioLoopStationAudioProcessor::releaseResources()
{
    transportSource.releaseResources();
}

bool AudioLoopStationAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
#if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
#else
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
        && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

#if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
#endif

    return true;
#endif
}

void AudioLoopStationAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                              juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused (midiMessages);

    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // Clear output channels that don't contain input data
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    // Clear the main buffer first
    buffer.clear();

    // Let the transport source fill the buffer
    if (readerSource.get() != nullptr)
    {
        juce::AudioSourceChannelInfo info(&buffer, 0, buffer.getNumSamples());
        transportSource.getNextAudioBlock(info);
    }
}

//==============================================================================
bool AudioLoopStationAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* AudioLoopStationAudioProcessor::createEditor()
{
    return new AudioLoopStationEditor (*this);
}

//==============================================================================
void AudioLoopStationAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void AudioLoopStationAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));

    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName(apvts.state.getType()))
            apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
}

void AudioLoopStationAudioProcessor::loadFile(const juce::File& audioFile)
{
    // Stop anything currently playing
    transportSource.stop();
    transportSource.setSource(nullptr);
    readerSource.reset();

    if (audioFile.existsAsFile())
    {
        auto* reader = formatManager.createReaderFor(audioFile);

        if (reader != nullptr)
        {
            auto newSource = std::make_unique<juce::AudioFormatReaderSource>(reader, true);

            // Set the source with appropriate parameters
            transportSource.setSource(newSource.get(),
                                      0,                    // no read-ahead
                                      nullptr,              // no background thread
                                      reader->sampleRate);  // sample rate for conversion

            // Keep the source alive
            readerSource.reset(newSource.release());

            // Enable looping for our loop station
            readerSource->setLooping(true);
        }
    }
}

void AudioLoopStationAudioProcessor::startPlayback()
{
    transportSource.start();
}

void AudioLoopStationAudioProcessor::stopPlayback()
{
    transportSource.stop();
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AudioLoopStationAudioProcessor();
}
