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
#include "CircularBuffer.h"

/**
 * Represents a single loop track with recording, playback, and DSP.
 *
 * Gin components used directly:
 * - gin::AudioFifo: Lock-free recording buffer
 * - gin::SamplePlayer: Professional playback engine
 * - gin::SmoothedValue: Click-free parameter changes
 * - gin::ScratchBuffer: Temporary buffers from pool (no allocations!)
 */
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
                      const SyncEngine& syncEngine);                       // for mute logic

    // === Transport Controls
    void armForRecording(bool armed);
    void startRecording(juce::int64  globalSample);
    void stopRecording();
    void startPlayback();
    void stopPlayback();
    void stop();
    void clear();                                               // Clear loop

    // === DSP Controls (per Track)===
    void setVolumeDb(float volumeDb);
    void setPan(float pan);                                     // REQUIRED FEATURE 1 from OSU project page
    void setMute(bool muted);
    void setSolo(bool soloed);
    void setReverse(bool reverse);                              // REQUIRED FEATURE 2 from OSU project page
    void setSlip(int samples);                                  // REQUIRED FEATURE 3 from OSU project page
    void setLoopLength (int samples);

    // === Loop Manipulation
    void multiplyLoop();                                        // double loop length
    void divideLoop();                                          // halve loop length
    void performUndo();                                         // Undo last op


    // === Getters for UI and manager) ===
    State getState() const noexcept { return currentState.load(); }
    int getTrackId() const noexcept {return trackId; }
    int getLoopLengthSamples() const noexcept { return loopLengthSamples.load(); }
    bool hasLoop() const noexcept { return loopLengthSamples.load() > 0; }
    bool isArmed() const noexcept { return isArmedForRecording.load(); }
    bool isMuted() const noexcept { return muteState.load(); }
    bool isSoloed() const noexcept { return soloState.load(); }
    float getCurrentVolumeDb() const noexcept { return currentVolumeDb.load(); }
    float getCurrentPan() const noexcept { return currentPan.load(); }
    juce::String getStateString() const;

    // === Audio data access for FileHandler ===
    void setAudioBuffer(const juce::AudioSampleBuffer &newBuffer, double sourceSampleRate);
    const juce::AudioSampleBuffer& getAudioBuffer() const { return player.getBuffer(); }
    double getSourceSampleRate() const { return player.getSourceSampleRate(); }
    bool hasAudio() const { return player.getBuffer().getNumSamples() > 0; }


    // === Sync info (for manager to read/write) ===
    void setRecordingStartGlobalSample(juce::int64 sample) {
        recordingStartGlobalSample = sample;
    }
    juce::int64 getRecordingStartGlobalSample() const noexcept {
        return recordingStartGlobalSample.load();
    }


private:
    // === Core class components===
    const int trackId;
    bool playerLoaded = false;
    gin::AudioFifo recordingBuffer;
    gin::AudioFifo undoBuffer;
    gin::SamplePlayer player;

    // === States (atomic for thread safety) ===
    std::atomic<State> currentState {State::Empty };
    std::atomic<bool> isArmedForRecording { false };
    std::atomic<bool> isRecordingActive { false };
    std::atomic<bool> isPlaybackActive { false };

    // === Loop metadata ===
    std::atomic<int> loopLengthSamples {0 };                          // Current loop length measured by sample rate
    std::atomic<juce::int64> recordingStartGlobalSample { 0 };


    // === DSP ===
    gin::EasedValueSmoother<float, gin::QuadraticOutEasing> volumeSmoother;
    gin::EasedValueSmoother<float, gin::QuadraticOutEasing> panSmoother;
    std::atomic<float> currentVolumeDb { TrackConfig::DEFAULT_VOLUME_DB };
    std::atomic<float> currentPan { TrackConfig::DEFAULT_PAN };
    std::atomic<bool> muteState { false };
    std::atomic<bool> soloState { false };
    std::atomic<bool> reverseState { false };
    std::atomic<int> slipOffset { 0 };

    // === Sample rate ===
    double sampleRate = 0.0;

    // === Undo ===
    bool hasUndo = false;
    int undoLoopLength = 0;

    // === Private Helpers ===
    static void applyReverse(juce::AudioBuffer<float>& buffer);  // Helper for reverse
    void applySlip(juce::AudioBuffer<float>& buffer);     // Helper for slip
    void applyDspProcessing(juce::AudioBuffer<float>& buffer);
    void saveUndo();
    void loadRecordingToPlayer();


    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LoopTrack)
};


