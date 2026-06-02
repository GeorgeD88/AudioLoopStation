//
// Created by George Doujaiji on 4/28/26
//
// Integration tests for the Track 1 record/playback workflow.
// Validates core user journey: open plugin, record, playback.
// Simulating the DAW actions rather than reaching into internal state.
//

#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include <thread>
#include <vector>
#include <cmath>

#include "../PluginProcessor.h"
#include "../Utils/Config.h"

class RecordPlaybackIntegrationTests : public juce::UnitTest
{
public:
    RecordPlaybackIntegrationTests() : juce::UnitTest("RecordPlaybackIntegrationTests") {}

    void runTest() override
    {
        // === Foundation ===
        testProcessorCreation();
        testProcessorInitialization();
        testSampleRateChanges();

        // === Audio Pipeline ===
        testEmptyBlockProcessing();
        testSustainedProcessing();
        testEdgeCases();

        // === Component Access ===
        testComponentAccess();
        testTrackStates();

        // === Performance ===
        testProcessingLatency();
        testNoUIBlocking();             // Task 17    - TODO: Performance

        // === Task Focused Tests ===
        testRecordPlaybackCycle();      // Task 18    - TODO: Record/Playback
        testFourTrackPlayback();        // Task 23/41 - TODO: Multi-track
    }

/* === Goal is to deal with these tasks, big contribution! ===:
 * Task 18: Test recording → playback cycle (framework in place)
 * Task 17: Verify audio thread performance
 * Task 23: Foundation for 4-track testing
 * Sprint 1 Goal: "Add end-to-end single track integration test path"
 */

private:
    // === Foundation ===

    void testProcessorCreation()
    {
        beginTest("Processor creates successfully");
        {
            AudioLoopStationAudioProcessor processor;  // initialize (constructor) processor
            expect(true, "Processor constructed");
        }
    }

    void testProcessorInitialization()
    {
        beginTest("Processor initializes for audio processing");
        {
            AudioLoopStationAudioProcessor processor;
            processor.prepareToPlay(Config::SampleRate::DEFAULT, Config::DEFAULT_BUFFER_SIZE);
            expect(true, "prepareToPlay completed");
        }
    }

    void testSampleRateChanges()
    {
        beginTest("Handles sample rate changes");
        {
            AudioLoopStationAudioProcessor processor;

            // Simulate user changing audio interface settings (run prepareToPlay again after initial run)
            processor.setRateAndBufferSizeDetails(44100.0, 512);  // must emulate DAW updating base-class getters
            processor.prepareToPlay(44100.0, 512);
            processor.setRateAndBufferSizeDetails(48000.0, 256);
            processor.prepareToPlay(48000.0, 256);
            processor.setRateAndBufferSizeDetails(96000.0, 128);
            processor.prepareToPlay(96000.0, 128);

            expect(true, "Adapted to sample rate changes");
        }
    }


    // === Audio Pipeline ===

    void testEmptyBlockProcessing()
    {
        beginTest("Process empty audio blocks without artifacts");
        {
            AudioLoopStationAudioProcessor processor;
            processor.prepareToPlay(Config::SampleRate::DEFAULT, Config::DEFAULT_BUFFER_SIZE);

            // Use 2 channels for stereo
            juce::AudioBuffer<float> buffer(2, Config::DEFAULT_BUFFER_SIZE);
            buffer.clear();
            juce::MidiBuffer midi;

            processor.processBlock(buffer, midi);  // process empty buffer

            expect(isSilent(buffer), "Output should be silent for empty input");
        }
    }

    void testSustainedProcessing()
    {
        beginTest("Sustained processing remains stable");
        {
            AudioLoopStationAudioProcessor processor;
            processor.prepareToPlay(Config::SampleRate::DEFAULT, Config::DEFAULT_BUFFER_SIZE);

            juce::AudioBuffer<float> buffer(2, Config::DEFAULT_BUFFER_SIZE);
            juce::MidiBuffer midi;

            // Simulate 5 seconds of continuous playback (5s for sustained, continuous processing)
            int numBlocks = static_cast<int>(5.0 * Config::SampleRate::DEFAULT / Config::DEFAULT_BUFFER_SIZE);

            for (int i = 0; i < numBlocks; ++i)
            {
                buffer.clear();
                processor.processBlock(buffer, midi);
            }

            expect(true, "Processed " + juce::String(numBlocks) + " blocks without crash");
        }
    }

