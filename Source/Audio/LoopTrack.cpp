//
// Created by Vincewa Tran on 1/28/26.
// Represents a single independent loop track with a state machine
// - Integrate the CircularBuffer + volume/pan + record/play state
// - Where audio data gets processed.
// - DSP and effects chain will eventually go here as well.

#include "LoopTrack.h"
#include "../Utils/DebugLogger.h"

LoopTrack::LoopTrack()
{
}

LoopTrack::~LoopTrack()
{
}

/**
 * Prepares the track for audio processing
 * @param sampleRate - Current audio sample rate (e.g., 48000 Hz)
 * @param samplesPerBlock - Buffer size for each process callback
 *
 * Called on the audio thread during prepareToPlay()
 * Allocates buffers and initializes DSP components
 */
void LoopTrack::prepareToPlay(double sampleRate, int /*samplesPerBlock*/) {

    trackSampleRate = sampleRate;

    // Allocate the buffer for the max supported loop time
    // Do this here in the constructor to avoid allocation in the process block
    int totalSamples = static_cast<int>(sampleRate * maxLoopLengthSeconds);

    // Assuming stereo for now
    loopBuffer.setSize(2, totalSamples);
    loopBuffer.clear();

    undoBuffer.setSize(2, totalSamples);
    undoBuffer.clear();

    fxCaptureBuffer.setSize(2, totalSamples);
    fxCaptureBuffer.clear();

    clear();
}

/**
 * Main audio processing callback
 * TODO: Define
 */
