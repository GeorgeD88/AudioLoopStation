//
// Created by Besher Al Maleh on 1/29/26.
//

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
        volumeSmoothers[i].setCurrentAndTargetValue(1.0f);
}

void MixerEngine::attachParameters(juce::AudioProcessorValueTreeState& apvts)
{
    // planning to hook APVTS params here
    juce::ignoreUnused(apvts);
    
}

void MixerEngine::process(std::vector<juce::AudioBuffer<float>*>& inputTracks,
                          juce::AudioBuffer<float>& masterOutput)
{
    // stub: just clear output for now
    juce::ignoreUnused(inputTracks);

    if (masterOutput.getNumSamples() == 0)
        return;

    masterOutput.clear();
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