    void testEdgeCases()
    {
        beginTest("Edge cases: zero-length and variable blocks");
        {
            AudioLoopStationAudioProcessor processor;
            processor.prepareToPlay(Config::SampleRate::DEFAULT, 1024);

            juce::MidiBuffer midi;

            // Zero-length block (some hosts send these during transport changes)   - edge test 1
            juce::AudioBuffer<float> zeroBlock(2, 0);
            processor.processBlock(zeroBlock, midi);

            // Variable block sizes (real-world DAWs do this)                       - edge test 2
            std::vector<int> blockSizes = {32, 64, 128, 256, 512};
            for (int size : blockSizes)
            {
                juce::AudioBuffer<float> buffer(2, size);
                buffer.clear();
                processor.processBlock(buffer, midi);
            }

            expect(true, "Handled edge cases");
        }
    }


    // === Component Access ===

    void testComponentAccess()
    {
        beginTest("Can access core components");
        {
            AudioLoopStationAudioProcessor processor;
            processor.prepareToPlay(Config::SampleRate::DEFAULT, Config::DEFAULT_BUFFER_SIZE);

            auto& tracks = processor.getTracks();
            expect(tracks.size() == Config::NUM_TRACKS, "Correct track count");

            auto& mixerEngine = processor.getMixerEngine();
            expect(&mixerEngine != nullptr, "MixerEngine accessible");

            expect(processor.getBpm() > 0.0, "BPM initialized");
            expect(processor.getGlobalTotalSamples() == 0, "Global clock starts at zero");
            expect(processor.isFirstLoop(), "Starts in first-loop phase");
        }
    }

    void testTrackStates()
    {
        beginTest("All tracks start in correct state");
        {
            AudioLoopStationAudioProcessor processor;
            processor.prepareToPlay(Config::SampleRate::DEFAULT, Config::DEFAULT_BUFFER_SIZE);

            auto& tracks = processor.getTracks();
            int validTracks = 0;

            for (size_t i = 0; i < tracks.size(); ++i)
            {
                auto* track = tracks[i].get();

                expect(track != nullptr, "Track exists");
                expect(track->getState() == LoopTrack::State::Empty, "Track starts Empty");
                expect(!track->hasLoop(), "No audio data");

                // Keep track of all valid tracks
                if (track != nullptr
                    && track->getState() == LoopTrack::State::Empty
                    && !track->hasLoop())
                {
                    ++validTracks;
                }
            }

            expect(validTracks == (int)Config::NUM_TRACKS, "All tracks should start in valid state");
            logMessage("Verified " + juce::String(validTracks) + " tracks");
        }
    }


    // === Performance ===

    void testProcessingLatency()
    {
        beginTest("Processing latency within target");
        {
            AudioLoopStationAudioProcessor processor;
            processor.prepareToPlay(Config::SampleRate::DEFAULT, Config::DEFAULT_BUFFER_SIZE);

            juce::AudioBuffer<float> buffer(2, Config::DEFAULT_BUFFER_SIZE);
            juce::MidiBuffer midi;

            // Test latency of 100 blocks of playback
            const int numBlocks = 100;
            auto startTime = juce::Time::getMillisecondCounterHiRes();

            for (int i = 0; i < numBlocks; ++i)
            {
                buffer.clear();
                processor.processBlock(buffer, midi);
            }

            auto endTime = juce::Time::getMillisecondCounterHiRes();
            double avgTime = (endTime - startTime) / numBlocks;
            double allowedTime = (Config::DEFAULT_BUFFER_SIZE / Config::SampleRate::DEFAULT) * 1000.0;

            expect(avgTime < allowedTime, "Processing fast enough");

            logMessage("Average: " + juce::String(avgTime, 2) + "ms, Allowed: " +
                      juce::String(allowedTime, 2) + "ms");
        }
    }