void LoopTrack::processBlock(juce::AudioBuffer<float>& outputBuffer, const juce::AudioBuffer<float>& inputBuffer,
                             const juce::AudioBuffer<float>& sidechainBuffer,
                             juce::int64 globalTotalSamples, bool isMasterTrack, int masterLoopLength, bool anySoloActive)
{
    const int numSamples = outputBuffer.getNumSamples();
    State state = currentState.load();

    // If stopped or empty, we generally don't output sound,
    // provided we aren't currently recording the first pass.
    if (state == State::Stopped || (state == State::Empty && state != State::Recording))
    {
        return;
    }

    // Check Solo/Mute logic
    // If ANY solo is active, we are muted UNLESS we are soloed.
    // If NO solo is active, we respect our local mute.
    bool shouldBeSilent = false;

    if (anySoloActive)
    {
        if (!isSolo.load()) shouldBeSilent = true;
    }
    else
    {
        if (isMuted.load()) shouldBeSilent = true;
    }

    // Auto-finish Fixed Length Recording (Slave Tracks)
    if (state == State::Recording && !isMasterTrack)
    {
        // Simple check: if we exceed the loop length, we switch to playing
        // Ensure calculation is correct with floats
        float multiplier = targetMultiplier;
        if (multiplier < 1.0f / 64.0f) multiplier = 1.0f / 64.0f;
        if (multiplier > 64.0f) multiplier = 64.0f;

        int targetLen = static_cast<int>((float)masterLoopLength * multiplier);
        if (targetLen < 1) targetLen = 1;                                           // Secure

        if (recordedSamplesCurrent >= targetLen)
        {
            // IMPORTANT: Set loop length BEFORE switching to Playing
            loopLengthSamples = targetLen;

            LOG("SLAVE REC FINISHED | recorded=" + juce::String(recordedSamplesCurrent) +
                " loopLen=" juce::String(loopLengthSamples) +
                " multiplier=" + juce::String(multiplier) +
                " offset=" + juce::juce::String(recordingStartOffset));

            setPlaying();
            state = State::Playing;

            // NOTE: Synchronization now happens automatically in the Playing section
            // thanks to the calculation with recordingStartOffset
        }

    }

    int writePos = 0;
    int readPos = 0;
    int currentLoopLength = 0;

    if (isMasterTrack)
    {
        if (state == State::Recording)
        {
            //Linear recording
            writePos = playbackPosition;
            currentLoopLength = loopBuffer.getNumSamples(); // Prevent overflow
        }
        else
        {
            // Master Playing/Overdubbing - use global transport provided by processor
            if (loopLengthSamples > 0)
            {
                readPos = globalTotalSamples % loopLengthSamples;
                writePos = readPos;
                currentLoopLength = loopLengthSamples;
            }
        }
    }
    else
    {
        // Slave Track
        if (state == State::Recording)
        {
            // IMPORTANT: Recording a slave track
            // We record linearly into the buffer starting from position 0
            // BUT we memorize at which position of the master cycle we started


            // If this is the first recorded sample (recordedSamplesCurrent == 0),
            // we capture the current position in the master cycle
            if (recordedSamplesCurrent == 0 && masterLoopLength > 0)
            {
                // Calculate current position in the master cycle
                recordingStartOffset = static_cast<int>(globalTotalSamples % masterLoopLength);
                recordingStartGlobalSample = globalTotalSamples;

                LOG("SLAVE REC START | offset=" + juce::String(recordingStartOffset) +
                    " globalSample=" + juce::String((juce::int64)globalTotalSamples) +
                    " masterLen=" + juce::String(masterLoopLength) +
                    " targetMult=" + juce::String(targetMultiplier));
            }
            // Record linearly from position 0 in our buffer
            writePos = recordedSamplesCurrent;
            currentLoopLength = loopBuffer.getNumSamples();

            // Calculate target length with multiplier
            float multiplier = targetMultiplier;
            if (multiplier < 1.0f / 64.0f) multiplier = 1.0f / 64.0f;
            if (multiplier > 64.0f) multiplier = 64.0f;
            int targetLen = static_cast<int>((float)masterLoopLength * multiplier);
            if (targetLen < 1) targetLen = 1;

            if (loopLengthSamples != targetLen) loopLengthSamples = targetLen;
        }
        else
        {
            // Slave Playing/Overdubbing
            // SYNCHRONIZATION: use the absolute time elapsed since the start of recording
            // This works for loops both shorter and longer than the master
            if (loopLengthSamples > 0)
            {
                juce::int64 elapsed = globalTotalSamples - recordingStartGlobalSample;
                if (elapsed < 0) elapsed = 0;
                readPos = static_cast<int>(elapsed % loopLengthSamples);
                writePos = readPos;
                currentLoopLength = loopLengthSamples;
            }
            else
            {
                currentLoopLength = masterLoopLength; //fallback
            }
        }
    }

    if (anySoloActive)
    {
        if (!isSolo.load()) shouldBeSilent = true;
    }
    else
    {
        if (isMuted.load()) shouldBeSilent = true;
    }
    // Apply any pending progressive buffer replacement (playhead-first)
    if (mReplace.active) processReplaceChunk(readPos, numSamples);

    switch (state)
    {
        case State::Recording:
            handleRecording(inputBuffer, numSamples, writePos);
            if (isMasterTrack)
                playbackPosition += numSamples;
            else
                recordedSamplesCurrent += numSamples;
            break;

        case State::Playing:
            // Continuously capture sidechain into staging buffer for later one-shot replace
            if (loopLengthSamples > 0)
                captureSidechain(sidechainBuffer, numSamples, readPos, currentLoopLength);

            // We must update position even if silent
            if (shouldBeSilent)
            {
                handlePlayback(outputBuffer, numSamples, readPos, currentLoopLength, true);
            }
            else
            {
                handlePlayback(outputBuffer, numSamples, readPos, currentLoopLength, false);
            }
            break;


        case State::Overdubbing:
            // Continuously capture sidechain into staging buffer for later one-shot replace
            if (loopLengthSamples > 0)
                captureSidechain(sidechainBuffer, numSamples, readPos, currentLoopLength);

            handleOverdub(outputBuffer, inputBuffer, numSamples, readPos, currentLoopLength, shouldBeSilent);
            break;

        default:
            break;
    }
}



/**
 * Clears all track data and resets to empty state
 * Safe to call from any thread
 */
void LoopTrack::clear() {
    currentState.store(State::Empty);
    loopLengthSamples = 0;
    playbackPosition = 0;
    recordedSamplesCurrent = 0;
    loopBuffer.clear();
    hasUndo = false;

    // IMPORTANT: Reset targetMultiplier to 1.0 (default value)
    targetMultiplier = 1.0f;

    // IMPORTANT: Reset synchronization offset
    recordingStartOffset = 0;
    recordingStartGlobalSample = 0;
    fxCaptureSamplesWritten = 0;

    // Cancel any active progressive replace processes
    mReplace.active = false;
    mReplace.source = nullptr;

}

