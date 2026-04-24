#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
AudioLoopStationAudioProcessor::AudioLoopStationAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
        : AudioProcessor (BusesProperties()
#if ! JucePlugin_IsMidiEffect
#if ! JucePlugin_IsSynth
                                  .withInput  ("Input",        juce::AudioChannelSet::stereo(), true)
                                  .withInput  ("FX Return 1",  juce::AudioChannelSet::stereo(), false)
                                  .withInput  ("FX Return 2",  juce::AudioChannelSet::stereo(), false)
                                  .withInput  ("FX Return 3",  juce::AudioChannelSet::stereo(), false)
                                  .withInput  ("FX Return 4",  juce::AudioChannelSet::stereo(), false)
                                  .withInput  ("FX Return 5",  juce::AudioChannelSet::stereo(), false)
                                  .withInput  ("FX Return 6",  juce::AudioChannelSet::stereo(), false)
#endif
                                  .withOutput ("Monitor 1/2",   juce::AudioChannelSet::stereo(), true)
                                  .withOutput ("Output 3/4",    juce::AudioChannelSet::stereo(), false)
                                  .withOutput ("Output 5/6",    juce::AudioChannelSet::stereo(), false)
                                  .withOutput ("Output 7/8",    juce::AudioChannelSet::stereo(), false)
                                  .withOutput ("Output 9/10",   juce::AudioChannelSet::stereo(), false)
                                  .withOutput ("Output 11/12",  juce::AudioChannelSet::stereo(), false)
                                  .withOutput ("Output 13/14",  juce::AudioChannelSet::stereo(), false)
#endif
),
          apvts(*this, nullptr, "PARAMETERS", createParameterLayout())
#else
: apvts(*this, nullptr, "PARAMETERS", createParameterLayout())
#endif
{
    // First initialize the logger
    DebugLogger::getInstance().initialize();

    LOG_SEP("PLUGIN CONSTRUCTOR");

    // Initialize tracks immediately so they exist for the Editor
    int numTracks = NUM_TRACKS;
    for (int i = 0; i < numTracks; ++i)
    {
        mTracks.push_back(std::make_unique<LoopTrack>());
        LOG("Track " + juce::String(i) + " created");
    }

    mixerEngine.attachParameters(apvts);

    // Cache APVTS parameter pointers for real-time access in processBlock
    for (int i = 0; i < NUM_TRACKS; ++i)
    {
        const juce::String prefix = "Track" + juce::String(i + 1) + "_";
        mParamVol[i]       = apvts.getRawParameterValue(prefix + "Volume");
        mParamRecPlay[i]   = apvts.getRawParameterValue(prefix + "Record");
        mParamStop[i]      = apvts.getRawParameterValue(prefix + "Stop");
        mParamMute[i]      = apvts.getRawParameterValue(prefix + "Mute");
        mParamSolo[i]      = apvts.getRawParameterValue(prefix + "Solo");
        mParamAfterLoop[i] = apvts.getRawParameterValue(prefix + "Afterloop");
        mParamClear[i]     = apvts.getRawParameterValue(prefix + "Clear");
        mParamUndo[i]      = apvts.getRawParameterValue(prefix + "Undo");
        mParamMul[i]       = apvts.getRawParameterValue(prefix + "Multiply");
        mParamDiv[i]       = apvts.getRawParameterValue(prefix + "Divide");
        mParamOutSelect[i] = apvts.getRawParameterValue(prefix + "OutSelect");
        mParamResample[i]  = apvts.getRawParameterValue(prefix + "FxReplace");
    }
    mParamBounce          = apvts.getRawParameterValue("BounceBack");
    mParamReset           = apvts.getRawParameterValue("ResetAll");
    mParamMidiSyncChannel = apvts.getRawParameterValue("MidiSyncChannel");
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
    return 0.0;
}

int AudioLoopStationAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
    // so this should be at least 1, even if you're not really implementing programs.
}

int AudioLoopStationAudioProcessor::getCurrentProgram()
{
    return 0;
}

void AudioLoopStationAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String AudioLoopStationAudioProcessor::getProgramName (int index)
{
    return {};
}

void AudioLoopStationAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void AudioLoopStationAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    LOG_SEP("PREPARE TO PLAY");
    LOG_VALUE("Sample Rate", sampleRate);
    LOG_VALUE("Samples Per Block", samplesPerBlock);

    // --- INITIALIZATION ---

    // 1. Prepare auxiliary input buffer
    // We need at least the number of input channels and the block size
    int numInputChannels = getTotalNumInputChannels();
    // Safety check for 0 channels (rare but possible)
    if (numInputChannels == 0) numInputChannels = 2;

    mInputCache.setSize(numInputChannels, samplesPerBlock);
    for (int i = 0; i < NUM_TRACKS; ++i)
    {
        mFxReturnCache[i].setSize(2, samplesPerBlock);
        mFxReturnCache[i].clear();
    }

    // 2. Setup Loop Tracks
    // Tracks are already created in constructor. Just prepare them.
    for (int i = 0; i < mTracks.size(); ++i)
    {
        LOG_TRACK(i, "PREPARE", "");
        mTracks[i]->prepareToPlay(sampleRate, samplesPerBlock);
    }

    // 3. Setup Retrospective Buffer (After Loop) - 5 minutes circular buffer
    int retroSize = static_cast<int>(sampleRate * 300.0);
    mRetrospectiveBuffer.setSize(2, retroSize);
    mRetrospectiveBuffer.clear();
    mRetroWritePos = 0;
    mRetroBufferSize = retroSize;

    // 4. Pre-allocate work buffer for bounce/afterloop operations
    mWorkBuffer.setSize(2, retroSize);
    mWorkBuffer.clear();

    LOG("Preparation complete");
}

void AudioLoopStationAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool AudioLoopStationAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
#if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
#else
    // Main output must be stereo
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // Input must be stereo
#if ! JucePlugin_IsSynth
    if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
#endif

    // Additional input buses (sidechain): must be stereo or disabled
    for (int i = 1; i < layouts.inputBuses.size(); ++i)
    {
        if (!layouts.inputBuses[i].isDisabled()
            && layouts.inputBuses[i] != juce::AudioChannelSet::stereo())
            return false;
    }

    // Additional output buses: must be stereo or disabled
    for (int i = 1; i < layouts.outputBuses.size(); ++i)
    {
        if (!layouts.outputBuses[i].isDisabled()
            && layouts.outputBuses[i] != juce::AudioChannelSet::stereo())
            return false;
    }

    return true;
#endif
}
#endif

void AudioLoopStationAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // --- SNAPSHOT ALL INPUTS before touching the buffer ---

    // Ensure our input cache is big enough (in case block size changes or wasn't set right)
    if (mInputCache.getNumSamples() < buffer.getNumSamples())
        mInputCache.setSize(juce::jmax(1, totalNumInputChannels), buffer.getNumSamples());

    // Snapshot main input (Bus 0)
    auto mainInputBuf = getBusBuffer(buffer, true, 0);
    for (int ch = 0; ch < mainInputBuf.getNumChannels(); ++ch)
        mInputCache.copyFrom(ch, 0, mainInputBuf, ch, 0, buffer.getNumSamples());

    // Snapshot per-track FX Return inputs (Buses 1-8) if enabled
    for (int t = 0; t < NUM_TRACKS; ++t)
    {
        if (mFxReturnCache[t].getNumSamples() < buffer.getNumSamples())
            mFxReturnCache[t].setSize(2, buffer.getNumSamples());
        mFxReturnCache[t].clear(0, buffer.getNumSamples());

        int fxBusIdx = t + 1; // Bus 0 = main input, Buses 1-4 = FX Returns
        if (fxBusIdx < getBusCount(true) && getBus(true, fxBusIdx)->isEnabled())
        {
            auto fxBuf = getBusBuffer(buffer, true, fxBusIdx);
            for (int ch = 0; ch < juce::jmin(2, fxBuf.getNumChannels()); ++ch)
                mFxReturnCache[t].copyFrom(ch, 0, fxBuf, ch, 0, buffer.getNumSamples());
        }
    }

    // --- CLEAR ALL output buses to prevent FX Return bleed ---
    // Input and output buses share channels in the buffer (e.g. FX Return 1 and
    // Output 3/4 both map to channels 2-3). Without clearing, FX Return audio
    // passes straight through to the output, causing feedback.
    for (int bus = 0; bus < getBusCount(false); ++bus)
    {
        if (getBus(false, bus)->isEnabled())
        {
            auto outBuf = getBusBuffer(buffer, false, bus);
            outBuf.clear(0, buffer.getNumSamples());
        }
    }

    // Re-add main input to Main Output for input monitoring
    {
        auto mainOutBuf = getBusBuffer(buffer, false, 0);
        for (int ch = 0; ch < juce::jmin(mInputCache.getNumChannels(), mainOutBuf.getNumChannels()); ++ch)
            mainOutBuf.copyFrom(ch, 0, mInputCache, ch, 0, buffer.getNumSamples());
    }

    // 2. Record input into retrospective buffer (circular) for After Loop feature
    if (mRetroBufferSize > 0)
    {
        int numSamples = buffer.getNumSamples();
        int retroCh = juce::jmin(mInputCache.getNumChannels(), mRetrospectiveBuffer.getNumChannels());
        for (int ch = 0; ch < retroCh; ++ch)
        {
            int toEnd = mRetroBufferSize - mRetroWritePos;
            if (numSamples <= toEnd)
            {
                mRetrospectiveBuffer.copyFrom(ch, mRetroWritePos, mInputCache, ch, 0, numSamples);
            }
            else
            {
                mRetrospectiveBuffer.copyFrom(ch, mRetroWritePos, mInputCache, ch, 0, toEnd);
                mRetrospectiveBuffer.copyFrom(ch, 0, mInputCache, ch, toEnd, numSamples - toEnd);
            }
        }
        mRetroWritePos = (mRetroWritePos + numSamples) % mRetroBufferSize;
    }

    // 3. Handle DAW parameter triggers (MIDI mapped via Ableton Configure, etc.)
    handleParameterChanges();

    // 4. Track Control Logic
    // Access via pointer
    if (!mTracks.empty())
    {
        auto* track1 = mTracks[0].get();
        LoopTrack::State state = track1->getState();

        // NOTE: We removed the automatic mIsRecording override logic here
        // because it was conflicting with the UI buttons (TrackComponent),
        // causing the loop to close immediately (buzzing).
        // The UI now manages the state directly.

        // --- FIRST LOOP LOGIC ---

        // Detect if the First Loop just finished recording
        if (mIsFirstLoop.load())
        {
            // If track 1 has transitioned to Playing and has a valid length
            if (track1->getState() == LoopTrack::State::Playing && track1->getLoopLengthSamples() > 0)
            {
                // Capture the primary loop details
                int len = track1->getLoopLengthSamples();
                mPrimaryLoopLengthSamples.store(len);
                calculateBpm(len, getSampleRate());

                LOG_SEP("FIRST LOOP COMPLETED");
                LOG_VALUE("Master Loop Length", len);
                LOG_VALUE("BPM", mBpm.load());
                LOG_VALUE("Global Position", mGlobalPlaybackPosition);

                // Switch mode
                mIsFirstLoop.store(false);
                mGlobalPlaybackPosition = 0;
            }
        }
    }

    // 3. Process All Tracks
    int masterLength = mPrimaryLoopLengthSamples.load();
    bool isFirstLoopPhase = mIsFirstLoop.load();
    juce::int64 currentGlobalTotal = mGlobalTotalSamples.load();

    // Check for Global Solo
    bool anySolo = false;
    for (auto& t : mTracks)
    {
        if (t->getSolo())
        {
            anySolo = true;
            break;
        }
    }

    for (size_t i = 0; i < mTracks.size(); ++i)
    {
        bool isMaster = (i == 0);

        // If Master Track changes status (e.g. multiplied/divided), we need to update mPrimaryLoopLengthSamples
        if (isMaster && masterLength > 0 && !isFirstLoopPhase)
        {
            // Check if loop length changed
            int currentLen = mTracks[i]->getLoopLengthSamples();
            if (currentLen > 0 && currentLen != masterLength)
            {
                // Update global length
                masterLength = currentLen;
                mPrimaryLoopLengthSamples.store(masterLength);
            }
        }

        // Determine output bus for this track from APVTS parameter
        int outChoice = (i < NUM_TRACKS) ? juce::roundToInt(mParamOutSelect[i]->load()) : 0;
        int targetBus = outChoice;

        // Safety: fallback to main output if selected bus is out of range or disabled
        if (targetBus < 0 || targetBus >= getBusCount(false)
            || !getBus(false, targetBus)->isEnabled())
            targetBus = 0;

        auto busBuffer = getBusBuffer(buffer, false, targetBus);

        auto& fxCache = (i < NUM_TRACKS) ? mFxReturnCache[i] : mFxReturnCache[0];
        mTracks[i]->processBlock(busBuffer, mInputCache, fxCache, currentGlobalTotal, isMaster, masterLength, anySolo);
    }

    // 4. Update Global Transport (Playback & Synchronization)
    if (!isFirstLoopPhase && masterLength > 0)
    {
        mGlobalPlaybackPosition += buffer.getNumSamples();
        mGlobalPlaybackPosition %= masterLength;

        mGlobalTotalSamples.store(currentGlobalTotal + buffer.getNumSamples());
    }

    // 5. Execute deferred heavy operations (bounce, afterloop)
    executePendingOperations();

    // 6. Output MIDI Clock (24 PPQN) based on detected BPM + optional MIDI pulse on selected channel
    double bpm = mBpm.load();
    if (bpm > 10.0 && masterLength > 0 && !isFirstLoopPhase)
    {
        int syncChannel = 1;
        if (mParamMidiSyncChannel != nullptr)
            syncChannel = juce::jlimit(1, 16, juce::roundToInt(mParamMidiSyncChannel->load()) + 1);

        if (!mMidiClockRunning)
        {
            // Send MIDI Start
            midiMessages.addEvent(juce::MidiMessage(0xFA), 0);
            // Optional pulse to help routing/monitoring on virtual MIDI tracks
            midiMessages.addEvent(juce::MidiMessage::noteOn(syncChannel, mMidiPulseNote, (juce::uint8)1), 0);
            midiMessages.addEvent(juce::MidiMessage::noteOff(syncChannel, mMidiPulseNote), juce::jmin(4, buffer.getNumSamples() - 1));
            mMidiClockRunning = true;
            mMidiClockAccumulator = 0.0;
        }

        double samplesPerTick = (getSampleRate() * 60.0) / (bpm * 24.0);
        int numSamples = buffer.getNumSamples();

        while (mMidiClockAccumulator < (double)numSamples)
        {
            int tickPos = static_cast<int>(mMidiClockAccumulator);
            if (tickPos >= numSamples) break;
            midiMessages.addEvent(juce::MidiMessage(0xF8), tickPos);
            // Also mirror each clock tick as a very short note pulse on the selected MIDI channel.
            // Some hosts/devices route channel messages more reliably than real-time MIDI clock.
            midiMessages.addEvent(juce::MidiMessage::noteOn(syncChannel, mMidiPulseNote, (juce::uint8)1), tickPos);
            int offPos = juce::jmin(numSamples - 1, tickPos + 1);
            midiMessages.addEvent(juce::MidiMessage::noteOff(syncChannel, mMidiPulseNote), offPos);
            mMidiClockAccumulator += samplesPerTick;
        }
        mMidiClockAccumulator -= (double)numSamples;
    }
    else if (mMidiClockRunning)
    {
        // Send MIDI Stop
        midiMessages.addEvent(juce::MidiMessage(0xFC), 0);
        mMidiClockRunning = false;
        mMidiClockAccumulator = 0.0;
    }
}

