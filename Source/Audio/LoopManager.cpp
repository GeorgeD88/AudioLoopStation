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
    checkLoopBoundary(syncEngine.getGlobalSample(), numSamples);
    processCommands();

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

void LoopManager::startRecording(size_t trackIndex) {
    if (trackIndex >= TrackConfig::MAX_TRACKS) return;

    // Post a StartRecording command immediately
    postCommand({ LoopCommandType::StartRecording, trackIndex, syncEngine.getGlobalSample() });

    DBG("[LOOP] Start recording on track " << trackIndex);
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
    // Clear any pending recording requests when stopping
    for (size_t i = 0; i < TrackConfig::MAX_TRACKS; ++i) {
        pendingRecordRequests[i].store(false, std::memory_order_release);
    }
}

void LoopManager::stopAllRecording() {
    for (size_t i = 0; i < TrackConfig::MAX_TRACKS; ++i) {
        auto* track = tracks[i].get();
        if (!track) continue;

        auto state = track->getState();

        // Handle both Recording and Queued states
        if (state == LoopTrack::State::Recording) {
            postCommand({LoopCommandType::StopRecording,
                         i,
                         syncEngine.getGlobalSample()});
        }
        else if (state == LoopTrack::State::Queued) {
            // Cancel queued recordings directly (or via command)
            cancelTrackRecording(i);
        }
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

void LoopManager::postCommand(const LoopCommand &command) {
    commandQueue.write(command);
}

void LoopManager::processCommands() {
    while (auto cmd = commandQueue.read())
    {
        // Validate range FIRST - prevents any invalid access
        if (!juce::isPositiveAndBelow(cmd->trackIndex, TrackConfig::MAX_TRACKS))
        {
            DBG("Ignoring command for invalid track: " + juce::String(cmd->trackIndex));
            continue;
        }

        // Safe conversion - we've guaranteed it's in range
        auto trackIdx = static_cast<size_t>(cmd->trackIndex);
        auto* track = tracks[trackIdx].get();

        // 2. Clean switch with no conditionals inside cases
        switch (cmd->type) {

            case LoopCommandType::None:
                break;

            case LoopCommandType::ArmTrack:
                track->armForRecording(true);
                break;

            case LoopCommandType::StartRecording: {
                // Store loop length
                int preRecordingLength = track->getLoopLengthSamples();

                track->startRecording(cmd->scheduledSample);
                // Special case for Track 1 setting master loop
                if (trackIdx == 0 && preRecordingLength > 0)
                    syncEngine.setMasterLoopLength(preRecordingLength);
                break;
            }

            case LoopCommandType::StopRecording:
                track->stopRecording();

                // Auto-advance to next track after first recording
                if (track->hasLoop()) {
                    // Un-arm current track
                    track->armForRecording(false);

                    size_t nextTrack = trackIdx + 1;
                    if (nextTrack < TrackConfig::MAX_TRACKS) {
                        if (auto* next = getTrack(nextTrack)) {
                            next->armForRecording(true);
                            DBG("[LOOP] Auto-advanced from Track " << trackIdx
                            << "to Track " << nextTrack);
                        }
                    } else {
                        // If last track is reached, stay on it
                        DBG("[LOOP] Last track reached, staying on Track " << trackIdx);
                    }
                }
                break;

            case LoopCommandType::ClearTrack:
                track->clear();
                break;

            default:
                DBG("Unknown command type received");
                jassertfalse;
                break;
        }
    }
}

void LoopManager::cancelTrackRecording(size_t trackIndex) {
    if (trackIndex >= TrackConfig::MAX_TRACKS) return;

    // clear pending request if waiting
    pendingRecordRequests[trackIndex].store(false, std::memory_order_release);

    // Tell track to leave Qd state
    if (auto* track = getTrack(trackIndex)) {
        track->stopQueue();
    }

    DBG("[CANCEL_REC] track=" << trackIndex);
}

void LoopManager::requestTrackRecording(size_t trackIndex)
{
    auto* track = getTrack(trackIndex);
    if (!track) return;

    // Arm the track (this puts it in Queued state)
    track->armForRecording(true);

    // Track 1 or no master loop? Start now
    pendingRecordRequests[trackIndex].store(true, std::memory_order_release);
    DBG("[LOOP] Track " << trackIndex << " armed and queued");
}

// Call this at the START of processBlock
void LoopManager::checkLoopBoundary(juce::int64 currentSample, int numSamples)
{
    if (!syncEngine.hasMasterLoop()) return;

    juce::int64 nextBoundary = ((currentSample / syncEngine.getLoopLengthSamples()) + 1)
                               * syncEngine.getLoopLengthSamples();

    // If we're crossing a boundary in this block
    if (nextBoundary - currentSample < numSamples) {
        for (size_t i = 0; i < TrackConfig::MAX_TRACKS; ++i) {
            if (pendingRecordRequests[i]) {
                postCommand({ LoopCommandType::StartRecording, i, nextBoundary });
                pendingRecordRequests[i].store(false, std::memory_order_release);
            }
        }
    }
}


/** TODO: Ensure migration to MixerEngine or delete what is already immplemented there
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
*/