void LoopTrack::saveUndo()
{
    // Back up the current loop buffer and length
    int len = loopLengthSamples;
    if (len > 0)
    {
        // We only copy the valid part
        for (int ch = 0; ch < undoBuffer.getNumChannels(); ++ch)
            undoBuffer.copyFrom(ch, 0, loopBuffer, ch, 0, len);

        undoLoopLengthSamples = len;
        hasUndo = true;
    }
}

void LoopTrack::performUndo()
{
    if (hasUndo && undoLoopLengthSamples > 0)
    {
        // Stop any active recording/overdubbing first
        if (currentState.load() == State::Recording || currentState.load() == State::Overdubbing)
            setPlaying();

        // Swap Logic (Undo <-> Current)
        // This effectively makes "Undo" toggle between two states (Undo/Redo)
        // We can just swap the data via copying.
        int currentLen = loopLengthSamples;
        int restoredLen = undoLoopLengthSamples;

        // Direct swap loop.
        int maxLen = juce::jmax(currentLen, restoredLen);
        for (int ch = 0; ch < loopBuffer.getNumChannels(); ++ch)
        {
            auto* d1 = loopBuffer.getWritePointer(ch);
            auto* d2 = undoBuffer.getWritePointer(ch);
            for (int i = 0; i < maxLen; ++i)
            {
                std::swap(d1[i], d2[i]);
            }
        }

        loopLengthSamples = restoredLen;
        undoLoopLengthSamples = currentLen;

        // hasUndo remains true (to allow Redo)
    }
}

void LoopTrack::multiplyLoop()
{
    if (loopLengthSamples <= 0)
    {
        // Pre-recording: increase target length
        targetMultiplier = std::min(64.0f, targetMultiplier * 2.0f);
        return;
    }

    // Check bounds
    if (loopLengthSamples * 2 > loopBuffer.getNumSamples()) return;

    saveUndo();

    // Copy [0..len] to [len..2*len]
    for (int ch = 0; ch < loopBuffer.getNumChannels(); ++ch)
    {
        loopBuffer.copyFrom(ch, loopLengthSamples, loopBuffer, ch, 0, loopLengthSamples);
    }

    loopLengthSamples *= 2;
}

void LoopTrack::divideLoop()
{
    if (loopLengthSamples <= 0)
    {
        // Pre-recording: decrease target length
        targetMultiplier = std::max(1.0f / 64.0f, targetMultiplier / 2.0f);
        return;
    }

    // Minimum ~256 samples to avoid degenerate loops
    if (loopLengthSamples / 2 < 256) return;

    saveUndo();

    loopLengthSamples /= 2;
}

//==============================================================================
// State Setters

void LoopTrack::setRecording()
{
    // Can only start fresh recording if we are empty or choose to overwrite.
    // For this basic implementation, we assume we can only 'Record' from Empty.
    // Use 'Overdub' to add to existing.
    if (currentState.load() == State::Empty)
    {
        playbackPosition = 0;
        loopLengthSamples = 0; // Reset length
        recordedSamplesCurrent = 0;

        LOG("LoopTrack: RECORDING started | targetMult=" + juce::String(targetMultiplier));

        currentState.store(State::Recording);
    }
}

void LoopTrack::setOverdubbing()
{
    if (loopLengthSamples > 0)
    {
        // Save state before overdubbing starts
        // CHECK: If we are already overdubbing, don't save again?
        // Usually we save when entering the state.
        if (currentState.load() != State::Overdubbing)
        {
            saveUndo();
        }

        currentState.store(State::Overdubbing);
    }
}

void LoopTrack::setPlaying()
{
    // If we were recording, this transition DEFINES the loop length
    if (currentState.load() == State::Recording)
    {
        // If master track (linear recording), use playbackPosition
        if (playbackPosition > 0)
        {
            loopLengthSamples = playbackPosition;
            LOG("LoopTrack: MASTER REC->PLAY | len=" + juce::String(loopLengthSamples) +
                " playbackPos=" + juce::String(playbackPosition));
        }
        else if (recordedSamplesCurrent > 0)
        {
            // SLAVE TRACK Logic
            // If we recorded as a slave, the loop length is handled externally or implicitly.
            // We ensure loopLengthSamples is valid if it wasn't already set.
            // Note: processBlock usually sets this for slaves, but just in case:
            if (loopLengthSamples == 0)
                loopLengthSamples = recordedSamplesCurrent; // Fallback

            LOG("LoopTrack: SLAVE REC->PLAY | len=" + juce::String(loopLengthSamples) +
                " recordedSamples=" + juce::String(recordedSamplesCurrent) +
                " targetMult=" + juce::String(targetMultiplier));
        }

        playbackPosition = 0; // Reset to start for playback
        LOG("LoopTrack: Playback position RESET to 0");
    }

    if (loopLengthSamples > 0)
    {
        // Smooth the loop boundary to eliminate the click when recording stops
        applyCrossfade(loopBuffer, loopLengthSamples, 128);

        currentState.store(State::Playing);
        LOG("LoopTrack: State = PLAYING");
    }
    else
    {
        LOG_ERROR("LoopTrack: Cannot play - loopLength is 0!");
    }
}

