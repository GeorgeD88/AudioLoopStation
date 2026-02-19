//
// Created by Vincewa Tran on 1/28/26.
// Represents a single independent loop track with a state machine
// - Integrate the CircularBuffer + volume/pan + record/play state
// - Where audio data gets processed.
// - DSP and effects chain will eventually go here as well.

#include "LoopTrack.h"

LoopTrack::LoopTrack(int id) : trackId(id),
recordingBuffer(2, 1024),
undoBuffer(2, 1024) {

    // Configure Gin's SamplePlayer
    player.setLooping(true);
    player.setCrossfadeSamples(TrackConfig::DEFAULT_BUFFER_SIZE);       // Buffer size of 256

}

LoopTrack::~LoopTrack() {
    releaseResources();
}

/**
 * Releases audio resources when playback stops
 */
void LoopTrack::releaseResources() {
    recordingBuffer.setSize(0, 0);
    undoBuffer.setSize(0, 0);
    player.reset();
}

/**
 * Prepares the track for audio processing
 * @param sampleRate - Current audio sample rate (e.g., 48000 Hz)
 * @param samplesPerBlock - Buffer size for each process callback
 * @param numChannels - Number of audio channels (1 for mono, 2 for stereo)
 *
 * Called on the audio thread during prepareToPlay()
 * Allocates buffers and initializes DSP components
 */
void LoopTrack::prepareToPlay(double sr, int /*samplesPerBlock*/, int numChannels) {

    sampleRate = sr;

    // Resize buffers for actual sample rate
    int bufferSize = static_cast<int>(sampleRate * TrackConfig::MAX_LOOP_LENGTH_SECONDS);
    recordingBuffer.setSize(numChannels, bufferSize);
    undoBuffer.setSize(numChannels, bufferSize);

    // Reset buffers
    recordingBuffer.reset();
    undoBuffer.reset();

    // Configure SamplePlayer (GIN)
    player.setPlaybackSampleRate(sampleRate);
    player.reset();
    playerLoaded = false;

    // 50ms volume fade
    volumeSmoother.setSampleRate(sampleRate);
    volumeSmoother.setTime(TrackConfig::VOLUME_FADE_SECONDS);
    volumeSmoother.setValueUnsmoothed(
            juce::Decibels::decibelsToGain(currentVolumeDb.load()));

    // 30ms pan fade
    panSmoother.setSampleRate(sampleRate);
    panSmoother.setTime(TrackConfig::PAN_FADE_SECONDS);
    panSmoother.setValueUnsmoothed(currentPan.load());
}

/**
 * Main audio processing callback
 * @param input - Live input from audio interface
 * @param output - Buffer to fill with processed track audio
 * @param isSoloActive - Whether any track in the project is soloed
 *
 * Called on the real-time audio thread - must be lock-free and non-blocking
 * Handles three main operations:
 * 1. Recording - Writes input to circular buffer
 * 2. Playback - Reads from buffer with DSP processing
 * 3. Input Monitoring - Passes live input when armed
 */
void LoopTrack::processBlock(const juce::AudioBuffer<float> &input,
                             juce::AudioBuffer<float> &output,
                             const SyncEngine& syncEngine) {

    const int numSamples = input.getNumSamples();
    State state = currentState.load();

    // === Recording ===
    if (state == State::Recording && isRecordingActive.load()) {
        // Write to FIFO
        if (!recordingBuffer.write(input)){
            // Buffer is full so overwrite the oldest sampled audio
            recordingBuffer.ensureFreeSpace(numSamples);
            recordingBuffer.write(input);
        }

        // If this is the initial input/recording in the loop
        if (loopLengthSamples.load() == 0) {
            int recorded = recordingBuffer.getNumReady();
            int samplesPerBeat = syncEngine.getSamplesPerBeat();

            if (samplesPerBeat > 0) {
                // Align to nearest beat boundary
                int alignedLength = ((recorded + samplesPerBeat -1) /
                        samplesPerBeat) * samplesPerBeat;
                setLoopLength(alignedLength);
            }
        }
    }

    // === Playback ===
    bool shouldPlay = (state == State::Playing || state == State::Recording)
            && hasLoop()
            && !muteState.load()
            && !soloState.load();

    if (shouldPlay) {
        // If it's the first time hitting play, load the buffer into Gin's SamplePlayer
        if (!playerLoaded && hasLoop()) {
            loadRecordingToPlayer();
            player.play();
            playerLoaded = true;
        }

        // Get temporary buffer - NO ALLOCATION!
        gin::ScratchBuffer playerOutput(output.getNumChannels(), numSamples);

        // SamplePlayer handles crossfading, looping, and position tracking
        player.processBlock(playerOutput);

        // Apply Effects
        // Slip
        if (reverseState.load()) {
            applyReverse(playerOutput);
        }

        // Offset sample from the grid
        if (slipOffset.load() != 0) {
            applySlip(playerOutput);
        }

        applyDspProcessing(playerOutput);

        // Mix into output
        for (int ch = 0; ch < output.getNumChannels(); ++ch) {
            int sourceCh = ch % playerOutput.getNumChannels();
            output.addFrom(ch,
                           0,
                           playerOutput,
                           sourceCh,
                           0,
                           numSamples);
        }

        // Handle loop wrap
        if (recordingBuffer.getNumReady() < numSamples * 2) {
            recordingBuffer.reset();
        }
    }

    // === Input Monitoring ===
    // When armed but not recording, pass live input through at reduced gain
    if (isArmedForRecording.load() && !isRecordingActive.load()) {
        constexpr float monitorGain = 0.5f;
        for (int ch = 0; ch < output.getNumChannels(); ++ch) {
            int sourceCh = ch % input.getNumChannels();
            output.addFrom(ch,
                           0,
                           input,
                           sourceCh,
                           0,
                           numSamples,
                           monitorGain);
        }
    }
}

