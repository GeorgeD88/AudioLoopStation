#include "MixerEngine.h"

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
        volumeSmoothers[i].reset(sampleRate, 0.01); // ~10ms
        volumeSmoothers[i].setCurrentAndTargetValue(1.0f);
    }
}

void MixerEngine::attachParameters(juce::AudioProcessorValueTreeState& apvts)
{
    // hook APVTS params here (value names might change later, these are temporary)
    for (int i = 0; i < TrackConfig::MAX_TRACKS; ++i)
    {
        auto idx = juce::String(i);

        volParams[i]  = apvts.getRawParameterValue("track_" + idx + "_vol");
        panParams[i]  = apvts.getRawParameterValue("track_" + idx + "_pan");
        muteParams[i] = apvts.getRawParameterValue("track_" + idx + "_mute");
        soloParams[i] = apvts.getRawParameterValue("track_" + idx + "_solo");

    }

    // TODO: double-check these IDs with Maddox
}

void MixerEngine::process(std::vector<juce::AudioBuffer<float>*>& inputTracks,
                          juce::AudioBuffer<float>& masterOutput)
{
    // stub: only smoothing for now, still clearing output
    if (masterOutput.getNumSamples() == 0)
        return;

    int numSamples = masterOutput.getNumSamples();

    // read params per block (audio thread)
    for (int i = 0; i < TrackConfig::MAX_TRACKS; ++i)
    {
        float volDb = 0.0f;
        float pan = 0.0f;

        if (volParams[i] != nullptr)
            volDb = volParams[i]->load();

        if (panParams[i] != nullptr)
            pan = panParams[i]->load();

        lastVolDb[i] = volDb;
        lastPan[i] = pan;

        float gain = juce::Decibels::decibelsToGain(volDb);
        volumeSmoothers[i].setTargetValue(gain);

        float startGain = volumeSmoothers[i].getCurrentValue();
        volumeSmoothers[i].skip(numSamples);
        float endGain = volumeSmoothers[i].getCurrentValue();

        if (i < static_cast<int>(inputTracks.size()) && inputTracks[i] != nullptr)
        {
            auto* track = inputTracks[i];
            for (int ch = 0; ch < track->getNumChannels(); ++ch)
                track->applyGainRamp(ch, 0, numSamples, startGain, endGain);
        }

        juce::ignoreUnused(volDb, pan);
    }

    masterOutput.clear();
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