void LoopTrack::stop() {
    // If we press stop while recording, we define the loop length but go silent
    if (currentState.load() == State::Recording)
    {
        loopLengthSamples = playbackPosition;
        playbackPosition = 0;
    }

    if (loopLengthSamples > 0)
    {
        currentState.store(State::Stopped);
        playbackPosition = 0;                       // Optional: rewind on stop
    }
}

void LoopTrack::setLoopFromMix(const juce::AudioBuffer<float>& mixedBuffer, int length, int startOffset, juce::int64 startGlobalSample)
{
    if (length <= 0 || length > loopBuffer.getNumSamples()) return;

    saveUndo();

    loopBuffer.clear();
    for (int ch = 0; ch < juce::jmin(loopBuffer.getNumChannels(), mixedBuffer.getNumChannels()); ++ch)
        loopBuffer.copyFrom(ch, 0, mixedBuffer, ch, 0, length);

    loopLengthSamples = length;
    playbackPosition = 0;
    recordedSamplesCurrent = length;
    recordingStartOffset = startOffset;
    recordingStartGlobalSample = startGlobalSample;
    currentState.store(State::Playing);

    LOG("LoopTrack::setLoopFromMix | len=" + juce::String(length) + " offset=" + juce::String(startOffset) + " globalSample=" + juce::String(startGlobalSample));
}

void LoopTrack::overdubFromBuffer(const juce::AudioBuffer<float>& inputBuffer, int inputLength, juce::int64 inputStartGlobalSample)
{
    if (loopLengthSamples <= 0 || inputLength <= 0) return;

    saveUndo();

    // Compute where in our loop the input audio starts
    juce::int64 elapsed = inputStartGlobalSample - recordingStartGlobalSample;
    if (elapsed < 0) elapsed = 0;
    int writeStart = static_cast<int>(elapsed % loopLengthSamples);

    // Add input audio on top of existing loop buffer, with wrapping
    int numCh = juce::jmin(loopBuffer.getNumChannels(), inputBuffer.getNumChannels());
    for (int ch = 0; ch < numCh; ++ch)
    {
        int remaining = inputLength;
        int srcOffset = 0;
        int dstPos = writeStart;

        while (remaining > 0)
        {
            int toEnd = loopLengthSamples - dstPos;
            int chunk = juce::jmin(remaining, toEnd);
            loopBuffer.addFrom(ch, dstPos, inputBuffer, ch, srcOffset, chunk);
            srcOffset += chunk;
            dstPos += chunk;
            if (dstPos >= loopLengthSamples) dstPos = 0;
            remaining -= chunk;
        }
    }

    LOG("LoopTrack::overdubFromBuffer | inputLen=" + juce::String(inputLength) +
        " writeStart=" + juce::String(writeStart) + " loopLen=" + juce::String(loopLengthSamples));
}

//==============================================================================
// Audio Processing Implementation

void LoopTrack::handleRecording(const juce::AudioBuffer<float>& inputBuffer, int numSamples, int startWritePos)
{
    // In 'Recording' state, we are defining the loop length.
    // We just write linearly into the buffer.

    // Safety check: don't overflow the max allocated buffer
    if (startWritePos + numSamples >= loopBuffer.getNumSamples())
    {
        // Handle buffer overflow (auto-finish loop or stop)
        setPlaying();
        // handlePlayback might fail if we don't have loopLength set, but setPlaying sets it if we were linear recording...
        // But for slave tracks, we shouldn't hit this unless the master loop is huge.
        return;
    }

    // Copy input to loop buffer
    for (int channel = 0; channel < juce::jmin(inputBuffer.getNumChannels(), loopBuffer.getNumChannels()); ++channel)
    {
        loopBuffer.copyFrom(channel, startWritePos, inputBuffer, channel, 0, numSamples);
    }
}