//==============================================================================
bool AudioLoopStationAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* AudioLoopStationAudioProcessor::createEditor()
{
    return new AudioLoopStationEditor (*this);
}

//==============================================================================
void AudioLoopStationAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
}

void AudioLoopStationAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
}

void AudioLoopStationAudioProcessor::calculateBpm(int lengthSamples, double sampleRate)
{
    if (lengthSamples <= 0 || sampleRate <= 0) return;

    double durationSecs = (double)lengthSamples / sampleRate;

    // Calculate raw BPM based on the assumption that the loop is 1 Beat
    double rawBpm = 60.0 / durationSecs;

    // Sanity Check & Adjustment (Target 70-140 BPM)
    // If BPM is too low (long loop), assume it contains multiple beats/bars
    while (rawBpm < 70.0)
    {
        rawBpm *= 2.0;
    }
    // If BPM is too high (short loop), assume it's a fraction of a beat (though less common for loopers)
    while (rawBpm > 140.0)
    {
        rawBpm /= 2.0;
    }

    mBpm.store(rawBpm);

    // Log finding (debug)
    // DBG("Calculated BPM: " << rawBpm);
}

void AudioLoopStationAudioProcessor::resetAll()
{
    LOG_SEP("RESET ALL");
    suspendProcessing(true);
    resetAllInternal();
    LOG("Reset complete");
    LOG_VALUE("IsFirstLoop", true);
    LOG_VALUE("PrimaryLoopLength", 0);
    LOG_VALUE("GlobalPosition", 0);
    suspendProcessing(false);
}

