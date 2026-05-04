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
            LoopTrack track;
            expect(track.getState() == LoopTrack::State::Empty, "Initial state should be Empty");
            expect(!track.hasLoop(), "New track should not have loop");
        }

        beginTest("Recording -> Playing transition creates a loop");
        {
            LoopTrack track;
            track.prepareToPlay(48000.0, 256);

            track.setRecording();
            expect(track.getState() == LoopTrack::State::Recording, "State should be Recording");

            juce::AudioBuffer<float> input(2, 256);
            juce::AudioBuffer<float> output(2, 256);
            juce::AudioBuffer<float> sidechain(2, 256);
            input.clear();
            output.clear();
            sidechain.clear();

            track.processBlock(output, input, sidechain, 0, true, 0, false);
            track.setPlaying();

            expect(track.getState() == LoopTrack::State::Playing, "State should transition to Playing");
            expect(track.hasLoop(), "Track should have loop after recording");
        }

        beginTest("Clear returns track to Empty state");
        {
            LoopTrack track;
            track.prepareToPlay(48000.0, 256);

            juce::AudioBuffer<float> input(2, 256);
            juce::AudioBuffer<float> output(2, 256);
            juce::AudioBuffer<float> sidechain(2, 256);
            input.clear();
            output.clear();
            sidechain.clear();

            track.setRecording();
            track.processBlock(output, input, sidechain, 0, true, 0, false);
            track.setPlaying();

            expect(track.hasLoop(), "Should have loop before clear");

            track.clear();
            expect(track.getState() == LoopTrack::State::Empty, "State should be Empty after clear");
            expect(!track.hasLoop(), "Should not have loop after clear");
            expect(track.getLoopLengthSamples() == 0, "Loop length should be 0");
        }

        beginTest("Playing -> Stop -> Playing transitions");
        {
            LoopTrack track;
            track.prepareToPlay(48000.0, 256);

            juce::AudioBuffer<float> input(2, 256);
            juce::AudioBuffer<float> output(2, 256);
            juce::AudioBuffer<float> sidechain(2, 256);
            input.clear();
            output.clear();
            sidechain.clear();

            track.setRecording();
            track.processBlock(output, input, sidechain, 0, true, 0, false);
            track.setPlaying();

            expect(track.getState() == LoopTrack::State::Playing, "Should be Playing after recording");

            track.stop();
            expect(track.getState() == LoopTrack::State::Stopped, "Should be Stopped");

            track.setPlaying();
            expect(track.getState() == LoopTrack::State::Playing, "Should return to Playing");
        }
    }
};

static LoopTrackTests loopTrackTests;
