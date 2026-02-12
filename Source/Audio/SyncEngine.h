//
// Created by Vincewa Tran on 2/12/26.
//
#pragma once
#include "juce_audio_basics/juce_audio_basics.h"
#include "atomic"
#include "../Utils/TrackConfig.h"


/**
 * Global synchronization engine for all loop tracks
 * - Timing logic/data
 */
class SyncEngine {
public:
    SyncEngine() = default;
    ~SyncEngine() = default;

    void prepare(double sampleRate, int /*samplesPerBlock*/) {
        this->sampleRate = sampleRate;
        globalSample.store(0);
    }

    void advance(int numSamples) {
        globalSample.fetch_add(numSamples);
    }

    // === Thread-safe getters for UI ===
    juce::int64 getGlobalSample() const noexcept {
        return globalSample.load();
    }

    double getGlobalSeconds() const noexcept {
        return globalSample.load() / sampleRate;
    }

    double getSampleRate() const noexcept {
        return sampleRate;
    }

    // === Tempo/BPM management ===
    void setTempo(float bpm) noexcept {
        tempoBPM.store(bpm);
    }

    float getTempo() const noexcept {
        return tempoBPM.load();
    }

    float getSamplesPerBeat() const noexcept {
        float bpm = tempoBPM.load();
        if (bpm <= 0) return 0;
        return static_cast<int>((60.0 / bpm) * sampleRate);
    }

    int getSamplesPerBar(int beatsPerBar = 4) const noexcept {
        return getSamplesPerBeat() * beatsPerBar;
    }

private:
    std::atomic<juce::int64> globalSample { 0 };
    std::atomic<float> tempoBPM {TrackConfig::DEFAULT_BPM};         // 120.0f is the default BPM
    double sampleRate = TrackConfig::DEFAULT_SAMPLE_RATE;


    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SyncEngine);
};


