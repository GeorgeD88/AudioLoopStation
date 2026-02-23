#ifndef AUDIOLOOPSTATION_MIXERENGINE_H
#define AUDIOLOOPSTATION_MIXERENGINE_H

#include <array>
#include <atomic>
#include <cstdint>
#include <vector>

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include "../Utils/TrackConfig.h"

class MixerEngine {
public:
    static constexpr float kDefaultHeadroomScale = 0.25f;

    MixerEngine();

    void prepare(double sampleRate, int samplesPerBlock);
    void attachParameters(juce::AudioProcessorValueTreeState& apvts);
    void setGlobalSampleCounter(std::atomic<std::int64_t>* counter) noexcept;
    void process(const std::vector<juce::AudioBuffer<float>*>& inputTracks,
                 juce::AudioBuffer<float>& masterOutput);
    float getLastVolDb(int track) const;
    float getLastPan(int track) const;

private:
    // APVTS parameter pointers (safe for audio-thread reads via atomic load).
    std::array<std::atomic<float>*, TrackConfig::MAX_TRACKS> volParams{};
    std::array<std::atomic<float>*, TrackConfig::MAX_TRACKS> panParams{};
    std::array<std::atomic<float>*, TrackConfig::MAX_TRACKS> muteParams{};
    std::array<std::atomic<float>*, TrackConfig::MAX_TRACKS> soloParams{};

    // Per-track smoothing/history.
    std::array<juce::LinearSmoothedValue<float>, TrackConfig::MAX_TRACKS> volumeSmoothers;
    std::array<juce::dsp::Panner<float>, TrackConfig::MAX_TRACKS> panners;
    std::array<float, TrackConfig::MAX_TRACKS> lastVolDb{};
    std::array<float, TrackConfig::MAX_TRACKS> lastPan{};

    // Scratch buffers used each block before summing into master.
    std::array<juce::AudioBuffer<float>, TrackConfig::MAX_TRACKS> trackWorkingBuffers{};

    double sampleRate = 0.0;
    int blockSize = 0;
    float masterHeadroomScale = kDefaultHeadroomScale;

    // Optional shared clock from SyncEngine/AudioProcessor.
    std::atomic<std::int64_t>* globalSampleCounter = nullptr;

    void copyTrackIntoWorkingBuffer(int trackIndex,
                                    const juce::AudioBuffer<float>* sourceTrack,
                                    int numSamples,
                                    std::int64_t blockStartSample);
    bool isAnySoloActive() const;
};

#endif //AUDIOLOOPSTATION_MIXERENGINE_H
