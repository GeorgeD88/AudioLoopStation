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

enum class LoopCommandType
{
    None,
    ArmTrack,
    StartRecording,
    StopRecording,
    ClearTrack
};

struct LoopCommand
{
    LoopCommandType type = LoopCommandType::None;
    size_t trackIndex = (size_t) TrackConfig::INVALID_TRACK_ID;
    juce::int64 scheduledSample = 0;

    static LoopCommand createStartRecordingCommand(size_t trackIndex, juce::int64 sample)
    {
        return { LoopCommandType::StartRecording, trackIndex, sample };
    }
};

class LoopManager {
public:
    explicit LoopManager(SyncEngine& syncEngine);
    ~LoopManager() = default;

    // === Audio thread methods ===
    void prepareToPlay(double sampleRate, int samplesPerBlock, int numChannels);
    void releaseResources();
    void processBlock(const juce::AudioBuffer<float>& input);

    // === Track access ===
    LoopTrack* getTrack(size_t trackIndex);
    const LoopTrack* getTrack(size_t trackIndex) const;
    static size_t getNumTracks() noexcept { return TrackConfig::MAX_TRACKS; }

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

    // === UI helpers ===
    LoopTrack::State getTrackState(size_t index) const;
    bool isTrackArmed(size_t index) const;
    void postCommand(const LoopCommand& command);
    void processCommands();
    void requestTrackRecording(size_t trackIndex);
    void checkLoopBoundary(juce::int64 currentSample, int numSamples);

    /** TODO: Verify in MixerEngine or move
    bool isTrackMuted(size_t index) const;
    bool isTrackSoloed(size_t index) const;
    float getTrackVolume(size_t index) const;
    float getTrackPan(size_t index) const;
     */

    // === Get track outputs for MixerEngine ===
    std::vector<juce::AudioBuffer<float>*> getTrackOutputs();
    std::vector<const juce::AudioBuffer<float>*> getTrackOutputs() const;

private:
    // === Core components ===
    SyncEngine& syncEngine;
    std::array<std::unique_ptr<LoopTrack>, TrackConfig::MAX_TRACKS> tracks;
    gin::LockFreeQueue<LoopCommand> commandQueue{ 32 };
    std::array<bool, TrackConfig::MAX_TRACKS> pendingRecordRequests { false };

    // === Per-track output buffers ===
    std::array<std::unique_ptr<gin::ScratchBuffer>, TrackConfig::MAX_TRACKS> trackOutputs;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LoopManager)
};


