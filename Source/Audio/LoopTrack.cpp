//
// Created by Vincewa Tran on 1/28/26.
// Represents a single independent loop track with a state machine
// - Integrate the CircularBuffer + volume/pan + record/play state
// - Where audio data gets processed.
// - DSP and effects chain will eventually go here as well.

#include "LoopTrack.h"
#include "DebugLogger.h"


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
 * @param numChannels - Number of audio channels (1 for mono, 2 for stereo)
 *
 * Called on the audio thread during prepareToPlay()
 * Allocates buffers and initializes DSP components
 */
void LoopTrack::prepareToPlay(double sampleRate, int samplesPerBlock) {

    trackSampleRate = sampleRate;

    // Allocate the buffer for the maximum supported loop time
    int totalSamples = static_cast<int>(sampleRate * maxLoopLengthSeconds);

    // Assuming stereo for now
    loopBuffer.setSize(2, totalSamples);
    loopBuffer.clear();

    fxCaptureBuffer.setSize(2, totalSamples);
    fxCaptureBuffer.clear();
}

void LoopTrack::processBlock(juce::AudioBuffer<float> &outputBuffer, const juce::AudioBuffer<float> &inputBuffer,
                             const juce::AudioBuffer<float> &sidechainBuffer, juce::int64 globalTotalSamples,
                             bool isMasterTrack, int masterLoopLength, bool anySoloActive) {

    const int numSamples = outputBuffer.getNumSamples();
    State state = currentState.load();

    /* If stopped or empty, we generally don't output sound,
     * providing we aren't currently recording the first pass. */
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
        if (isMutedState()) shouldBeSilent = true;
    }

    // Force mute for playback if needed

    // Auto-finish fixed length recording (Slave Tracks)
    if (state == State::Recording && !isMasterTrack)
    {
        // Simple check: if we exceed the loop length, switch to playing
        float mult = targetMultiplier;
        if (mult < 1.0f / 64.0f) mult = 1.0f / 64.0f;
        if (mult > 64.0f) mult = 64.0f;

        int targetLen = static_cast<int>((float)masterLoopLength * mult);
        if (targetLen < 1) targetLen = 1;

        if (recordedSamplesCurrent >= targetLen)
        {
            // Define the loop length before passing in Playing
            loopLengthSamples = targetLen;
            LOG("SLAVE REC FINISHED | recorded=" + juce::String(recordedSamplesCurrent) +
                " loopLen=" + juce::String(loopLengthSamples) +
                " mult=" + juce::String(mult) +
                " offset=" + juce::String(recordingStartOffset));

            setPlaying();
            state = State::Playing;

            // Synchronisation is handled automatically in the Playing section
            // calculated with recordingStartOffset
        }
    }

    int writePos = 0;
    int readPos = 0;
    int currentLoopLength = 0;

    if (isMasterTrack)
    {
        if (state == State::Recording)
        {
            // Linear recording
            writePos = playbackPosition;
            currentLoopLength = loopBuffer.getNumSamples(); // Prevent overflow
        }
        else
        {
            // Master Playing/Overdubbing - use global transport provided by Processor
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
        // Slave track
        if (state == State::Recording)
        {
            // Recoding a slave track
            // Record linearly into the buffer, starting from pos 0
            // BUT we store the position within the master cycle
            // If this is the first recorded sample
            // capture the starting position within the master cycle
            if (recordedSamplesCurrent == 0 && masterLoopLength > 0)
            {
                // Calculate the actual position in the master cycle
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

            // Calculate the target length using the multiplier
            float mult = targetMultiplier;
            if (mult < 1.0f / 64.0f) mult = 1.0f / 64.0f;
            if (mult > 64.0f) mult = 64.0f;
            int targetLen = static_cast<int>((float)masterLoopLength * mult);
            if (targetLen < 1) targetLen = 1;

            if (loopLengthSamples != targetLen)
                loopLengthSamples = targetLen;
        }
        else
        {
            // Slave Playing/Overdubbing
            // Use the absolute time elapsed since the start of recording
            // Works for loops that are both shorter and longer than the master

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
                currentLoopLength = masterLoopLength;
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


}