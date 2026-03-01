#include "MixerEngine.h"

namespace
{
constexpr int kStereoChannels = 2;
constexpr double kSmoothingSeconds = 0.01;
constexpr float kClipMin = -1.0f;
constexpr float kClipMax = 1.0f;
}

MixerEngine::MixerEngine()
{
    // starting with safe defaults
    for (size_t i = 0; i < TrackConfig::MAX_TRACKS; ++i)
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
    gainRampScratch.assign(static_cast<size_t>(juce::jmax(1, blockSize)), 1.0f);

    juce::dsp::ProcessSpec spec{};
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(blockSize);
    spec.numChannels = static_cast<juce::uint32>(kStereoChannels);

    for (size_t i = 0; i < TrackConfig::MAX_TRACKS; ++i)
    {
        volumeSmoothers[i].reset(sampleRate, kSmoothingSeconds); // ~10ms
        volumeSmoothers[i].setCurrentAndTargetValue(1.0f);
        panners[i].setRule(juce::dsp::PannerRule::squareRoot3dB);
        panners[i].prepare(spec);
        panners[i].setPan(0.0f);

        trackWorkingBuffers[i].setSize(kStereoChannels, blockSize, false, false, true);
        trackWorkingBuffers[i].clear();
    }
}

void MixerEngine::attachParameters(juce::AudioProcessorValueTreeState& apvts)
{
    // hook APVTS params here (value names might change later, these are temporary)
    for (size_t i = 0; i < TrackConfig::MAX_TRACKS; ++i)
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

void MixerEngine::copyTrackIntoWorkingBuffer(size_t trackIndex,
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
        auto* destData = workingBuffer.getWritePointer(channel);

        if (block1 > 0)
        {
            const auto* srcData = sourceTrack->getReadPointer(sourceChannel, sourceStart);
            juce::FloatVectorOperations::copy(destData, srcData, block1);
        }

        if (block2 > 0)
        {
            const int wrappedCopy = juce::jmin(block2, sourceSamples);
            const auto* srcData = sourceTrack->getReadPointer(sourceChannel, 0);
            juce::FloatVectorOperations::copy(destData + block1, srcData, wrappedCopy);
        }
    }
}

void MixerEngine::process(const std::vector<juce::AudioBuffer<float>*>& inputTracks,
                          juce::AudioBuffer<float>& masterOutput)
{
    if (masterOutput.getNumSamples() == 0)
        return;

    int numSamples = masterOutput.getNumSamples();
    if (static_cast<int>(gainRampScratch.size()) < numSamples)
        gainRampScratch.resize(static_cast<size_t>(numSamples), 1.0f);

    const std::int64_t blockStartSample = globalSampleCounter == nullptr
        ? 0
        : globalSampleCounter->load(std::memory_order_relaxed);

    // master buffer is rebuilt every block by summing trackWorkingBuffers
    masterOutput.clear();

    // read params per block (audio thread), process each track, then sum into master
    for (size_t i = 0; i < TrackConfig::MAX_TRACKS; ++i)
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
            i < inputTracks.size() ? inputTracks[i] : nullptr;
        copyTrackIntoWorkingBuffer(i, sourceTrack, numSamples, blockStartSample);

        // Per-track gain smoothing avoids zipper noise from rapid UI changes
        volumeSmoothers[i].setTargetValue(volValue);
        float startGain = volumeSmoothers[i].getCurrentValue();
        volumeSmoothers[i].skip(numSamples);
        float endGain = volumeSmoothers[i].getCurrentValue();

        auto& workingBuffer = trackWorkingBuffers[i];

        if (startGain == endGain)
        {
            for (int ch = 0; ch < workingBuffer.getNumChannels(); ++ch)
                juce::FloatVectorOperations::multiply(workingBuffer.getWritePointer(ch), startGain, numSamples);
        }
        else
        {
            if (numSamples == 1)
            {
                gainRampScratch[0] = endGain;
            }
            else
            {
                const float step = (endGain - startGain) / static_cast<float>(numSamples - 1);
                float gain = startGain;
                for (int sample = 0; sample < numSamples; ++sample)
                {
                    gainRampScratch[static_cast<size_t>(sample)] = gain;
                    gain += step;
                }
            }

            for (int ch = 0; ch < workingBuffer.getNumChannels(); ++ch)
                juce::FloatVectorOperations::multiply(workingBuffer.getWritePointer(ch), gainRampScratch.data(), numSamples);
        }

        // JUCE DSP panner handles stereo panning per track
        panners[i].setPan(juce::jlimit(-1.0f, 1.0f, pan));
        juce::dsp::AudioBlock<float> block(workingBuffer);
        juce::dsp::ProcessContextReplacing<float> context(block);
        panners[i].process(context);

        const int channelsToSum = juce::jmin(masterOutput.getNumChannels(), workingBuffer.getNumChannels());
        for (int channel = 0; channel < channelsToSum; ++channel)
        {
            auto* masterData = masterOutput.getWritePointer(channel);
            const auto* trackData = workingBuffer.getReadPointer(channel);
            juce::FloatVectorOperations::addWithMultiply(masterData, trackData, 1.0f, numSamples);
        }
    }

    // keep consistent headroom when multiple full-scale tracks are active
    for (int channel = 0; channel < masterOutput.getNumChannels(); ++channel)
        juce::FloatVectorOperations::multiply(masterOutput.getWritePointer(channel), masterHeadroomScale, numSamples);

    // simple safety limiter: hard clip at 0 dBFS
    for (int channel = 0; channel < masterOutput.getNumChannels(); ++channel)
    {
        auto* data = masterOutput.getWritePointer(channel);
        juce::FloatVectorOperations::clip(data, data, kClipMin, kClipMax, numSamples);
    }
}

float MixerEngine::getLastVolDb(size_t track) const
{
    if (track >= TrackConfig::MAX_TRACKS)
        return 0.0f;
    return lastVolDb[track];
}

float MixerEngine::getLastPan(size_t track) const
{
    if (track >= TrackConfig::MAX_TRACKS)
        return 0.0f;
    return lastPan[track];
}

bool MixerEngine::isAnySoloActive() const
{
    
    // helper for mute/solo logic later
    for (size_t i = 0; i < TrackConfig::MAX_TRACKS; ++i)
    {
        if (soloParams[i] != nullptr && soloParams[i]->load() > 0.5f)
            return true;
    }

    return false;
}