void LoopTrack::handlePlayback(juce::AudioBuffer<float>& outputBuffer, int numSamples, int startReadPos, int loopEndRes, bool shouldBeSilent)
{
    if (loopEndRes <= 0)
        return;

    // If shouldBeSilent is true, we just don't add to output, but we assume the caller handles position tracking?
    // Wait, handlePlayback computes position locally but doesn't store it back if we pass 'readPos' by value.
    // 'readPos' is likely currentPlaybackPos (Global) or local.
    // In processBlock, we pass 'readPos'.
    // If we are master, we update Global Playback Position? No, PluginProcessor updates Global.
    // processBlock updates playbackPosition.

    if (shouldBeSilent)
        return;

    if (isMuted.load())
        return;

    float currentGain = gain.load();

    // During progressive replace, read from the source buffer (complete correct audio)
    // so there's no discontinuity between replaced and unreplaced regions.
    const juce::AudioBuffer<float>& readBuf =
            (mReplace.active && mReplace.source) ? *mReplace.source : loopBuffer;

    // Circular buffer read
    int samplesToDo = numSamples;
    int currentOutputOffset = 0;
    int localReadPos = startReadPos;

    while (samplesToDo > 0)
    {
        // Safety wrap
        if (localReadPos >= loopEndRes)
            localReadPos = 0;

        int samplesToEnd = loopEndRes - localReadPos;
        int chunk = juce::jmin(samplesToDo, samplesToEnd);

        if (chunk <= 0)
            break;

        // Add loop content to main output (Summing)
        for (int channel = 0; channel < juce::jmin(outputBuffer.getNumChannels(), readBuf.getNumChannels()); ++channel)
        {
            outputBuffer.addFrom(channel, currentOutputOffset, readBuf, channel, localReadPos, chunk, currentGain);
        }

        currentOutputOffset += chunk;
        localReadPos += chunk;
        samplesToDo -= chunk;

        // Wrap around
        if (localReadPos >= loopEndRes)
            localReadPos = 0;
    }
}

void LoopTrack::captureSidechain(const juce::AudioBuffer<float>& sidechainBuffer, int numSamples, int startWritePos, int loopEndRes)
{
    if (loopEndRes <= 0) return;

    int samplesToDo = numSamples;
    int srcOffset = 0;
    int localPos = startWritePos;

    while (samplesToDo > 0)
    {
        if (localPos >= loopEndRes)
            localPos = 0;

        int samplesToEnd = loopEndRes - localPos;
        int chunk = juce::jmin(samplesToDo, samplesToEnd);
        if (chunk <= 0) break;

        for (int ch = 0; ch < juce::jmin(sidechainBuffer.getNumChannels(), fxCaptureBuffer.getNumChannels()); ++ch)
        {
            fxCaptureBuffer.copyFrom(ch, localPos, sidechainBuffer, ch, srcOffset, chunk);
        }

        srcOffset += chunk;
        localPos += chunk;
        samplesToDo -= chunk;
        fxCaptureSamplesWritten += chunk;

        if (localPos >= loopEndRes)
            localPos = 0;
    }
}

void LoopTrack::applyFxReplace()
{
    if (loopLengthSamples <= 0) return;
    if (fxCaptureSamplesWritten < loopLengthSamples) return;

    saveUndo();

    for (int ch = 0; ch < juce::jmin(loopBuffer.getNumChannels(), fxCaptureBuffer.getNumChannels()); ++ch)
    {
        loopBuffer.copyFrom(ch, 0, fxCaptureBuffer, ch, 0, loopLengthSamples);
    }

    fxCaptureSamplesWritten = 0;
    LOG("FX Replace applied | loopLen=" + juce::String(loopLengthSamples));
}

