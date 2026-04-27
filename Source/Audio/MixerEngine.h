#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <vector>

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <juce_events/juce_events.h>

#include "../Utils/Config.h"

struct MixerTrackUiState
{
    size_t trackIndex = 0;
    bool audible = true;
};

class MixerEngine : private juce::AudioProcessorValueTreeState::Listener,
                    private juce::AsyncUpdater {
public:
    class Listener
    {
    public:
        virtual ~Listener() = default;
        virtual void mixerTrackUiStateChanged(const MixerTrackUiState& state) = 0;
    };

    static constexpr float kDefaultHeadroomScale = 0.25f;

    MixerEngine();
    ~MixerEngine() override;

    void prepare(double sampleRate, int samplesPerBlock);
    void attachParameters(juce::AudioProcessorValueTreeState& apvts);
    void detachParameters();
    void setGlobalSampleCounter(std::atomic<std::int64_t>* counter) noexcept;
    void process(const std::vector<juce::AudioBuffer<float>*>& inputTracks,
                 juce::AudioBuffer<float>& masterOutput);
    bool isTrackAudible(size_t trackIndex) const noexcept;
    MixerTrackUiState getTrackUiState(size_t trackIndex) const noexcept;
    void addListener(Listener* listener);
    void removeListener(Listener* listener);
    float getLastVolDb(size_t track) const;
    float getLastPan(size_t track) const;
    bool getIsAnyTrackSoloed() const noexcept;

private:
    // APVTS parameter pointers (safe for audio-thread reads via atomic load).
    std::array<std::atomic<float>*, Config::NUM_TRACKS> volParams{};
    std::array<std::atomic<float>*, Config::NUM_TRACKS> panParams{};
    std::array<std::atomic<float>*, Config::NUM_TRACKS> muteParams{};
    std::array<std::atomic<float>*, Config::NUM_TRACKS> soloParams{};

    // Per-track smoothing/history.
    std::array<juce::LinearSmoothedValue<float>, Config::NUM_TRACKS> volumeSmoothers;
    std::array<juce::dsp::Panner<float>, Config::NUM_TRACKS> panners;
    std::array<float, Config::NUM_TRACKS> lastVolDb{};
    std::array<float, Config::NUM_TRACKS> lastPan{};

    // Scratch buffers used each block before summing into master.
    std::array<juce::AudioBuffer<float>, Config::NUM_TRACKS> trackWorkingBuffers{};
    std::vector<float> gainRampScratch;

    double sampleRate = 0.0;
    int blockSize = 0;
    float masterHeadroomScale = kDefaultHeadroomScale;
    std::atomic<bool> isAnyTrackSoloed{false};

    // Optional shared clock from SyncEngine/AudioProcessor.
    std::atomic<std::int64_t>* globalSampleCounter = nullptr;
    juce::AudioProcessorValueTreeState* attachedApvts = nullptr;
    juce::ListenerList<Listener> listeners;

    void copyTrackIntoWorkingBuffer(size_t trackIndex,
                                    const juce::AudioBuffer<float>* sourceTrack,
                                    int numSamples,
                                    std::int64_t blockStartSample);
    void parameterChanged(const juce::String& parameterID, float newValue) override;
    void handleAsyncUpdate() override;
    void sendTrackUiStateNotifications();
    void refreshAnySoloStateFromParams() noexcept;
    bool isTrackAudible(size_t trackIndex, bool anySoloActive) const noexcept;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MixerEngine)
};
