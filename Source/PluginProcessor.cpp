#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
AudioLoopStationProcessor::AudioLoopStationProcessor()
        : AudioProcessor (BusesProperties()
#if ! JucePlugin_IsMidiEffect
#if ! JucePlugin_IsSynth
                                  .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
#endif
                                  .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
#endif
)
{
    formatManager.registerBasicFormats();
}

AudioLoopStationProcessor::~AudioLoopStationProcessor()
{
}

//==============================================================================
const juce::String AudioLoopStationProcessor::getName() const
{
    return JucePlugin_Name;
}

bool AudioLoopStationProcessor::acceptsMidi() const
{
#if JucePlugin_WantsMidiInput
    return true;
#else
    return false;
#endif
}

bool AudioLoopStationProcessor::producesMidi() const
{
#if JucePlugin_ProducesMidiOutput
    return true;
#else
    return false;
#endif
}

bool AudioLoopStationProcessor::isMidiEffect() const
{
#if JucePlugin_IsMidiEffect
    return true;
#else
    return false;
#endif
}

double AudioLoopStationProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int AudioLoopStationProcessor::getNumPrograms()
{
    return 1;
}

int AudioLoopStationProcessor::getCurrentProgram()
{
    return 0;
}

void AudioLoopStationProcessor::setCurrentProgram (int index)
{
    juce::ignoreUnused (index);
}

const juce::String AudioLoopStationProcessor::getProgramName (int index)
{
    juce::ignoreUnused (index);
    return {};
}

void AudioLoopStationProcessor::changeProgramName (int index, const juce::String& newName)
{
    juce::ignoreUnused (index, newName);
}

//==============================================================================
void AudioLoopStationProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    transportSource.prepareToPlay(samplesPerBlock, sampleRate);
}

void AudioLoopStationProcessor::releaseResources()
{
    transportSource.releaseResources();
}

bool AudioLoopStationProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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

void AudioLoopStationProcessor::processBlock (juce::AudioBuffer<float>& buffer,
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
bool AudioLoopStationProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* AudioLoopStationProcessor::createEditor()
{
    return new AudioLoopStationEditor (*this);
}

//==============================================================================
void AudioLoopStationProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::ignoreUnused (destData);
}

void AudioLoopStationProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    juce::ignoreUnused (data, sizeInBytes);
}

void AudioLoopStationProcessor::loadFile(const juce::File& audioFile)
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

void AudioLoopStationProcessor::startPlayback()
{
    transportSource.start();
}

void AudioLoopStationProcessor::stopPlayback()
{
    transportSource.stop();
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AudioLoopStationProcessor();
}
