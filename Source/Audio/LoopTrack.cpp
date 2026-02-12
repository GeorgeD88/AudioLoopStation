//
// Created by Vincewa Tran on 1/28/26.
//

#include "LoopTrack.h"

LoopTrack::LoopTrack(int id) : trackId(id), currentVolumeDb(juce::Decibels::gainToDecibels(TrackConfig::DEFAULT_VOLUME_DB)),
currentPan(TrackConfig::DEFAULT_PAN)
{
}

LoopTrack::~LoopTrack() {
    releaseResources();
}

void LoopTrack::prepareToPlay(double sampleRate, int samplesPerBlock, int numChannels) {

    // Setup DSP Components
    volumeSmoother.reset(sampleRate, 0.05);
    volumeSmoother.setCurrentAndTargetValue(
            juce::Decibels::decibelsToGain(currentVolumeDb.load()));

    // Prepare circular buffer (stereo by default)
    buffer.prepareToPlay(sampleRate, numChannels, TrackConfig::MAX_LOOP_LENGTH_SECONDS);
    undoBuffer.prepareToPlay(sampleRate, numChannels, TrackConfig::MAX_LOOP_LENGTH_SECONDS);

    inputCopy.setSize(numChannels, samplesPerBlock);
    playbackBuffer.setSize(numChannels, samplesPerBlock);
}

void LoopTrack::releaseResources() {
    buffer.releaseResources();
    inputCopy.setSize(0, 0);
    playbackBuffer.setSize(0, 0);
}

void LoopTrack::processBlock(const juce::AudioBuffer<float> &input, juce::AudioBuffer<float> &output, bool isSoloActive) {

    const int numSamples = input.getNumSamples();
    State state = currentState.load();
    // === Early exit if there's nothing for track to do ===
    if (state == State::Stopped || (state == State::Empty && state != State::Recording))
    {
        return;
    }

    // === Solo/Mute Logic ===
    bool shouldBeSilent = false;
    if (isSoloActive) {
        shouldBeSilent = soloState.load();
    } else {
        shouldBeSilent = muteState.load();
    }

    // === Recording ===
    if (state == State::Recording) {
        inputCopy.makeCopyOf(input, true);
        buffer.write(inputCopy);
        playbackPosition += numSamples;
    }

    // === Playback ===
    if (state == State::Playing && loopLengthSamples > 0 && !shouldBeSilent)
    {
        // Get the current read position
        int currentPos = buffer.getReadPosition();

        // Check if we need to reset to stay within loop bounds
        if (currentPos >= loopLengthSamples) {
            // Resets read positon to 0
            buffer.resetPlayback();
        }

        // Read audio into playback buffer
        playbackBuffer.clear();
        if (buffer.read(playbackBuffer)) {

            // === Reverse ===
            if (reverseState.load()) {
                applyReverse(playbackBuffer);
            }

            // === Slip ===
            if (slipOffset.load() != 0) {
                applySlip(playbackBuffer);
            }

            // Apply DSP (volume, pan)
            applyDspProcessing(playbackBuffer);

            // Add to output
            float gain = juce::Decibels::decibelsToGain(currentVolumeDb.load());

            for (int ch = 0; ch < output.getNumChannels(); ++ch) {
                int sourceCh = ch % playbackBuffer.getNumChannels();
                output.addFrom(ch, 0, playbackBuffer, sourceCh, 0, numSamples, gain);
            }

        }
    }

    // === Input Monitoring ===
    if (isArmedForRecording.load() && !isRecordingActive.load()) {
        constexpr float monitorGain = 0.5f;
        for (int ch = 0; ch < output.getNumChannels(); ++ch) {
            int sourceCh = ch % input.getNumChannels();
            output.addFrom(ch, 0, input, sourceCh, 0, numSamples, monitorGain);
        }
    }
}