    // Directly tackles Task 17
    void testNoUIBlocking()
    {
        // Audio thread must maintain operation within deadline while UI thread produces heavy concurrent load
        beginTest("Audio thread holds deadline under concurrent UI parameter writes");
        {
            AudioLoopStationAudioProcessor processor;
            processor.prepareToPlay(Config::SampleRate::DEFAULT, Config::DEFAULT_BUFFER_SIZE);

            auto& apvts = processor.apvts;
            const double deadlineMs = (Config::DEFAULT_BUFFER_SIZE / Config::SampleRate::DEFAULT) * 1000.0;

            std::atomic<bool> stopFlag { false };

            // "UI" thread - simulates track controls writing constantly
            std::thread uiThread([&apvts, &stopFlag]()
            {
                float v = 0.0f;
                while (!stopFlag.load())
                {
                    for (int t = 0; t < Config::NUM_TRACKS; ++t)
                    {
                        auto prefix = "Track" + juce::String(t + 1) + "_";
                        if (auto* vp = apvts.getRawParameterValue(prefix + "Volume")) vp->store(v);
                        if (auto* pp = apvts.getRawParameterValue(prefix + "Pan"))    pp->store(v * 2.0f - 1.0f);
                        if (auto* mp = apvts.getRawParameterValue(prefix + "Mute"))   mp->store((t % 2) ? 1.0f : 0.0f);
                        if (auto* sp = apvts.getRawParameterValue(prefix + "Solo"))   sp->store((t % 3) ? 1.0f : 0.0f);
                    }
                    v += 0.01f;
                    if (v > 1.0f) v = 0.0f;
                }
            });

            juce::AudioBuffer<float> buffer(2, Config::DEFAULT_BUFFER_SIZE);
            juce::MidiBuffer midi;

            const int numBlocks = 500;
            int deadlineViolations = 0;  // track deadline violations rather than failing
            double worstBlockMs = 0.0;

            for (int i = 0; i < numBlocks; ++i)
            {
                buffer.clear();
                auto t0 = juce::Time::getMillisecondCounterHiRes();
                processor.processBlock(buffer, midi);
                double elapsed = juce::Time::getMillisecondCounterHiRes() - t0;

                // Track deadline violations and worst delay
                if (elapsed > deadlineMs) ++deadlineViolations;
                if (elapsed > worstBlockMs) worstBlockMs = elapsed;
            }

            stopFlag.store(true);
            uiThread.join();

            logMessage("Deadline: " + juce::String(deadlineMs, 2) + "ms");
            logMessage("Worst block: " + juce::String(worstBlockMs, 2) + "ms");
            logMessage("Violations: " + juce::String(deadlineViolations) + "/" + juce::String(numBlocks));

            // Allow a small margin (~1%), the first few blocks often warm up caches
            // TODO: decide on best margin with team
            expect(deadlineViolations < numBlocks / 100,
                   "Audio thread should not be blocked by UI parameter writes");
        }
    }


    // === Task Focused Tests ===

    // Directly tackles Task 18 - Record/Playback
    void testRecordPlaybackCycle()
    {
        beginTest("Track 1: record -> play cycle");
        {
            AudioLoopStationAudioProcessor processor;
            processor.setRateAndBufferSizeDetails(Config::SampleRate::DEFAULT, Config::DEFAULT_BUFFER_SIZE);
            processor.prepareToPlay(Config::SampleRate::DEFAULT, Config::DEFAULT_BUFFER_SIZE);

            auto* track = processor.getTracks()[0].get();
            expect(track != nullptr, "Track 1 accessible");

            juce::MidiBuffer midi;

            // -- Phase 1: Initial state
            expect(track->getState() == LoopTrack::State::Empty, "Starts Empty");
            expect(!track->hasLoop(), "No loop yet");

            // -- Phase 2: Begin recording
            track->setRecording();
            expect(track->getState() == LoopTrack::State::Recording, "Enters Recording state");

            // Feed ~10 blocks of test signal
            const int recordBlocks = 10;
            for (int i = 0; i < recordBlocks; ++i)
            {
                auto input = createTestSignal(2, Config::DEFAULT_BUFFER_SIZE, Config::SampleRate::DEFAULT);
                processor.processBlock(input, midi);
            }

            // -- Phase 3: Transition to playback
            track->setPlaying();
            expect(track->getState() == LoopTrack::State::Playing, "Transitions to Playing");
            expect(track->hasLoop(), "Loop captured");
            expect(track->getLoopLengthSamples() > 0, "Loop length set");

            logMessage("Recorded loop length: " + juce::String(track->getLoopLengthSamples()) + " samples");

            // -- Phase 4: Silent input should still produce audible playback
            juce::AudioBuffer<float> playbackBuffer(2, Config::DEFAULT_BUFFER_SIZE);
            bool heardPlayback = false;
            for (int i = 0; i < 10; ++i)
            {
                playbackBuffer.clear();
                processor.processBlock(playbackBuffer, midi);
                if (!isSilent(playbackBuffer)) { heardPlayback = true; break; }
            }
            expect(heardPlayback, "Recorded loop plays back through master output");
        }
    }