void AudioLoopStationAudioProcessor::resetAllInternal()
{
    for (int i = 0; i < mTracks.size(); ++i)
    {
        LOG_TRACK(i, "RESET", "");
        mTracks[i]->clear();
    }

    mIsFirstLoop.store(true);
    mPrimaryLoopLengthSamples.store(0);
    mGlobalPlaybackPosition = 0;
    mGlobalTotalSamples.store(0);
    mBpm.store(0.0);
}

//==============================================================================
// Parameter Layout for DAW integration (Ableton Configure, MIDI mapping)

juce::AudioProcessorValueTreeState::ParameterLayout AudioLoopStationAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    for (int i = 0; i < NUM_TRACKS; ++i)
    {
        const juce::String prefix = "Track" + juce::String(i + 1) + "_";
        auto name = "Track " + juce::String(i + 1);                                 // Display label

        layout.add(std::make_unique<juce::AudioParameterFloat>(
                juce::ParameterID(prefix + "Volume", 1), name + " Volume",
                juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.8f));
        layout.add(std::make_unique<juce::AudioParameterFloat>(
                juce::ParameterID(prefix + "Pan", 1), name + " Pan",
                juce::NormalisableRange<float>(-1.0f, 1.0f, 0.01f), 0.0f));
        layout.add(std::make_unique<juce::AudioParameterBool>(
                juce::ParameterID(prefix + "Record", 1), name + " Rec/Play", false));
        layout.add(std::make_unique<juce::AudioParameterBool>(
                juce::ParameterID(prefix + "Mute", 1), name + " Mute", false));
        layout.add(std::make_unique<juce::AudioParameterBool>(
                juce::ParameterID(prefix + "Stop", 1), name + " Stop", false));
        layout.add(std::make_unique<juce::AudioParameterBool>(
                juce::ParameterID(prefix + "Solo", 1), name + " Solo", false));
        layout.add(std::make_unique<juce::AudioParameterBool>(
                juce::ParameterID(prefix + "Afterloop", 1), name + " After Loop", false));
        layout.add(std::make_unique<juce::AudioParameterBool>(
                juce::ParameterID(prefix + "Clear", 1), name + " Clear", false));
        layout.add(std::make_unique<juce::AudioParameterBool>(
                juce::ParameterID(prefix + "Undo", 1), name + " Undo", false));
        layout.add(std::make_unique<juce::AudioParameterBool>(
                juce::ParameterID(prefix + "Multiply", 1), name + " Multiply", false));
        layout.add(std::make_unique<juce::AudioParameterBool>(
                juce::ParameterID(prefix + "Divide", 1), name + " Divide", false));
        layout.add(std::make_unique<juce::AudioParameterChoice>(
                juce::ParameterID(prefix + "OutSelect", 1), name + " Output",
                juce::StringArray{"Monitor 1/2", "Output 3/4", "Output 5/6", "Output 7/8",
                                  "Output 9/10", "Output 11/12", "Output 13/14"}, i + 1));
        layout.add(std::make_unique<juce::AudioParameterBool>(
                juce::ParameterID(prefix + "FxReplace", 1), name + " FX Replace", false));
    }

    layout.add(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID("BounceBack", 1), "Bounce Back", false));
    layout.add(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID("ResetAll", 1), "Reset All", false));
    layout.add(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID("MidiSyncChannel", 1), "MIDI Sync Channel",
            juce::StringArray{
                    "CH 1", "CH 2", "CH 3", "CH 4", "CH 5", "CH 6", "CH 7", "CH 8",
                    "CH 9", "CH 10", "CH 11", "CH 12", "CH 13", "CH 14", "CH 15", "CH 16"
            },
            0));

    return layout;
}

