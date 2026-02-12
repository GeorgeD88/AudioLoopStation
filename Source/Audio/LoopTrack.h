//
// Created by Vincewa Tran on 1/28/26.
// Represents a single independent loop track with a state machine
// - Integrate the CircularBuffer + volume/pan + record/play state
// - Where audio data gets processed.
// - DSP and effects chain will eventually go here as well.
//
#pragma once
#include "juce_audio_basics/juce_audio_basics.h"
#include "juce_audio_processors/juce_audio_processors.h"
#include "juce_dsp/juce_dsp.h"

// Project includes
#include "../Utils/TrackConfig.h"
#include "CircularBuffer.h"

class LoopTrack {
public:

    // Recording state machine
    enum class State {
        Empty,                  // No loop recorded
        Recording,              // Recording the initial loop
        Playing,                // Playing back the recorded loop
        Stopped                 // Loop exists but is silent
    };

    explicit LoopTrack(int trackId = TrackConfig::INVALID_TRACK_ID);        // -1 representing an inactive/invalid track
    ~LoopTrack();

    // === Audio processing ===
    void prepareToPlay(double sampleRate, int samplesPerBlock,
                       int numChannels = 2);
    void releaseResources();
    void processBlock(const juce::AudioBuffer<float>& input,
                      juce::AudioBuffer<float>& output,
                      bool isSoloActive);                       // for mute logic

    // === Transport Controls
    void armForRecording(bool armed);
    void startRecording();
    void stopRecording();
    void startPlayback();
    void stopPlayback();
    // TODO:: startOverdubbing(); and stopOverdubbing();
    void stop();

    // === DSP Controls (per Track)===
    void setVolumeDb(float volumeDb);
    void setPan(float pan);                                     // REQUIRED FEATURE 1 from OSU project page
    void setMute(bool muted);
    void setSolo(bool soloed);
    void setReverse(bool reverse);                              // REQUIRED FEATURE 2 from OSU project page
    void setSlip(int samples);                                  // REQUIRED FEATURE 3 from OSU project page

    // === Loop Manipulation
    void multiplyLoop();                                        // double loop length
    void divideLoop();                                          // halve loop length
    void performUndo();                                         // Undo last op
    void clear();                                               // Clear loop


    // === Getters (for UI and manager) ===
    State getState() const { return currentState.load(); }
    int getTrackId() const {return trackId; }
    int getLoopLengthSamples() const { return getLoopLengthSamples(); }
    bool hasLoop() const { return getLoopLengthSamples() > 0; }
    bool isEmpty() const { return buffer.isEmpty(); }
    bool isArmed() const { return isArmedForRecording.load(); }
    bool isMuted() const { return muteState.load(); }
    bool canUndo() const { return hasUndo; }

    // === Sync info (for manager to read/write) ===
    void setRecordingStartGlobalSample(juce::int64 sample) {
        recordingStartGlobalSample = sample;
    }
    juce::int64 getRecordingStartGlobalSample() const {
        return recordingStartGlobalSample;
    }

    void setTargetMultiplier(float mult) {
        targetMultiplier = juce::jlimit(1.0f/64.0f, 64.0f, mult);
    }
    float getTargetMultiplier() const { return targetMultiplier; }


private:
    // === Core components ===
    CircularBuffer buffer;
    CircularBuffer undoBuffer;
    const int trackId;

    // === States (atomic for thread safety) ===
    std::atomic<State> currentState {State::Empty };
    std::atomic<bool> isArmedForRecording { false };
    std::atomic<bool> isRecordingActive { false };
    std::atomic<bool> isPlaybackActive { false };

    // === Loop metadata ===
    int loopLengthSamples = 0;                          // Current loop length measured by sample rate
    int playbackPosition = 0;                           // Current playback position

    // === Sync ===
    juce::int64 recordingStartGlobalSample = 0;

    // === DSP ===
    juce::LinearSmoothedValue<float> volumeSmoother;
    std::atomic<float> currentVolumeDb { TrackConfig::DEFAULT_VOLUME_DB };
    std::atomic<float> currentPan { TrackConfig::DEFAULT_PAN };
    std::atomic<bool> muteState { false };
    std::atomic<bool> soloState { false };
    std::atomic<bool> reverseState { false };
    std::atomic<bool> slipOffset { false };

    // === Other features and Undo ===
    float targetMultiplier = 1.0f;                      // for variable length recording
    bool hasUndo = false;
    int undoLoopLengthSamples = 0;

    // === Temporary processing buffers ===
    juce::AudioBuffer<float> inputCopy;
    juce::AudioBuffer<float> playbackBuffer;

    // === Private Helpers ===
    void saveUndo();
    void applyDspProcessing(juce::AudioBuffer<float>& buffer);
    void applyCrossfade(int fadeSamples = 128);
    void applyReverse(juce::AudioBuffer<float>& buffer);  // Helper for reverse
    void applySlip(juce::AudioBuffer<float>& buffer);     // Helper for slip


    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LoopTrack);
};


