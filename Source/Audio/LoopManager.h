//
// Created by Vincewa Tran on 1/28/26.
//
#pragma once
#include "memory"
#include "array"
#include "vector"
#include "gin_dsp/gin_dsp.h"
#include "LoopTrack.h"
#include "SyncEngine.h"
#include "MixerEngine.h"
#include "../Utils/TrackConfig.h"


class LoopManager {
public:
    explicit LoopManager(SyncEngine& syncEngine);
    ~LoopManager() = default;

    // === Audio thread methods ===
    void prepareToPlay(double sampleRate, int samplesPerBlock, int numChannels);
    void releaseResources();
    void processBlock(const juce::AudioBuffer<float>& input,
                      juce::AudioBuffer<float>& output);

    // === Track access ===
    LoopTrack* getTrack(size_t trackIndex);
    const LoopTrack* getTrack(size_t trackIndex) const;
    size_t getNumTracks() const noexcept { return TrackConfig::MAX_TRACKS; }

    // === Global transport controls ===
    void startAllPlayback();
    void stopAllPlayback();
    void clearAllTracks();
    void armAllTracks(bool armed);

    // === Sync access ===
    SyncEngine& getSyncEngine() noexcept { return syncEngine; }
    const SyncEngine& getSyncEngine() const noexcept { return syncEngine; }

    // === Status getters ===
    bool isAnyTrackRecording() const;
    bool isAnyTrackArmed() const;
    bool isAllTracksEmpty() const;
    int getNumActiveTracks() const;

    // === Get track outputs for MixerEngine ===
    std::vector<juce::AudioBuffer<float>*> getTrackOutputs();

private:
    // === Core components ===
    SyncEngine& syncEngine;
    std::array<std::unique_ptr<LoopTrack>, TrackConfig::MAX_TRACKS> tracks;

    // === Per-track output buffers ===
    std::vector<gin::ScratchBuffer> trackOutputs;

    // === Private helpers
    void updateTrackOutputs(int numChannels, int numSamples);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LoopManager)
};