void AudioLoopStationAudioProcessor::handleParameterChanges()
{
    for (int i = 0; i < (int)mTracks.size() && i < NUM_TRACKS; ++i)
    {
        // Volume (continuous, driven by SliderAttachment)
        float volVal = mParamVol[i]->load();
        mTracks[i]->setVolume(volVal);

        // Mute (direct sync, driven by ButtonAttachment toggle)
        bool muVal = mParamMute[i]->load() >= 0.5f;
        mTracks[i]->setMuted(muVal);

        // Solo (direct sync, driven by ButtonAttachment toggle)
        bool soVal = mParamSolo[i]->load() >= 0.5f;
        mTracks[i]->setSolo(soVal);

        // FX Replace trigger (any edge)
        bool rsmpVal = mParamResample[i]->load() >= 0.5f;
        if (rsmpVal != mPrevResample[i])
            mTracks[i]->applyFxReplace();
        mPrevResample[i] = rsmpVal;

        // Rec/Play trigger (any edge = state cycle, compatible with ButtonAttachment toggle)
        bool rpVal = mParamRecPlay[i]->load() >= 0.5f;
        if (rpVal != mPrevRecPlay[i])
        {
            auto state = mTracks[i]->getState();
            switch (state)
            {
                case LoopTrack::State::Empty:       mTracks[i]->setRecording(); break;
                case LoopTrack::State::Recording:   mTracks[i]->setPlaying(); break;
                case LoopTrack::State::Playing:     mTracks[i]->setOverdubbing(); break;
                case LoopTrack::State::Overdubbing:  mTracks[i]->setPlaying(); break;
                case LoopTrack::State::Stopped:     mTracks[i]->setPlaying(); break;
            }
        }
        mPrevRecPlay[i] = rpVal;

        // Stop trigger (any edge)
        bool stVal = mParamStop[i]->load() >= 0.5f;
        if (stVal != mPrevStop[i])
            mTracks[i]->stop();
        mPrevStop[i] = stVal;

        // After Loop trigger (any edge)
        bool alVal = mParamAfterLoop[i]->load() >= 0.5f;
        if (alVal != mPrevAfterLoop[i])
            mPendingAfterLoop.store(i);
        mPrevAfterLoop[i] = alVal;

        // Clear trigger (any edge)
        bool clVal = mParamClear[i]->load() >= 0.5f;
        if (clVal != mPrevClear[i])
            mTracks[i]->clear();
        mPrevClear[i] = clVal;

        // Undo trigger (any edge)
        bool udVal = mParamUndo[i]->load() >= 0.5f;
        if (udVal != mPrevUndo[i])
            mTracks[i]->performUndo();
        mPrevUndo[i] = udVal;

        // Multiply trigger (any edge)
        bool mlVal = mParamMul[i]->load() >= 0.5f;
        if (mlVal != mPrevMul[i])
        {
            if (mTracks[i]->getState() == LoopTrack::State::Empty)
            {
                float m = mTracks[i]->getTargetMultiplier() * 2.0f;
                if (m > 64.0f) m = 64.0f;
                mTracks[i]->setTargetMultiplier(m);
            }
            else
            {
                mTracks[i]->multiplyLoop();
            }
        }
        mPrevMul[i] = mlVal;

        // Divide trigger (any edge)
        bool dvVal = mParamDiv[i]->load() >= 0.5f;
        if (dvVal != mPrevDiv[i])
        {
            if (mTracks[i]->getState() == LoopTrack::State::Empty)
            {
                float m = mTracks[i]->getTargetMultiplier() / 2.0f;
                if (m < 1.0f / 64.0f) m = 1.0f / 64.0f;
                mTracks[i]->setTargetMultiplier(m);
            }
            else
            {
                mTracks[i]->divideLoop();
            }
        }
        mPrevDiv[i] = dvVal;
    }

    // Bounce Back trigger (any edge)
    bool bnVal = mParamBounce->load() >= 0.5f;
    if (bnVal != mPrevBounce)
        mPendingBounce.store(true);
    mPrevBounce = bnVal;

    // Reset All trigger (any edge)
    bool rsVal = mParamReset->load() >= 0.5f;
    if (rsVal != mPrevReset)
        resetAllInternal();
    mPrevReset = rsVal;
}

//==============================================================================
// Deferred operations - run inside processBlock after tracks have been processed

