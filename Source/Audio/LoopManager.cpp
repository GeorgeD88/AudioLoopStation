//
// Created by Vincewa Tran on 1/28/26.
//

#include "LoopManager.h"

LoopManager::LoopManager(SyncEngine& se) : syncEngine(se) {
     // Create all our tracks
     for (size_t i = 0; i < TrackConfig::MAX_TRACKS; i++) {
         tracks[i] = std::make_unique<LoopTrack>(static_cast<int>(i));
     }

     // Initialize track outputs
     for (auto& buf : trackOutputs) {
         buf = nullptr;
     }
}

void LoopManager::prepareToPlay(double sampleRate, int samplesPerBlock, int numChannels) {
    // Prepare each track
    for (auto& track : tracks) {
        if (track)
        track->prepareToPlay(sampleRate, samplesPerBlock, numChannels);
    }

    // Pre-allocate all track output buffers ONCE, happens on message thread not the audio thread
    for (auto& buf : trackOutputs) {
        buf = std::make_unique<gin::ScratchBuffer>(numChannels, samplesPerBlock);
        buf->clear();
    }

}

void LoopManager::releaseResources() {
    for (auto& track : tracks) {
        track->releaseResources();
    }

    // Release track outputs to return them to the ScratchBuffer pool
    for (auto& buf : trackOutputs) {
        buf.reset();
    }
}

void LoopManager::processBlock(const juce::AudioBuffer<float> &input) {
    const int numSamples = input.getNumSamples();

    // 1. Handle sync (advance the global clock)
    syncEngine.advance(numSamples);

    // 2. Clear all track output buffers (reuse, don't allocate)
    for (auto& buf : trackOutputs) {
        if (buf) {
            buf->clear();
        }
    }

    // 3. Process each track into its own output buffer
    for (size_t i = 0; i < TrackConfig::MAX_TRACKS; i++) {
        auto& track = tracks[i];
        auto& trackBuffer = trackOutputs[i];

        if (!track || !trackBuffer) continue;

        // Let the track process - reads from inputs, writes to trackBuffer
        track->processBlock(input, *trackBuffer, syncEngine);
    }
}

std::vector<juce::AudioBuffer<float>*> LoopManager::getTrackOutputs() {
    std::vector<juce::AudioBuffer<float>*> outputs;
    outputs.reserve(TrackConfig::MAX_TRACKS);

    for (auto& buf : trackOutputs) {
        if (buf) {
            outputs.push_back(buf.get());
        } else {
            outputs.push_back(nullptr);
        }
    }
    return outputs;
}

std::vector<const juce::AudioBuffer<float>*> LoopManager::getTrackOutputs() const {
    std::vector<const juce::AudioBuffer<float>*> outputs;
    outputs.reserve(TrackConfig::MAX_TRACKS);

    for (auto& buf : trackOutputs) {
        if (buf) {
            // Const-correctness: safe because we're providing read-only access
            outputs.push_back(static_cast<const juce::AudioBuffer<float>*>(buf.get()));
        } else {
            outputs.push_back(nullptr);
        }
    }
    return outputs;
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

// === Per-track status helpers (for UI) ===
LoopTrack::State LoopManager::getTrackState(size_t index) const {
    if (auto* track = getTrack(index))
        return track->getState();
    return LoopTrack::State::Empty;
}

bool LoopManager::isTrackArmed(size_t index) const {
    if (auto* track = getTrack(index))
        return track->isArmed();
    return false;
}

bool LoopManager::isTrackMuted(size_t index) const {
    if (auto* track = getTrack(index))
        return track->isMuted();
    return false;
}

bool LoopManager::isTrackSoloed(size_t index) const {
    if (auto* track = getTrack(index))
        return track->isSoloed();
    return false;
}

float LoopManager::getTrackVolume(size_t index) const {
    if (auto* track = getTrack(index))
        return track->getCurrentVolumeDb();
    return 0.0f;
}

float LoopManager::getTrackPan(size_t index) const {
    if (auto* track = getTrack(index))
        return track->getCurrentPan();
    return 0.0f;
}