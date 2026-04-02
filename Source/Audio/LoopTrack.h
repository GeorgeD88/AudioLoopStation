//
// Created by Vincewa Tran on 1/28/26.
// Represents a single independent loop track with a state machine
// - Integrate the CircularBuffer + volume/pan + record/play state
// - Where audio data gets processed.
// - DSP and effects chain will eventually go here as well.
//
#pragma once
// JUCE modules
#include "juce_audio_basics/juce_audio_basics.h"
#include "juce_audio_processors/juce_audio_processors.h"
#include "juce_dsp/juce_dsp.h"
// Gin Modules
#include "gin_dsp/gin_dsp.h"
// Project includes
#include "SyncEngine.h"
#include "../Utils/TrackConfig.h"

/**
 * Represents a single loop track with a state machine and circular buffer
 */
class LoopTrack {
public:
    // Recording state machine
    enum class State {
        Empty,                  // No loop recorded
        Recording,              // Recording the initial loop (defines length)
        Playing,                // Playing back the recorded loop
        Overdubbing,            // Playing back + mixing new input
        Stopped                 // Loop exists but is silent
    };

    LoopTrack();
    ~LoopTrack();

    void prepareToPlay(double sampleRate, int samplesPerBlock);

    // === Audio processing ===
    /** PROCESS: Main audio callback.
        - outputBuffer: The main mix to add our loop audio to.
        - inputBuffer: The incoming audio to record/overdub.
        - globalTotalSamples: The total monotonic sample count since transport start (for global sync).
        - isMasterTrack: If true, this track is defining the master loop length.
        - masterLoopLength: The length of the master loop in samples.
        - anySoloActive: If true, track only plays if it is soloed. */
    void processBlock(juce::AudioBuffer<float>& outputBuffer, const juce::AudioBuffer<float>& inputBuffer,
                      const juce::AudioBuffer<float>& sidechainBuffer,
                      juce::int64 globalTotalSamples, bool isMasterTrack, int masterLoopLength, bool anySoloActive);

    /** RESET: Clears the buffer and state. */
    void clear();

    // === State Controls ===
    void setRecording();
    void setOverdubbing();
    void setPlaying();
    void stop();

    void setVolume();

    void setVolume(float newVolume) { gain.store(newVolume); }
    void setMuted(bool shouldBeMuted) { isMuted.store(shouldBeMuted); }
    void setSolo(bool shouldBeSolo) { isSolo.store(shouldBeSolo); }
    // FX Replace: one-shot apply of captured sidechain audio
    void applyFxReplace();
    bool isFxCaptureReady() const { return loopLengthSamples > 0 && fxCaptureSamplesWritten >= loopLengthSamples; }

    float getTargetMultiplier() const { return getTargetMultiplier(); }

    State getState() const { return currentState.load(); }
    bool hasLoop() const { return loopLengthSamples > 0; }
    bool getLoopLengthSamples() const { return loopLengthSamples; }
    bool getSolo() const { return isMuted.load(); }
    bool isMutedState() const { return isMuted.load(); }

private:
    // Progressive replace state
    struct ProgressiveReplace {
        const juce::AudioBuffer<float>* source = nullptr;
        int length = 0;
        int cursor = 0;
        int remaining = 0;
        bool active = false;
    };
    ProgressiveReplace mReplace;

    // Audio Data
    juce::AudioBuffer<float> loopBuffer;
    juce::AudioBuffer<float> undoBuffer; // Buffer for undo state
    juce::AudioBuffer<float> fxCaptureBuffer; // Staging buffer for FX Replace
    int fxCaptureSamplesWritten = 0;
    double trackSampleRate = 44100.0;

    // Playback/Recording Logic
    std::atomic<State> currentState { State::Empty };
    std::atomic<float> gain { 1.0f };
    std::atomic<bool> isMuted { false };
    std::atomic<bool> isSolo { false }; // New Solo state

    int playbackPosition = 0;       // Current read/write head position
    int loopLengthSamples = 0;      // Defined after first recording finishes

    // SYNC: position in the master cycle where recording started (for the slave track)
    int recordingStartOffset = 0;   // Décalage de départ pour la synchronisation
    juce::int64 recordingStartGlobalSample = 0; // Absolute global sample count at recording start

    // Undo State
    int undoLoopLengthSamples = 0;
    bool hasUndo = false;
    void saveUndo();

    // Configuration
    float targetMultiplier = 1.0f; // How many bars (relative to master) to record

    // Allocating 5 minutes per track by default to avoid reallocation on audio thread
    const int maxLoopLengthSeconds = 300;

    // For fixed length recording
    int recordedSamplesCurrent = 0;

    // Helpers
    void handleRecording(const juce::AudioBuffer<float>& inputBuffer, int numSamples, int startWritePos);
    void handlePlayback(juce::AudioBuffer<float>& outputBuffer, int numSamples, int startReadPos, int loopEndRes, bool shouldBeSilent);
    void handleOverdub(juce::AudioBuffer<float>& outputBuffer, const juce::AudioBuffer<float>& inputBuffer, int numSamples, int startReadPos, int loopEndRes, bool shouldBeSilent);
    void captureSidechain(const juce::AudioBuffer<float>& sidechainBuffer, int numSamples, int startWritePos, int loopEndRes);



    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LoopTrack)
};