void AudioLoopStationAudioProcessor::executePendingOperations()
{
    int pendingAL = mPendingAfterLoop.load();
    bool pendingBn = mPendingBounce.load();

    if (!pendingBn && pendingAL < 0)
        return;

    // No suspendProcessing: bounce uses progressive replace,
    // afterloop copies are small enough to run in one block.

    if (pendingBn)
    {
        mPendingBounce.store(false);
        performBounceBack();
    }

    if (pendingAL >= 0)
    {
        mPendingAfterLoop.store(-1);
        performCaptureAfterLoop(pendingAL);
    }
}

//==============================================================================
// Bounce Back: mix all tracks into track 1, clear others

void AudioLoopStationAudioProcessor::bounceBack()
{
    LOG_SEP("BOUNCE BACK");
    performBounceBack();
}

void AudioLoopStationAudioProcessor::performBounceBack()
{
    int masterLen = mPrimaryLoopLengthSamples.load();
    if (masterLen <= 0) return;

    // Find the longest loop across all tracks
    int bounceLen = masterLen;
    for (auto& t : mTracks)
    {
        if (t->hasLoop())
            bounceLen = juce::jmax(bounceLen, t->getLoopLengthSamples());
    }

    if (bounceLen > mWorkBuffer.getNumSamples()) return;

    mWorkBuffer.clear(0, bounceLen);

    // Mix all tracks into work buffer using block-based operations
    for (auto& t : mTracks)
    {
        if (!t->hasLoop()) continue;

        auto& lb = t->getLoopBuffer();
        int trackLen = t->getLoopLengthSamples();
        juce::int64 startGlobal = t->getRecordingStartGlobalSample();

        int numCh = juce::jmin(2, lb.getNumChannels());

        // Compute where in the track's loop sample 0 of the bounce corresponds to
        juce::int64 elapsedAtZero = -startGlobal;
        int readStart = static_cast<int>(((elapsedAtZero % trackLen) + trackLen) % trackLen);

        // Block-copy with wrapping
        for (int ch = 0; ch < numCh; ++ch)
        {
            int remaining = bounceLen;
            int dstPos = 0;
            int srcPos = readStart;

            while (remaining > 0)
            {
                int toLoopEnd = trackLen - srcPos;
                int chunk = juce::jmin(remaining, toLoopEnd);
                mWorkBuffer.addFrom(ch, dstPos, lb, ch, srcPos, chunk);
                dstPos += chunk;
                srcPos += chunk;
                if (srcPos >= trackLen) srcPos = 0;
                remaining -= chunk;
            }
        }
    }

    // Apply crossfade to avoid click at loop boundary
    LoopTrack::applyCrossfade(mWorkBuffer, bounceLen, CROSSFADE_SAMPLES);

    // Start progressive replacement on track 1 (playhead-first, zero glitch)
    mTracks[0]->beginProgressiveReplace(&mWorkBuffer, bounceLen, 0, 0);

    // Clear all other tracks immediately (they go silent)
    for (size_t i = 1; i < mTracks.size(); ++i)
        mTracks[i]->clear();

    mPrimaryLoopLengthSamples.store(bounceLen);
    LOG("Bounce started progressive | bounceLen=" + juce::String(bounceLen));
}

//==============================================================================
// After Loop: capture last N bars of audio from retrospective buffer into a specific track

void AudioLoopStationAudioProcessor::captureAfterLoop(int trackIndex)
{
    LOG_SEP("AFTER LOOP (Track " + juce::String(trackIndex) + ")");
    performCaptureAfterLoop(trackIndex);
}

