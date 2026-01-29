#ifndef AUDIOLOOPSTATION_MIXERENGINE_H
#define AUDIOLOOPSTATION_MIXERENGINE_H

#include <array>
#include <atomic>
#include <vector>

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>

#include "../Utils/TrackConfig.h"

class MixerEngine {
public:
    MixerEngine();

    void prepare(double sampleRate, int samplesPerBlock);
    
    void attachParameters(juce::AudioProcessorValueTreeState& apvts);
    void process(std::vector<juce::AudioBuffer<float>*>& inputTracks,
                 juce::AudioBuffer<float>& masterOutput);

private:
    std::array<std::atomic<float>*, TrackConfig::MAX_TRACKS> volParams{};
    std::array<std::atomic<float>*, TrackConfig::MAX_TRACKS> panParams{};
    
    std::array<std::atomic<float>*, TrackConfig::MAX_TRACKS> muteParams{};
    std::array<std::atomic<float>*, TrackConfig::MAX_TRACKS> soloParams{};

    std::array<juce::LinearSmoothedValue<float>, TrackConfig::MAX_TRACKS> volumeSmoothers;

    double sampleRate = 0.0;
    int blockSize = 0;

    
    bool isAnySoloActive() const;
};

#endif //AUDIOLOOPSTATION_MIXERENGINE_H
