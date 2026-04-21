//
// Created by George Doujaiji on 2/21/26.
//

#include <juce_audio_processors/juce_audio_processors.h>
#include "LoopTrack.h"

class LoopTrackTests : public juce::UnitTest
{
public:
    LoopTrackTests() : juce::UnitTest("LoopTrackTests") {}

    void runTest() override
    {
        beginTest("New track starts in Empty state");
        {
            LoopTrack track(0);
            expect(track.getState() == LoopTrack::State::Empty, "Initial state should be Empty");
            expect(!track.hasLoop(), "New track should not have loop");
            expect(!track.isArmed(), "New track should not be armed");
        }

        beginTest("Arm -> Start Recording -> Stop Recording transitions");
        {
             SyncEngine sync;
             sync.prepare(48000.0, 256);

            LoopTrack track(0);
            track.prepareToPlay(48000.0, 256, 2);

            // Arm track
            track.armForRecording(true);
            expect(track.isArmed(), "Track should be armed");
            expect(track.getState() == LoopTrack::State::Empty, "State still Empty after arming");

            // Start recording
            track.startRecording(0);
            expect(track.getState() == LoopTrack::State::Recording, "State should be Recording");

            // Stop recording
            track.stopRecording();
            expect(track.getState() == LoopTrack::State::Playing, "State should transition to Playing after stop");
            expect(track.hasLoop(), "Track should have loop after recording");
        }

        beginTest("Cannot start recording if not armed");
        {
            LoopTrack track(0);
            track.prepareToPlay(48000.0, 256, 2);

            // Try to start recording without arming
            track.startRecording(0);

            // Should remain in Empty state
            expect(track.getState() == LoopTrack::State::Empty,
                "Should not enter Recording state without arming");
        }

        beginTest("Clear returns track to Empty state");
        {
            SyncEngine sync;
            sync.prepare(48000.0, 256);

            LoopTrack track(0);
            track.prepareToPlay(48000.0, 256, 2);

            // Record something
            track.armForRecording(true);
            track.startRecording(0);

            // Simulate recording by processing some blocks
            juce::AudioBuffer<float> input(2, 256);
            juce::AudioBuffer<float> output(2, 256);
            input.clear();
            for (int i = 0; i < 10; ++i) // 10 blocks
                track.processBlock(input, output, sync);

            track.stopRecording();
            expect(track.hasLoop(), "Should have loop before clear");

            // Clear
            track.clear();
            expect(track.getState() == LoopTrack::State::Empty, "State should be Empty after clear");
            expect(!track.hasLoop(), "Should not have loop after clear");
            expect(track.getLoopLengthSamples() == 0, "Loop length should be 0");
        }

        beginTest("Playing -> Stop -> Playing transitions");
        {
            SyncEngine sync;
            sync.prepare(48000.0, 256);

            LoopTrack track(0);
            track.prepareToPlay(48000.0, 256, 2);

            // Record a loop
            track.armForRecording(true);
            track.startRecording(0);
            juce::AudioBuffer<float> input(2, 256);
            juce::AudioBuffer<float> output(2, 256);
            input.clear();
            for (int i = 0; i < 10; ++i)
                track.processBlock(input, output, sync);
            track.stopRecording();

            expect(track.getState() == LoopTrack::State::Playing, "Should be Playing after recording");

            // Stop playback
            track.stopPlayback();
            expect(track.getState() == LoopTrack::State::Stopped, "Should be Stopped");

            // Restart playback
            track.startPlayback();
            expect(track.getState() == LoopTrack::State::Playing, "Should return to Playing");
        }

    }
};

static LoopTrackTests loopTrackTests;