void AudioLoopStationAudioProcessor::performCaptureAfterLoop(int trackIndex)
{
    int masterLen = mPrimaryLoopLengthSamples.load();
    if (masterLen <= 0 || mRetroBufferSize <= 0) return;
    if (trackIndex < 0 || trackIndex >= (int)mTracks.size()) return;

    // Determine capture length based on targetMultiplier
    float mult = mTracks[trackIndex]->getTargetMultiplier();
    if (mult < 1.0f / 64.0f) mult = 1.0f / 64.0f;
    if (mult > 64.0f) mult = 64.0f;
    int captureLen = static_cast<int>((float)masterLen * mult);

    if (captureLen <= 0) return;
    if (captureLen > mRetroBufferSize) return;
    if (captureLen > mWorkBuffer.getNumSamples()) return;

    // Read last captureLen samples from the circular retrospective buffer
    int retroReadStart = (mRetroWritePos - captureLen + mRetroBufferSize) % mRetroBufferSize;

    mWorkBuffer.clear(0, captureLen);

    int retroCh = juce::jmin(2, mRetrospectiveBuffer.getNumChannels());
    for (int ch = 0; ch < retroCh; ++ch)
    {
        int toEnd = mRetroBufferSize - retroReadStart;
        if (captureLen <= toEnd)
        {
            mWorkBuffer.copyFrom(ch, 0, mRetrospectiveBuffer, ch, retroReadStart, captureLen);
        }
        else
        {
            mWorkBuffer.copyFrom(ch, 0, mRetrospectiveBuffer, ch, retroReadStart, toEnd);
            mWorkBuffer.copyFrom(ch, toEnd, mRetrospectiveBuffer, ch, 0, captureLen - toEnd);
        }
    }

    juce::int64 globalNow = mGlobalTotalSamples.load();
    juce::int64 captureStartGlobal = globalNow - (juce::int64)captureLen;
    if (captureStartGlobal < 0) captureStartGlobal = 0;

    // Apply crossfade to avoid click at loop boundary
    LoopTrack::applyCrossfade(mWorkBuffer, captureLen, CROSSFADE_SAMPLES);

    if (!mTracks[trackIndex]->hasLoop())
    {
        // Track is empty: create a fresh loop from captured audio
        int alignedOffset = static_cast<int>(captureStartGlobal % masterLen);
        mTracks[trackIndex]->setLoopFromMix(mWorkBuffer, captureLen, alignedOffset, captureStartGlobal);
        LOG("After Loop (new): " + juce::String(captureLen) + " samples (mult=" + juce::String(mult) +
            ") into track " + juce::String(trackIndex) + " globalStart=" + juce::String(captureStartGlobal));
    }
    else
    {
        // Track has a loop: overdub captured audio on top
        mTracks[trackIndex]->overdubFromBuffer(mWorkBuffer, captureLen, captureStartGlobal);
        LOG("After Loop (overdub): " + juce::String(captureLen) + " samples (mult=" + juce::String(mult) +
            ") into track " + juce::String(trackIndex));
    }
}


//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AudioLoopStationAudioProcessor();
}

/*
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

void AudioLoopStationAudioProcessor::startRecordingOnArmedTrack()
{
    // Find the armed track
    for (size_t i = 0; i < TrackConfig::MAX_TRACKS; ++i) {
        auto* track = loopManager.getTrack(i);
        if (track && track->isArmed()) {
            // Found armed track - start it!
            if (track->getState() == LoopTrack::State::Queued ||
                track->getState() == LoopTrack::State::Empty) {
                // It's ready to record, start now
                loopManager.startRecording(i);
                DBG("[PROC] Started recording on armed track " << i);
            }
            return;
        }
    }

    DBG("[PROC] No armed track found");
}

void AudioLoopStationAudioProcessor::startRecording(int trackIndex)
{
    if (trackIndex >= 0 && trackIndex < static_cast<int>(TrackConfig::MAX_TRACKS)) {
        loopManager.startRecording(static_cast<size_t>(trackIndex));
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

void AudioLoopStationAudioProcessor::stopRecording() {
    loopManager.stopAllRecording();
}

void AudioLoopStationAudioProcessor::stopAll() {
    loopManager.stopAllRecording();
    loopManager.stopAllRecording();
    isPlaying_ = false;
}

void AudioLoopStationAudioProcessor::requestTrackRecording(int trackIndex) {
    if (trackIndex >= 0 && trackIndex < static_cast<int>(TrackConfig::MAX_TRACKS)) {
        loopManager.requestTrackRecording(static_cast<size_t>(trackIndex));
    }
}

void AudioLoopStationAudioProcessor::cancelTrackRecording(int trackIndex) {
    if (trackIndex >= 0 && trackIndex < static_cast<int>(TrackConfig::MAX_TRACKS)) {
        loopManager.cancelTrackRecording(static_cast<size_t>(trackIndex));
    }
}

void AudioLoopStationAudioProcessor::clearTrack(int trackIndex) {
    if (trackIndex >= 0 && trackIndex < static_cast<int>(TrackConfig::MAX_TRACKS)) {
        loopManager.postCommand({ LoopCommandType::ClearTrack,
                                  static_cast<size_t>(trackIndex), 0});
    }
}
*/


