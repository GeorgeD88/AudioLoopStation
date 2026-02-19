#include "MixerEngine.h"

#include <cmath>

namespace
{
constexpr int kStereoChannels = 2;
constexpr double kSmoothingSeconds = 0.01;
}

MixerEngine::MixerEngine()
{
    // starting with safe defaults
    for (int i = 0; i < TrackConfig::MAX_TRACKS; ++i)
    {
        volParams[i] = nullptr;
        panParams[i] = nullptr;
        muteParams[i] = nullptr;
        soloParams[i] = nullptr;
        
        volumeSmoothers[i].setCurrentAndTargetValue(1.0f);
    }
}

void MixerEngine::prepare(double sampleRateIn, int samplesPerBlock)
{
    // setup for audio thread
    sampleRate = sampleRateIn;
    blockSize = samplesPerBlock;

    for (int i = 0; i < TrackConfig::MAX_TRACKS; ++i)
    {
        volumeSmoothers[i].reset(sampleRate, kSmoothingSeconds); // ~10ms
        volumeSmoothers[i].setCurrentAndTargetValue(1.0f);

        trackWorkingBuffers[i].setSize(kStereoChannels, blockSize, false, false, true);
        trackWorkingBuffers[i].clear();
    }
}

void MixerEngine::attachParameters(juce::AudioProcessorValueTreeState& apvts)
{
    // hook APVTS params here (value names might change later, these are temporary)
    for (int i = 0; i < TrackConfig::MAX_TRACKS; ++i)
    {
        auto idx = juce::String(i + 1);
        auto prefix = "Track" + idx + "_";

        volParams[i]  = apvts.getRawParameterValue(prefix + "Volume");
        panParams[i]  = apvts.getRawParameterValue(prefix + "Pan");
        muteParams[i] = apvts.getRawParameterValue(prefix + "Mute");
        soloParams[i] = apvts.getRawParameterValue(prefix + "Solo");

    }

    // TODO: double-check these IDs with Maddox
}

void MixerEngine::setGlobalSampleCounter(std::atomic<std::int64_t>* counter) noexcept
{
    globalSampleCounter = counter;
}

void MixerEngine::copyTrackIntoWorkingBuffer(int trackIndex,
                                             const juce::AudioBuffer<float>* sourceTrack,
                                             int numSamples,
                                             std::int64_t blockStartSample)
{
    auto& workingBuffer = trackWorkingBuffers[trackIndex];
    if (workingBuffer.getNumSamples() != numSamples)
        workingBuffer.setSize(kStereoChannels, numSamples, false, false, true);

    workingBuffer.clear();

    if (sourceTrack == nullptr)
        return;

    const int sourceSamples = sourceTrack->getNumSamples();
    const int sourceChannels = sourceTrack->getNumChannels();

    if (sourceSamples <= 0 || sourceChannels <= 0)
        return;

    const int channelsToCopy = juce::jmin(sourceChannels, workingBuffer.getNumChannels());
    int sourceStart = 0;

    if (sourceSamples >= numSamples)
        sourceStart = static_cast<int>(blockStartSample % static_cast<std::int64_t>(sourceSamples));

    const int block1 = juce::jmin(numSamples, sourceSamples - sourceStart);
    const int block2 = numSamples - block1;

    for (int channel = 0; channel < channelsToCopy; ++channel)
    {
        // replicate mono tracks across stereo so panning still works as expected.
        const int sourceChannel = (sourceChannels == 1) ? 0 : channel;

        if (block1 > 0)
        {
            workingBuffer.copyFrom(channel, 0, *sourceTrack,
                                   sourceChannel, sourceStart, block1);
        }

        if (block2 > 0)
        {
            workingBuffer.copyFrom(channel, block1, *sourceTrack,
                                   sourceChannel, 0, juce::jmin(block2, sourceSamples));
        }
    }
}

void MixerEngine::process(const std::vector<juce::AudioBuffer<float>*>& inputTracks,
                          juce::AudioBuffer<float>& masterOutput)
{
    if (masterOutput.getNumSamples() == 0)
        return;

    int numSamples = masterOutput.getNumSamples();
    const std::int64_t blockStartSample = globalSampleCounter == nullptr
        ? 0
        : globalSampleCounter->load(std::memory_order_relaxed);

    // master buffer is rebuilt every block by summing trackWorkingBuffers
    masterOutput.clear();

    // read params per block (audio thread), process each track, then sum into master
    for (int i = 0; i < TrackConfig::MAX_TRACKS; ++i)
    {
        float volValue = 1.0f;
        float pan = 0.0f;

        if (volParams[i] != nullptr)
            volValue = volParams[i]->load();

        if (panParams[i] != nullptr)
            pan = panParams[i]->load();

        lastVolDb[i] = volValue;
        lastPan[i] = pan;

        const juce::AudioBuffer<float>* sourceTrack =
            i < static_cast<int>(inputTracks.size()) ? inputTracks[i] : nullptr;
        copyTrackIntoWorkingBuffer(i, sourceTrack, numSamples, blockStartSample);

        // Per-track gain smoothing avoids zipper noise from rapid UI changes
        volumeSmoothers[i].setTargetValue(volValue);
        float startGain = volumeSmoothers[i].getCurrentValue();
        volumeSmoothers[i].skip(numSamples);
        float endGain = volumeSmoothers[i].getCurrentValue();

        auto& workingBuffer = trackWorkingBuffers[i];
        for (int ch = 0; ch < workingBuffer.getNumChannels(); ++ch)
        {
            workingBuffer.applyGainRamp(ch, 0, numSamples, startGain, endGain);
        }

        // equal power pan per track before summing into master
        const float clampedPan = juce::jlimit(-1.0f, 1.0f, pan);
        const float angle = (clampedPan + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
        const float leftPanGain = std::cos(angle);
        const float rightPanGain = std::sin(angle);

        if (workingBuffer.getNumChannels() > 0)
            workingBuffer.applyGain(0, 0, numSamples, leftPanGain);

        if (workingBuffer.getNumChannels() > 1)
            workingBuffer.applyGain(1, 0, numSamples, rightPanGain);

        const int channelsToSum = juce::jmin(masterOutput.getNumChannels(), workingBuffer.getNumChannels());
        for (int channel = 0; channel < channelsToSum; ++channel)
        {
            masterOutput.addFrom(channel, 0, workingBuffer, channel, 0, numSamples);
        }
    }

    // keep consistent headroom when multiple full-scale tracks are active
    masterOutput.applyGain(masterHeadroomScale);
}

float MixerEngine::getLastVolDb(int track) const
{
    if (track < 0 || track >= TrackConfig::MAX_TRACKS)
        return 0.0f;
    return lastVolDb[track];
}

float MixerEngine::getLastPan(int track) const
{
    if (track < 0 || track >= TrackConfig::MAX_TRACKS)
        return 0.0f;
    return lastPan[track];
}

bool MixerEngine::isAnySoloActive() const
{
    
    // helper for mute/solo logic later
    for (int i = 0; i < TrackConfig::MAX_TRACKS; ++i)
    {
        if (soloParams[i] != nullptr && soloParams[i]->load() > 0.5f)
            return true;
    }

    return false;
}