void LoopTrack::handleOverdub(juce::AudioBuffer<float>& outputBuffer, const juce::AudioBuffer<float>& inputBuffer, int numSamples, int startReadPos, int loopEndRes, bool shouldBeSilent)
{
    if (loopEndRes <= 0) return;

    // Overdub = Playback existing + Write new input on top

    // Cache atomic values
    bool muted = isMuted.load();
    float currentGain = gain.load();

    // During progressive replace, read from the source buffer
    const juce::AudioBuffer<float>& readBuf =
            (mReplace.active && mReplace.source) ? *mReplace.source : loopBuffer;

    // If effective silence is forced (e.g. valid loop but soloed out), we treat as muted output
    if (shouldBeSilent) muted = true;

    int samplesToDo = numSamples;
    int currentOffset = 0;
    int localPos = startReadPos;

    while (samplesToDo > 0)
    {
        // Safety wrap
        if (localPos >= loopEndRes)
            localPos = 0;

        int samplesToEnd = loopEndRes - localPos;
        int chunk = juce::jmin(samplesToDo, samplesToEnd);

        if (chunk <= 0) break;

        for (int channel = 0; channel < juce::jmin(outputBuffer.getNumChannels(), loopBuffer.getNumChannels()); ++channel)
        {
            // 1. Output the existing loop audio (if not muted)
            if (!muted)
            {
                outputBuffer.addFrom(channel, currentOffset, readBuf, channel, localPos, chunk, currentGain);
            }

            // 2. Input -> Add to Storage (Constructive interference / Summing)
            loopBuffer.addFrom(channel, localPos, inputBuffer, channel, currentOffset, chunk);
        }

        currentOffset += chunk;
        localPos += chunk;
        samplesToDo -= chunk;

        if (localPos >= loopEndRes)
            localPos = 0;
    }
}

void LoopTrack::beginProgressiveReplace(const juce::AudioBuffer<float>* source, int length,
                                        int startOffset, juce::int64 startGlobal)
{
    if (!source || length <= 0 || length > loopBuffer.getNumSamples()) return;

    saveUndo();

    mReplace.source    = source;
    mReplace.length    = length;
    mReplace.cursor    = 0;
    mReplace.remaining = length;
    mReplace.active    = true;

    // Update metadata immediately so playback wraps at the new length
    loopLengthSamples          = length;
    playbackPosition           = 0;
    recordedSamplesCurrent     = length;
    recordingStartOffset       = startOffset;
    recordingStartGlobalSample = startGlobal;

    if (currentState.load() == State::Empty)
        currentState.store(State::Playing);

    LOG("beginProgressiveReplace | len=" + juce::String(length));
}

void LoopTrack::processReplaceChunk(int playheadPos, int blockSize)
{
    if (!mReplace.active || !mReplace.source) return;

    int numCh = juce::jmin(loopBuffer.getNumChannels(), mReplace.source->getNumChannels());
    int len   = mReplace.length;

    auto copyRegion = [&](int startPos, int count) {
        int pos = startPos;
        int rem = count;
        while (rem > 0) {
            if (pos >= len) pos -= len;
            int toEnd = len - pos;
            int chunk  = juce::jmin(rem, toEnd);
            for (int ch = 0; ch < numCh; ++ch)
                loopBuffer.copyFrom(ch, pos, *mReplace.source, ch, pos, chunk);
            pos += chunk;
            rem -= chunk;
        }
    };

    // Sequential fill only – safe because playback reads from mReplace.source,
    // not from loopBuffer. No read/write conflict possible.
    int budget = juce::jmin(blockSize * 16, mReplace.remaining);
    if (budget > 0)
    {
        copyRegion(mReplace.cursor, budget);
        mReplace.cursor    = (mReplace.cursor + budget) % len;
        mReplace.remaining -= budget;
    }

    if (mReplace.remaining <= 0)
    {
        mReplace.active = false;
        mReplace.source = nullptr;
        LOG("Progressive replace complete");
    }
}

void LoopTrack::applyCrossfade(juce::AudioBuffer<float>& buffer, int loopLength, int fadeSamples)
{
    if (loopLength <= 0 || fadeSamples <= 0) return;
    fadeSamples = juce::jmin(fadeSamples, loopLength / 2);

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        auto* data = buffer.getWritePointer(ch);

        // Crossfade: blend the end of the loop into the beginning
        // so that when the loop wraps around there is no discontinuity.
        for (int i = 0; i < fadeSamples; ++i)
        {
            float fadeIn  = (float)i / (float)fadeSamples;           // 0 → 1
            float fadeOut = 1.0f - fadeIn;                           // 1 → 0

            float headSample = data[i];
            float tailSample = data[loopLength - fadeSamples + i];

            // Blend: beginning fades in from tail, tail fades out into beginning
            data[i]                          = headSample * fadeIn + tailSample * fadeOut;
            data[loopLength - fadeSamples + i] = tailSample * fadeOut + headSample * fadeIn;
        }
    }
}