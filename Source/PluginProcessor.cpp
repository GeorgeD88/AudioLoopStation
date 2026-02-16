#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::AudioProcessorValueTreeState::ParameterLayout
AudioLoopStationAudioProcessor::createParameterLayout() {
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // Track parameters
    for (int i = 0; i < TrackConfig::MAX_TRACKS; i++) {
        juce::String id = "track_" + juce::String(i);

        // Volume parameter (dB)
        layout.add(std::make_unique<juce::AudioParameterFloat>(
                id + "_vol",
                id + " Volume",
                juce::NormalisableRange<float>(TrackConfig::MIN_VOLUME_DB,
                                               TrackConfig::MAX_VOLUME_DB, 0.1f),
                                               TrackConfig::DEFAULT_VOLUME_DB,
                                               juce::AudioParameterFloatAttributes().withLabel("dB")));

        // Pan parameters
        layout.add(std::make_unique<juce::AudioParameterFloat>(
                id + "_pan",
                id + " Pan",
                juce::NormalisableRange<float>(TrackConfig::MIN_PAN,
                                               TrackConfig::MAX_PAN, 0.1f),
                TrackConfig::DEFAULT_PAN,
                juce::AudioParameterFloatAttributes().withLabel("%")));

        // Mute button
        layout.add(std::make_unique<juce::AudioParameterBool>(
                id + "_mute",
                id + " Mute",
                false));

        // Solo button
        layout.add(std::make_unique<juce::AudioParameterBool>(
                id + "_solo",
                id + " Mute",
                false));

        // Record arm button
        layout.add(std::make_unique<juce::AudioParameterBool>(
                id + "_arm",
                id + "Record Arm",
                false));
    }
    // Global tempo/BPM parameter
    layout.add(std::make_unique<juce::AudioParameterFloat>(
            "tempo",
            "Tempo",
            juce::NormalisableRange<float>(TrackConfig::BPM_GLOBAL_MIN,
                                           TrackConfig::BPM_GLOBAL_MAX,
                                           TrackConfig::DEFAULT_BPM_INCR),
            TrackConfig::DEFAULT_BPM,
            juce::AudioParameterFloatAttributes().withLabel("BPM")));

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
          apvts(*this, nullptr, "PARAMETERS", createParameterLayout()) {
    formatManager.registerBasicFormats();

    // Connect parameters to MixerEngine
    mixerEngine.attachParameters(apvts);

    // Link tempo to SyncEngine
    apvts.addParameterListener("tempo", this);
}

AudioLoopStationAudioProcessor::~AudioLoopStationAudioProcessor()
{
    apvts.removeParameterListener("tempo", this);
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
    if (parameterID == "tempo") {
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
    int numInputChannels = getTotalNumInputChannels();

    // Prepare SyncEngine
    syncEngine.prepare(sampleRate, samplesPerBlock);

    // Prepare LoopManager
    loopManager.prepareToPlay(sampleRate, samplesPerBlock, numInputChannels);

    // Prepare MixerEngine
    mixerEngine.prepare(sampleRate, samplesPerBlock);

    // Set initial tempo
    float tempo = apvts.getRawParameterValue("tempo")->load();
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
    juce::ignoreUnused (midiMessages);

    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // Clear output channels that don't contain input data
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    // Clear the main buffer first
    loopManager.processBlock(buffer, buffer);

    // Get track outputs from LoopManager
    auto trackOutputs = loopManager.getTrackOutputs();

    // Mix outputs
    mixerEngine.process(trackOutputs, buffer);

    // Update playback state
    isPlaying_ = (loopManager.isAnyTrackRecording()
            || !loopManager.isAllTracksEmpty());
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
