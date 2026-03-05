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

    // Global tempo/BPM parameter
    layout.add(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID("Tempo", 1),  // Use ParameterID for consistency
            "Tempo",
            juce::NormalisableRange<float>(TrackConfig::BPM_GLOBAL_MIN,
                                           TrackConfig::BPM_GLOBAL_MAX, 0.1f),
            TrackConfig::DEFAULT_BPM,
            juce::String(),
            juce::AudioProcessorParameter::genericParameter,
            [](float value, int) { return juce::String(value, 1) + " BPM"; },
            nullptr
    ));
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
          loopManager(syncEngine),
          fileHandler(std::make_unique<LoopFileHandler>()),
          apvts(*this, nullptr, "PARAMETERS", createParameterLayout()) {

    formatManager.registerBasicFormats();

    // Connect parameters to MixerEngine
    mixerEngine.attachParameters(apvts);

    // Link tempo to SyncEngine
    apvts.addParameterListener("Tempo", this);
}

AudioLoopStationAudioProcessor::~AudioLoopStationAudioProcessor()
{
    mixerEngine.detachParameters();
    apvts.removeParameterListener("Tempo", this);
}

//==============================================================================
/**
 * Handles parameter changes from the UI,
 * This currently only processes tempo changes.
 * Other parameters are handled directly by MixerEngine via attachParameters()
 *
 * @param parameterID  The ID of the changed parameter
 * @param newValue     The new parameter value
 */
void AudioLoopStationAudioProcessor::parameterChanged(const juce::String &parameterID, float newValue) {
    if (parameterID == "Tempo") {
        syncEngine.setTempo(newValue);
    }

    // Handle any other parameter changes that won't go in MixerEngine
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
    // Get channel config
    int numTrackChannels = juce::jmax(1, getTotalNumOutputChannels());

    // Prepare SyncEngine
    syncEngine.prepare(sampleRate, samplesPerBlock);

    // Prepare LoopManager
    loopManager.prepareToPlay(sampleRate, samplesPerBlock, numTrackChannels);

    // Prepare MixerEngine
    mixerEngine.prepare(sampleRate, samplesPerBlock);

    // Set initial tempo
    float tempo = apvts.getRawParameterValue("Tempo")->load();
    syncEngine.setTempo(tempo);

    // Legacy JUCE transport not needed as everything is handled by Gin's SamplePlayer
}

void AudioLoopStationAudioProcessor::releaseResources()
{
    loopManager.releaseResources();
    mixerEngine.prepare(0, 0);
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
    juce::ignoreUnused(midiMessages);
    juce::ScopedNoDenormals noDenormals;

    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // 1. Clear only the extra output channels (standard JUCE practice)
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    // 2. Process loop tracks into per-track buffers.
    loopManager.processBlock(buffer);

    // 3. Route per-track outputs through the mixer into the master output.
    mixerEngine.process(loopManager.getTrackOutputs(), buffer);

    // 4. Add Transport Source: Use a temporary buffer so we don't overwrite the loops
    if (readerSource.get() != nullptr)
    {
        juce::AudioBuffer<float> transportBuffer (buffer.getNumChannels(), buffer.getNumSamples());
        transportBuffer.clear();

        juce::AudioSourceChannelInfo info(&transportBuffer, 0, transportBuffer.getNumSamples());
        transportSource.getNextAudioBlock(info);

        // Add the transport audio TO the loop audio instead of replacing it
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            buffer.addFrom(ch, 0, transportBuffer, ch, 0, buffer.getNumSamples());
    }

    // 5. Update VU Meter: Now measuring the COMBINED output of loops + transport
    float peak = 0.0f;
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        auto* data = buffer.getReadPointer(ch);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            peak = juce::jmax(peak, std::abs(data[i]));
    }
    outputLevel.store(peak, std::memory_order_relaxed);
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
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml != nullptr) {
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
    }
}

void AudioLoopStationAudioProcessor::loadFileToTrack(const juce::File &audioFile, int trackIndex) {
    if (!fileHandler) fileHandler = std::make_unique<LoopFileHandler>();

    auto index = static_cast<size_t>(trackIndex);

    if (auto* track = loopManager.getTrack(index))
    {
        if (fileHandler->loadAudioFile(audioFile, *track))
        {
            DBG("Successfully loaded " + audioFile.getFileName() + " to Track " + juce::String(trackIndex + 1));
        }
    }
}

void AudioLoopStationAudioProcessor::startPlayback()
{
    loopManager.startAllPlayback();
    isPlaying_ = true;
}

void AudioLoopStationAudioProcessor::stopPlayback()
{
    loopManager.stopAllPlayback();
    isPlaying_ = false;
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AudioLoopStationAudioProcessor();
}