/**
 * Applies volume and pan DSP to an audio buffer
 * @param bufferToProcess - Audio buffer to process in-place
 *
 * Features:
 * - Linear ramp for volume changes (avoids clicks/pops)
 * - Equal-power panning for stereo signals
 */
void LoopTrack::applyDspProcessing(juce::AudioBuffer<float> &bufferToProcess) {

    const int numSamples = bufferToProcess.getNumSamples();
    const int numChannels = bufferToProcess.getNumChannels();

    // Apply volume smoothing
    // Convert dB to linear gain and apply ramp
    float targetGain = juce::Decibels::decibelsToGain(currentVolumeDb.load());
    float targetPan = currentPan.load();

    volumeSmoother.setValue(targetGain);
    panSmoother.setValue(targetPan);

    // Apply with QuadraticOutEasing
    for (int s = 0; s < numSamples; ++s) {
        float gain = volumeSmoother.getNextValue();
        float pan = panSmoother.getNextValue();

        if (numChannels == 2) {
            // Stereo
            float leftGain, rightGain;
            if (pan <= 0.0f) {
                leftGain = gain;
                rightGain = gain * (1.0f + pan);
            } else {
                leftGain = gain * (1.0f - pan);
                rightGain = gain;
            }

            bufferToProcess.getWritePointer(0)[s] *= leftGain;
            bufferToProcess.getWritePointer(1)[s] *= rightGain;
        } else {
            // Mono - only apply volume
            for (int ch = 0; ch < numChannels; ++ch) {
                bufferToProcess.getWritePointer(ch)[s] *= gain;
            }
        }
    }
}

void LoopTrack::armForRecording(bool isArmed) {
    isArmedForRecording.store(isArmed);
}

void LoopTrack::startRecording(juce::int64 globalSample) {

    if(!isArmedForRecording.load()) return;

    recordingStartGlobalSample.store(globalSample);
    isRecordingActive.store(true);
    currentState.store(State::Recording);

    // Clear previous loop
    recordingBuffer.reset();
    loopLengthSamples.store(0);
    player.reset();
}

void LoopTrack::stopRecording() {
    isRecordingActive.store (false);

    if (hasLoop()) {
        currentState.store(State::Playing);
        startPlayback();
    } else {
        currentState.store(State::Empty);
    }
}

void LoopTrack::startPlayback() {
    // Check if there's anything in the buffer
    if (!hasLoop()) return;

    // Play and update state
    isPlaybackActive.store(true);
    player.play();
    currentState.store(State::Playing);
}

void LoopTrack::stopPlayback() {

    isPlaybackActive.store(false);
    player.stop();
    recordingBuffer.reset();

    if (currentState.load() != State::Recording) {
        currentState.store(hasLoop() ? State::Stopped : State::Empty);
    }
}

void LoopTrack::stop() {
    stopRecording();
    stopPlayback();
}

/**
 * Clears all track data and resets to empty state
 * Safe to call from any thread
 */
void LoopTrack::clear() {
    stop();

    recordingBuffer.reset();
    undoBuffer.reset();

    // Reset all states
    currentVolumeDb.store(TrackConfig::DEFAULT_VOLUME_DB);
    currentPan.store(TrackConfig::DEFAULT_PAN);
    muteState.store(false);
    soloState.store(false);
    reverseState.store(false);
    slipOffset.store(0);

    // Reset smoothers
    volumeSmoother.setValueUnsmoothed(
            juce::Decibels::decibelsToGain(currentVolumeDb.load()));
    panSmoother.setValueUnsmoothed(currentPan.load());

    loopLengthSamples = 0;
    recordingStartGlobalSample.store(0);
    currentState.store(State::Empty);
    hasUndo = false;
    playerLoaded = false;
}

