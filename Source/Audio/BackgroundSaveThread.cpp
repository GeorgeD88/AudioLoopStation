#include "BackgroundSaveThread.h"

BackgroundSaveThread::BackgroundSaveThread()
    : juce::Thread("BackgroundSaveThread")
{
    startThread(juce::Thread::Priority::low);
}

BackgroundSaveThread::~BackgroundSaveThread()
{
    signalThreadShouldExit();
    mTrigger.signal();      // unblock the waiting thread so it can exit
    stopThread(2000);       // wait up to 2 s for clean shutdown
}

bool BackgroundSaveThread::triggerSave(const juce::File& destination,
                                       const std::vector<std::unique_ptr<LoopTrack>>& tracks,
                                       double sampleRate,
                                       float  bpm)
{
    if (mIsSaving.load())
        return false;   // previous save still running – skip this cycle

    // Copy audio data on the caller's thread (message thread) before handing
    // off to the background thread.  This is safe because triggerSave() is
    // only called while all tracks are in Empty / Stopped state.
    {
        juce::ScopedLock lock(mSnapshotLock);

        mSnapshot.clear();
        mSnapshot.reserve(tracks.size());

        for (const auto& track : tracks)
        {
            LoopFileHandler::TrackSaveData data;
            if (track && track->hasLoop())
            {
                data.buffer          = track->getLoopBuffer();   // AudioBuffer copy
                data.loopLengthSamples = track->getLoopLengthSamples();
                data.hasAudio        = true;
            }
            mSnapshot.push_back(std::move(data));
        }

        mDestination = destination;
        mSampleRate  = sampleRate;
        mBpm         = bpm;
    }

    mIsSaving.store(true);
    mTrigger.signal();
    return true;
}

void BackgroundSaveThread::run()
{
    while (!threadShouldExit())
    {
        mTrigger.wait(-1);  // block until triggerSave() or destructor signals

        if (threadShouldExit())
            break;

        // Take a local copy of the snapshot metadata so we release the lock
        // before doing potentially slow disk I/O.
        juce::File                                  dest;
        std::vector<LoopFileHandler::TrackSaveData> snapshot;
        double sampleRate;
        float  bpm;

        {
            juce::ScopedLock lock(mSnapshotLock);
            dest       = mDestination;
            snapshot   = mSnapshot;     // copies AudioBuffers
            sampleRate = mSampleRate;
            bpm        = mBpm;
        }

        const bool ok = mFileHandler.saveProjectFromSnapshot(dest, snapshot, sampleRate, bpm);

        if (ok)
            DBG("BackgroundSaveThread: saved to " + dest.getFullPathName());
        else
            DBG("BackgroundSaveThread: save failed");

        mIsSaving.store(false);
    }
}