    // Directly tackles Task 23/41 - Multi-track
    void testFourTrackPlayback()
    {
        beginTest("Layer 4 tracks sequentially, all play simultaneously");
        {
            AudioLoopStationAudioProcessor processor;
            processor.setRateAndBufferSizeDetails(Config::SampleRate::DEFAULT, Config::DEFAULT_BUFFER_SIZE);
            processor.prepareToPlay(Config::SampleRate::DEFAULT, Config::DEFAULT_BUFFER_SIZE);

            auto& tracks = processor.getTracks();
            juce::MidiBuffer midi;
            const int blocksPerTrack = 8;

            for (size_t t = 0; t < tracks.size(); ++t)
            {
                auto* track = tracks[t].get();
                expect(track != nullptr, "Track " + juce::String((int)t) + " accessible");

                track->setRecording();
                expect(track->getState() == LoopTrack::State::Recording,
                       "Track " + juce::String((int)t) + " enters Recording");

                for (int i = 0; i < blocksPerTrack; ++i)
                {
                    auto input = createTestSignal(2, Config::DEFAULT_BUFFER_SIZE, Config::SampleRate::DEFAULT);
                    processor.processBlock(input, midi);
                }

                track->setPlaying();
                expect(track->getState() == LoopTrack::State::Playing,
                       "Track " + juce::String((int)t) + " transitions to Playing");
                expect(track->hasLoop(),
                       "Track " + juce::String((int)t) + " has loop");
            }

            // Verify all 4 are still Playing after the layering pass
            int playingCount = 0;
            for (size_t t = 0; t < tracks.size(); ++t)
                if (tracks[t]->getState() == LoopTrack::State::Playing) ++playingCount;
            expect(playingCount == (int)Config::NUM_TRACKS, "All 4 tracks in Playing state");

            // All 4 tracks should mix into the master output
            juce::AudioBuffer<float> mixBuffer(2, Config::DEFAULT_BUFFER_SIZE);
            bool heardMix = false;
            for (int i = 0; i < 10; ++i)
            {
                mixBuffer.clear();
                processor.processBlock(mixBuffer, midi);
                if (!isSilent(mixBuffer)) { heardMix = true; break; }
            }
            expect(heardMix, "4-track mix produces audible output");

            logMessage("All " + juce::String((int)Config::NUM_TRACKS) + " tracks playing");
        }
    }


    // === Helpers ===

    // Generate a test tone (440Hz sine) for recording input
    juce::AudioBuffer<float> createTestSignal(int numChannels, int numSamples, double sampleRate)
    {
        juce::AudioBuffer<float> buffer(numChannels, numSamples);
        constexpr double freq = 440.0;

        for (int ch = 0; ch < numChannels; ++ch)
            for (int i = 0; i < numSamples; ++i)
                buffer.setSample(ch, i,
                    0.5f * std::sin(2.0 * juce::MathConstants<double>::pi * freq * i / sampleRate));

        return buffer;
    }

    // Confirms that every sample in the buffer is considered silent
    bool isSilent(const juce::AudioBuffer<float>& buffer)
    {
        constexpr float threshold = 0.0001f;    // less than -80dB considered inaudible

        // Confirm each and every sample in each channel is "silent"
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                if (std::abs(buffer.getSample(ch, i)) > threshold)
                    return false;  // returns early

        return true;    // All samples silent! :D
    }

};

static RecordPlaybackIntegrationTests recordPlaybackIntegrationTests;