void LoopTrack::setLoopLength(int samples) {
    jassert(samples > 0);
    jassert(samples <= recordingBuffer.getNumReady());

    loopLengthSamples.store(samples);

    // Trim buffer if needed
    int currentReady = recordingBuffer.getNumReady();
    if (currentReady > samples) {
        recordingBuffer.pop(currentReady - samples);
    }
    loadRecordingToPlayer();
}

void LoopTrack::multiplyLoop() {
    int currentLength = loopLengthSamples.load();
    if (currentLength <= 0) return;

    saveUndo();

    int newLength = currentLength * 2;
    if (newLength <= recordingBuffer.getNumReady()) {
        setLoopLength(newLength);
    }
}

void LoopTrack::divideLoop() {
    int currentLength = loopLengthSamples.load();
    if (currentLength < 1024) return;

    saveUndo();

    int newLength = currentLength / 2;
    setLoopLength(newLength);
}

void LoopTrack::performUndo() {
    if (!hasUndo) return;

    // Restore from undo buffer
    recordingBuffer.reset();

    gin::ScratchBuffer temp(undoBuffer.getNumChannels(), undoLoopLength);
    if (undoBuffer.peek(temp, 0, undoLoopLength)) {
        recordingBuffer.write(temp);
        setLoopLength(undoLoopLength);
    }

    hasUndo = false;
}

// === DSP controls ===
void LoopTrack::setVolumeDb(float newVolumeDb) {
    currentVolumeDb.store(juce::jlimit(TrackConfig::MIN_VOLUME_DB,
                                   TrackConfig::MAX_VOLUME_DB,
                                   newVolumeDb));
}

void LoopTrack::setPan(float newPan) {
    currentPan.store(juce::jlimit(TrackConfig::MIN_PAN,
                              TrackConfig::MAX_PAN,
                              newPan));
}

void LoopTrack::setMute (bool shouldMute) { muteState.store(shouldMute); }
void LoopTrack::setSolo(bool shouldSolo) { soloState.store(shouldSolo); }
void LoopTrack::setReverse(bool reverse) { reverseState.store(reverse); }
void LoopTrack::setSlip(int samples) { slipOffset.store(samples); }



// === Private Helpers ===
void LoopTrack::loadRecordingToPlayer() {
    int loopLen = loopLengthSamples.load();
    if (loopLen <= 0) return;

    int channels = recordingBuffer.getNumChannels();

    // Get temporary buffer from pool
    gin::ScratchBuffer temp(channels, loopLen);

    // Peek at recording buffer
    if (recordingBuffer.peek(temp, 0, loopLen)) {
        // Load into gin::SamplePlayer
        player.setBuffer(temp, sampleRate);
    }
}

void LoopTrack::saveUndo() {
    int currentLen = loopLengthSamples.load();
    if (currentLen > 0) {
        undoBuffer.reset();

        gin::ScratchBuffer temp(recordingBuffer.getNumChannels(),currentLen);

        if (recordingBuffer.peek(temp, 0, currentLen)) {
            undoBuffer.write(temp);
            undoLoopLength = currentLen;
            hasUndo = true;
        }
    }
}

void LoopTrack::applyReverse(juce::AudioBuffer<float> &buffer) {
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch) {
        auto* data = buffer.getWritePointer(ch);
        std::reverse(data, data + buffer.getNumSamples());
    }
}

/**
 * Applies slip offset to the track's audio buffer
 * @param audioBuffer - Buffer to rotate
 * @param offset - Number of samples to shift (positive = later, negative = earlier)
 *
 * Implements circular buffer rotation for slip feature
 * Example: [1,2,3,4] with offset=2 becomes [3,4,1,2]
 */
void LoopTrack::applySlip(juce::AudioBuffer<float>& buffer) {

    int offset = slipOffset.load();
    if (offset == 0) return;

    const int numSamples = buffer.getNumSamples();
    offset = offset % numSamples;
    if (offset < 0) offset += numSamples;
    if (offset == 0) return;

    // Create temporary copy
    gin::ScratchBuffer temp(buffer);

    // Apply rotation
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch) {
        auto* dest = buffer.getWritePointer(ch);
        auto* src = temp.getReadPointer(ch);

        // Copy second part to beginning
        std::memcpy(dest,
                    src + offset,
                    static_cast<size_t>(numSamples - offset) * sizeof(float));
        // Copy first part to end
        std::memcpy(dest + (numSamples - offset),
                    src,
                    static_cast<size_t>(offset) * sizeof(float));
    }
}

juce::String LoopTrack::getStateString() const {
    switch (currentState.load()) {
        case State::Empty:          return "Empty";
        case State::Recording:      return "REC";
        case State::Playing:        return "Play";
        case State::Stopped:        return "Stop";
        default:                    return "unknown";
    }
}