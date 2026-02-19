//
// Created by Vincewa Tran on 1/28/26.
//

#include "LoopManager.h"

LoopManager::LoopManager(SyncEngine& se) : syncEngine(se) {

     // Create all our tracks
     for (size_t i = 0; i < TrackConfig::MAX_TRACKS; i++) {
         tracks[i] = std::make_unique<LoopTrack>(static_cast<int>(i));
     }

     // Pre-allocate vector for track outputs
     trackOutputs.reserve(TrackConfig::MAX_TRACKS);
}

void LoopManager::prepareToPlay(double sampleRate, int samplesPerBlock, int numChannels) {
    // Prepare each track
    for (auto& track : tracks) {
        track->prepareToPlay(sampleRate, samplesPerBlock, numChannels);
    }

    // Prepare output buffers for the next process block
    updateTrackOutputs(numChannels, samplesPerBlock);
}

void LoopManager::releaseResources() {
    for (auto& track : tracks) {
        track->releaseResources();
    }
    trackOutputs.clear();
}

void LoopManager::processBlock(const juce::AudioBuffer<float> &input, juce::AudioBuffer<float> &output) {

    const int numSamples = output.getNumSamples();
    const int numChannels = output.getNumChannels();

    // 1. Handle sync (advance the global clock)
    syncEngine.advance(numSamples);

    // 2. Update output buffers for this block
    updateTrackOutputs(numChannels, numSamples);

    // 3. Process each track into its own output buffer
    for (size_t i = 0; i < TrackConfig::MAX_TRACKS; i++) {
        auto& track = tracks[i];
        auto& trackBuffer = trackOutputs[i];

        // Clear track's output buffer
        trackBuffer.clear();

        // Let the track process - pass syncEngine for beat alignment
        track->processBlock(input, trackBuffer, syncEngine);
    }

    // 4. Clear main output
    output.clear();
}

std::vector<juce::AudioBuffer<float>*> LoopManager::getTrackOutputs() {
    std::vector<juce::AudioBuffer<float>*> outputs;
    outputs.reserve(TrackConfig::MAX_TRACKS);

    for (size_t i = 0; i < TrackConfig::MAX_TRACKS; ++i) {
        outputs.push_back(&trackOutputs[i]);
    }
    return outputs;
}

void LoopManager::updateTrackOutputs(int numChannels, int numSamples) {
    jassert(numChannels > 0);
    jassert(numSamples > 0);

    trackOutputs.clear();
    trackOutputs.reserve(TrackConfig::MAX_TRACKS);

    for (size_t i = 0; i < TrackConfig::MAX_TRACKS; i++) {
        trackOutputs.emplace_back(numChannels, numSamples);
    }
}

/**
 *  Get track when we need to modify the track
 *  ex:
 *  - track->startRecording(globalSample) - Changes track state
 *  - track->setVolumeDb(0.5f) - Changes parameters
 *  - track->clear() - Modifies track data
 */
LoopTrack* LoopManager::getTrack(size_t trackIndex) {
    if (trackIndex < tracks.size()) {
        return tracks[trackIndex].get();
    }
    return nullptr;
}

/**
 *  Get track when the caller only needs to read track data
 *  ex:
 *  - track->getState() - Read-only query
 *  - track->getLoopLengthSamples() - Just checking values
 *  - track->getStateString() - Display only
 */
const LoopTrack* LoopManager::getTrack(size_t trackIndex) const {
    if (trackIndex < tracks.size()) {
        return tracks[trackIndex].get();
    }
    return nullptr;
}

void LoopManager::startAllPlayback() {
    for (auto& track : tracks) {
        if (track->hasLoop()) {
            track->startPlayback();
        }
    }
}

void LoopManager::stopAllPlayback() {
    for (auto& track : tracks) {
        track->stopPlayback();
    }
}

void LoopManager::clearAllTracks() {
    for (auto& track : tracks) {
        track->clear();
    }
}

void LoopManager::armAllTracks(bool armed) {
    for (auto& track : tracks) {
        track->armForRecording(armed);
    }
}

bool LoopManager::isAnyTrackRecording() const {
    for (const auto& track : tracks) {
        if (track->getState() == LoopTrack::State::Recording) {
            return true;
        }
    }
    return false;
}

bool LoopManager::isAnyTrackArmed() const {
    for (const auto& track : tracks) {
        if (track->isArmed()) {
            return true;
        }
    }
    return false;
}

bool LoopManager::isAllTracksEmpty() const {
    for (const auto& track : tracks) {
        if (track->hasLoop()) {
            return false;
        }
    }
    return true;
}

int LoopManager::getNumActiveTracks() const {
    int count = 0;
    for (const auto& track : tracks) {
        if (track->hasLoop()) {
            count++;
        }
    }
    return count;
}