#pragma once

#include "LoopTrack.h"
#include "LoopFileHandler.h"
#include <juce_core/juce_core.h>
#include <vector>
#include <memory>

/**
 * Off-loads project save I/O to a dedicated background thread so the
 * message thread is never blocked by disk writes.
 *
 * Usage:
 *   1. Call triggerSave() – returns false if a save is already running.
 *   2. The thread wakes up, serialises the snapshot, and goes back to sleep.
 *   3. Poll isSaving() if you need to know when it finishes.
 */
class BackgroundSaveThread final : public juce::Thread
{
public:
    BackgroundSaveThread();
    ~BackgroundSaveThread() override;

    /**
     * Snapshot the given tracks and schedule a save to @p destination.
     * Must be called from the message thread while no tracks are active.
     * @return false if a save is already in progress (this call is a no-op).
     */
    bool triggerSave(const juce::File& destination,
                     const std::vector<std::unique_ptr<LoopTrack>>& tracks,
                     double sampleRate,
                     float  bpm);

    /** True while a save is executing on the background thread. */
    bool isSaving() const noexcept { return mIsSaving.load(); }

private:
    void run() override;

    // --- snapshot (written by message thread, read by background thread) ---
    juce::CriticalSection                       mSnapshotLock;
    std::vector<LoopFileHandler::TrackSaveData> mSnapshot;
    juce::File                                  mDestination;
    double                                      mSampleRate { 0.0 };
    float                                       mBpm        { 120.0f };

    // --- synchronisation ---
    juce::WaitableEvent  mTrigger;   // auto-reset
    std::atomic<bool>    mIsSaving   { false };

    // --- I/O worker (owned by this thread, never touched by others) ---
    LoopFileHandler      mFileHandler;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BackgroundSaveThread)
};
