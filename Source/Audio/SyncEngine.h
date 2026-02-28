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

    void prepare(double sr, int /*samplesPerBlock*/) {
        sampleRate = sr;
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
        return static_cast<double>(globalSample.load()) / sampleRate;
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

    int getSamplesPerBeat() const noexcept {
        float bpm = tempoBPM.load();
        if (bpm <= 0) return 0;

        // (60 sec/min divided by bpm) x sampleRate = secs per beat x sampleRate
        double samplesPerBeat = (60.0 / static_cast<double>(bpm)) * sampleRate;

        // Round to the nearest integer
        return static_cast<int>(std::round(samplesPerBeat));
    }

    int getSamplesPerBar(int beatsPerBar = 4) const noexcept {
        return static_cast<int>(getSamplesPerBeat() * beatsPerBar);
    }

private:
    std::atomic<juce::int64> globalSample { 0 };
    std::atomic<float> tempoBPM {TrackConfig::DEFAULT_BPM};         // 120.0f is the default BPM
    double sampleRate = 0.0;


    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SyncEngine)
};