void LoopTrack::applyDspProcessing(juce::AudioBuffer<float> &bufferToProcess) {

    // TODO: apply volume smoothing, apply panning for stereo buffers
    const auto numSamples = bufferToProcess.getNumSamples();
    const auto numChannels = bufferToProcess.getNumChannels();

    // Apply volume smoothing
    auto targetGain = juce::Decibels::decibelsToGain(currentVolumeDb.load());
    volumeSmoother.setTargetValue(targetGain);

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto startGain = volumeSmoother.getCurrentValue();
        volumeSmoother.skip(numSamples);
        auto endGain = volumeSmoother.getCurrentValue();

        bufferToProcess.applyGainRamp(ch, 0, numSamples, startGain, endGain);
    }

    // Apply panning for stereo buffers
    if(numChannels == 2 && std::abs (currentPan) > 0.001f)
    {
        const auto panRadians = (currentPan + 1.0f) * juce::MathConstants<float>::pi / 4.0f;
        const auto leftGain = std::cos (panRadians);
        const auto rightGain = std::sin (panRadians);

        bufferToProcess.applyGain(0,0, numSamples, leftGain);
        bufferToProcess.applyGain(1,1, numSamples, rightGain);
    }

}

void LoopTrack::armForRecording(bool isArmed) {
    isArmedForRecording.store(isArmed);

    if (!isArmed && isRecordingActive.load())
    {
        stopRecording();
    }
}

void LoopTrack::startRecording() {

    if(!isArmedForRecording.load()) return;

    if (buffer.isEmpty())
    {
        buffer.clear();
        buffer.resetPlayback();
    }

    isRecordingActive.store(true);

    if(!isPlaybackActive.load())
    {
        startPlayback();
    }
}

void LoopTrack::stopRecording() {
    isRecordingActive.store (false);

    if(buffer.isEmpty())
    {
        stopPlayback();
    }
}

void LoopTrack::startPlayback() {

    if (buffer.isEmpty()) return;

    isPlaybackActive.store(true);
}

void LoopTrack::stopPlayback() {

    isPlaybackActive.store(false);
    buffer.resetPlayback();
}

void LoopTrack::setVolumeDb(float newVolumeDb) {

    currentVolumeDb = juce::jlimit(TrackConfig::MIN_VOLUME_DB,
                                   TrackConfig::MAX_VOLUME_DB,
                                   newVolumeDb);
}

void LoopTrack::setPan(float newPan) {
    currentPan = juce::jlimit(TrackConfig::MIN_PAN,
                              TrackConfig::MAX_PAN,
                              newPan);
}

void LoopTrack::setMute (bool shouldMute) {
    muteState = shouldMute;
}

void LoopTrack::setSolo(bool shouldSolo) {
    soloState = shouldSolo;
}

void LoopTrack::clear() {
    stopRecording();
    stopPlayback();

    buffer.clear();

    // Reset all states
    currentVolumeDb.store(TrackConfig::DEFAULT_VOLUME_DB);
    currentPan.store(TrackConfig::DEFAULT_PAN);
    muteState.store(false);
    soloState.store(false);
    reverseState.store(false);
    slipOffset.store(0);

    volumeSmoother.setCurrentAndTargetValue(
            juce::Decibels::decibelsToGain(currentVolumeDb.load()));

    currentState.store(State::Empty);
    isArmedForRecording.store(false);
    loopLengthSamples = 0;
    playbackPosition = 0;
    recordingStartGlobalSample = 0;
}

void LoopTrack::applyReverse(juce::AudioBuffer<float> &buffer) {
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch) {
        auto* data = buffer.getWritePointer(ch);
        std::reverse(data, data + buffer.getNumSamples());
    }
}

void LoopTrack::applySlip(juce::AudioBuffer<float>& buffer) {
    int offset = slipOffset.load();
    if (offset == 0) return;

    const int numSamples = buffer.getNumSamples();
    offset = offset % numSamples;
    if (offset < 0) offset += numSamples;
    if (offset == 0) return;

    // Create temporary copy
    juce::AudioBuffer<float> temp(buffer.getNumChannels(), numSamples);
    temp.makeCopyOf(buffer);

    // Apply rotation
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch) {
        auto* dest = buffer.getWritePointer(ch);
        auto* src = temp.getReadPointer(ch);

        // Copy second part to beginning
        std::memcpy(dest, src + offset, (numSamples - offset) * sizeof(float));
        // Copy first part to end
        std::memcpy(dest + (numSamples - offset), src, offset * sizeof(float));
    }
}