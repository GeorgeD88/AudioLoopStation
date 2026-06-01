/**
 * FourTrackSyncTests.cpp
 *
 * Integration tests for "Four-track playback & sync":
 *   - Record Track 1 (master) for a fixed number of blocks.
 *   - Add Tracks 2–4 as slave loops with different multipliers (×1.0, ×2.0, ×0.5).
 *   - Verify each slave finishes recording at the correct loop length.
 *   - Run 1 000 blocks of playback and confirm all tracks stay in Playing state.
 *   - Spot-check that the master's output matches its recorded content at a
 *     known global-sample offset.
 *
 * Uses LoopTrack directly (no JUCE audio device or plugin host required).
 * processBlock is called with a silent input buffer; the state-machine
 * transitions and position calculations are what are under test.
 */

#include <juce_audio_basics/juce_audio_basics.h>
#include "LoopTrack.h"

// ---------------------------------------------------------------------------
// Shared helpers
// ---------------------------------------------------------------------------

/** Feed one silent block through a track. */
static void stepTrack(LoopTrack& track,
                      juce::int64 globalSamples,
                      bool isMaster,
                      int masterLoopLength,
                      int blockSize)
{
    juce::AudioBuffer<float> output(2, blockSize);
    juce::AudioBuffer<float> input(2, blockSize);
    juce::AudioBuffer<float> sc(2, blockSize);
    output.clear();
    input.clear();
    sc.clear();
    track.processBlock(output, input, sc, globalSamples, isMaster, masterLoopLength, false);
}

/** Feed one block with a known input signal and return the output buffer. */
static juce::AudioBuffer<float> stepTrackGetOutput(LoopTrack& track,
                                                   const juce::AudioBuffer<float>& input,
                                                   juce::int64 globalSamples,
                                                   bool isMaster,
                                                   int masterLoopLength)
{
    const int blockSize = input.getNumSamples();
    juce::AudioBuffer<float> output(2, blockSize);
    juce::AudioBuffer<float> sc(2, blockSize);
    output.clear();
    sc.clear();
    track.processBlock(output, input, sc, globalSamples, isMaster, masterLoopLength, false);
    return output;
}

// ===========================================================================
class FourTrackSyncTests : public juce::UnitTest
{
public:
    FourTrackSyncTests() : juce::UnitTest("FourTrackSyncTests") {}

    void runTest() override
    {
        constexpr int kBlockSize        = 256;
        constexpr int kMasterBlocks     = 4;            // record 4 blocks
        constexpr int kMasterLoopLength = kBlockSize * kMasterBlocks;  // 1024 samples

        // -------------------------------------------------------------------
        // Test 1 - master loop length matches recorded block count
        // -------------------------------------------------------------------
        beginTest("master track loop length equals recorded block count");
        {
            LoopTrack master;
            master.prepareToPlay(48000.0, kBlockSize);

            master.setRecording();
            expect(master.getState() == LoopTrack::State::Recording,
                   "Master should enter Recording state.");

            juce::int64 gs = 0;
            for (int b = 0; b < kMasterBlocks; ++b)
            {
                stepTrack(master, gs, true, 0, kBlockSize);
                gs += kBlockSize;
            }

            master.setPlaying();

            expect(master.getState() == LoopTrack::State::Playing,
                   "Master should be Playing after setPlaying().");
            expectEquals(master.getLoopLengthSamples(), kMasterLoopLength,
                         "Master loop length must equal the number of recorded samples.");
            expect(master.hasLoop(), "master.hasLoop() must return true.");
        }

        // -------------------------------------------------------------------
        // Test 2 - slave ×1.0 records same length as master
        // -------------------------------------------------------------------
        beginTest("slave track multiplier 1.0 records same length as master");
        {
            LoopTrack master, slave;
            master.prepareToPlay(48000.0, kBlockSize);
            slave.prepareToPlay(48000.0, kBlockSize);

            // Record master
            master.setRecording();
            juce::int64 gs = 0;
            for (int b = 0; b < kMasterBlocks; ++b)
            {
                stepTrack(master, gs, true, 0, kBlockSize);
                gs += kBlockSize;
            }
            master.setPlaying();

            // Slave with ×1.0 multiplier
            slave.setTargetMultiplier(1.0f);
            slave.setRecording();
            expect(slave.getState() == LoopTrack::State::Recording,
                   "Slave should be Recording.");

            // Slave needs kMasterBlocks blocks to finish (1024 / 256 = 4)
            for (int b = 0; b < kMasterBlocks + 2; ++b)   // +2 buffer
            {
                stepTrack(slave, gs, false, kMasterLoopLength, kBlockSize);
                stepTrack(master, gs, true, kMasterLoopLength, kBlockSize);
                gs += kBlockSize;
            }

            expect(slave.getState() == LoopTrack::State::Playing,
                   "Slave (×1.0) should have auto-transitioned to Playing.");
            expectEquals(slave.getLoopLengthSamples(), kMasterLoopLength,
                         "Slave (×1.0) loop length must equal the master loop length.");
        }

        // -------------------------------------------------------------------
        // Test 3 - slave ×2.0 records double the master length
        // -------------------------------------------------------------------
        beginTest("slave track multiplier 2.0 records double master length");
        {
            LoopTrack master, slave;
            master.prepareToPlay(48000.0, kBlockSize);
            slave.prepareToPlay(48000.0, kBlockSize);

            master.setRecording();
            juce::int64 gs = 0;
            for (int b = 0; b < kMasterBlocks; ++b)
            {
                stepTrack(master, gs, true, 0, kBlockSize);
                gs += kBlockSize;
            }
            master.setPlaying();

            slave.setTargetMultiplier(2.0f);
            slave.setRecording();

            // Slave needs kMasterBlocks*2 blocks = 8 blocks
            const int blocksNeeded = kMasterBlocks * 2 + 2;   // +2 buffer
            for (int b = 0; b < blocksNeeded; ++b)
            {
                stepTrack(slave, gs, false, kMasterLoopLength, kBlockSize);
                stepTrack(master, gs, true, kMasterLoopLength, kBlockSize);
                gs += kBlockSize;
            }

            expect(slave.getState() == LoopTrack::State::Playing,
                   "Slave (×2.0) should have auto-transitioned to Playing.");
            expectEquals(slave.getLoopLengthSamples(), kMasterLoopLength * 2,
                         "Slave (×2.0) loop length must be twice the master loop length.");
        }

        // -------------------------------------------------------------------
        // Test 4 - slave ×0.5 records half the master length
        // -------------------------------------------------------------------
        beginTest("slave track multiplier 0.5 records half master length");
        {
            LoopTrack master, slave;
            master.prepareToPlay(48000.0, kBlockSize);
            slave.prepareToPlay(48000.0, kBlockSize);

            master.setRecording();
            juce::int64 gs = 0;
            for (int b = 0; b < kMasterBlocks; ++b)
            {
                stepTrack(master, gs, true, 0, kBlockSize);
                gs += kBlockSize;
            }
            master.setPlaying();

            slave.setTargetMultiplier(0.5f);
            slave.setRecording();

            // Slave needs kMasterBlocks/2 blocks = 2 blocks
            const int blocksNeeded = kMasterBlocks / 2 + 2;   // +2 buffer
            for (int b = 0; b < blocksNeeded; ++b)
            {
                stepTrack(slave, gs, false, kMasterLoopLength, kBlockSize);
                stepTrack(master, gs, true, kMasterLoopLength, kBlockSize);
                gs += kBlockSize;
            }

            expect(slave.getState() == LoopTrack::State::Playing,
                   "Slave (×0.5) should have auto-transitioned to Playing.");
            expectEquals(slave.getLoopLengthSamples(), kMasterLoopLength / 2,
                         "Slave (×0.5) loop length must be half the master loop length.");
        }

        // -------------------------------------------------------------------
        // Test 5 - all four tracks remain Playing after 1 000 blocks
        //
        // This is the primary regression guard for the integration task:
        // "Let playback run for an extended period; verify all tracks
        // stay in sync" - no track should silently drop to Empty/Stopped.
        // -------------------------------------------------------------------
        beginTest("all four tracks remain in Playing state after 1000 blocks");
        {
            // Track 0 = master  (×1.0, implicit)
            // Track 1 = slave1  (×1.0)
            // Track 2 = slave2  (×2.0)
            // Track 3 = slave3  (×0.5)
            LoopTrack master, slave1, slave2, slave3;
            master.prepareToPlay(48000.0, kBlockSize);
            slave1.prepareToPlay(48000.0, kBlockSize);
            slave2.prepareToPlay(48000.0, kBlockSize);
            slave3.prepareToPlay(48000.0, kBlockSize);

            // --- Phase 1: record master ---
            master.setRecording();
            juce::int64 gs = 0;
            for (int b = 0; b < kMasterBlocks; ++b)
            {
                stepTrack(master, gs, true, 0, kBlockSize);
                gs += kBlockSize;
            }
            master.setPlaying();
            const int mLen = master.getLoopLengthSamples();

            // --- Phase 2: start slaves recording ---
            slave1.setTargetMultiplier(1.0f);
            slave2.setTargetMultiplier(2.0f);
            slave3.setTargetMultiplier(0.5f);

            slave1.setRecording();
            slave2.setRecording();
            slave3.setRecording();

            // Process until the slowest slave (×2.0) finishes + a small buffer
            const int recordingPhaseBlocks = kMasterBlocks * 2 + 4;
            for (int b = 0; b < recordingPhaseBlocks; ++b)
            {
                stepTrack(master, gs, true,  mLen, kBlockSize);
                stepTrack(slave1, gs, false, mLen, kBlockSize);
                stepTrack(slave2, gs, false, mLen, kBlockSize);
                stepTrack(slave3, gs, false, mLen, kBlockSize);
                gs += kBlockSize;
            }

            // All slaves must have transitioned to Playing by now
            expect(slave1.getState() == LoopTrack::State::Playing,
                   "Slave1 (×1.0) must be Playing before the extended-playback phase.");
            expect(slave2.getState() == LoopTrack::State::Playing,
                   "Slave2 (×2.0) must be Playing before the extended-playback phase.");
            expect(slave3.getState() == LoopTrack::State::Playing,
                   "Slave3 (×0.5) must be Playing before the extended-playback phase.");

            // --- Phase 3: 1 000 blocks of sustained playback ---
            for (int b = 0; b < 1000; ++b)
            {
                stepTrack(master, gs, true,  mLen, kBlockSize);
                stepTrack(slave1, gs, false, mLen, kBlockSize);
                stepTrack(slave2, gs, false, mLen, kBlockSize);
                stepTrack(slave3, gs, false, mLen, kBlockSize);
                gs += kBlockSize;
            }

            expect(master.getState() == LoopTrack::State::Playing,
                   "Master must still be Playing after 1000 blocks.");
            expect(slave1.getState() == LoopTrack::State::Playing,
                   "Slave1 (×1.0) must still be Playing after 1000 blocks.");
            expect(slave2.getState() == LoopTrack::State::Playing,
                   "Slave2 (×2.0) must still be Playing after 1000 blocks.");
            expect(slave3.getState() == LoopTrack::State::Playing,
                   "Slave3 (×0.5) must still be Playing after 1000 blocks.");
        }

        // -------------------------------------------------------------------
        // Test 6 - master playback position aligns with global sample counter
        //
        // Fills the master loop with a linear ramp, then calls processBlock at
        // a known global-sample offset and verifies the first output sample
        // matches the ramp value at that position (position = gs % loopLength).
        // -------------------------------------------------------------------
        beginTest("master playback position aligns with global sample counter");
        {
            LoopTrack master;
            master.prepareToPlay(48000.0, kBlockSize);

            // Fill master loop with a linear ramp [0, 1) over kMasterLoopLength samples.
            juce::AudioBuffer<float> ramp(2, kMasterLoopLength);
            for (int ch = 0; ch < 2; ++ch)
                for (int s = 0; s < kMasterLoopLength; ++s)
                    ramp.setSample(ch, s, static_cast<float>(s) / static_cast<float>(kMasterLoopLength));

            // setLoopFromMix also puts the track into Playing state.
            master.setLoopFromMix(ramp, kMasterLoopLength, 0, 0);

            expect(master.getState() == LoopTrack::State::Playing,
                   "setLoopFromMix should leave the track in Playing state.");

            // Ask for a block starting exactly at globalSamples = 256.
            // Expected read position = 256 % 1024 = 256.
            // Expected first output sample = ramp[256] = 256/1024 ≈ 0.25.
            const juce::int64 queryGs = 256;
            juce::AudioBuffer<float> silentInput(2, kBlockSize);
            silentInput.clear();

            const juce::AudioBuffer<float> out =
                stepTrackGetOutput(master, silentInput, queryGs, true, kMasterLoopLength);

            const float expected = static_cast<float>(queryGs % kMasterLoopLength)
                                 / static_cast<float>(kMasterLoopLength);
            expectWithinAbsoluteError(out.getSample(0, 0), expected, 0.01f,
                                      "First output sample must correspond to ramp[gs % loopLength].");
            expectWithinAbsoluteError(out.getSample(1, 0), expected, 0.01f,
                                      "Right channel must match left channel for a centre-panned mono ramp.");

            // Advance by exactly one full master loop length; position wraps to 0.
            const juce::int64 wrappedGs = queryGs + kMasterLoopLength;
            const juce::AudioBuffer<float> outWrapped =
                stepTrackGetOutput(master, silentInput, wrappedGs, true, kMasterLoopLength);

            // After wrapping, read position == queryGs % loopLength == 256, same samples.
            expectWithinAbsoluteError(outWrapped.getSample(0, 0), expected, 0.01f,
                                      "After one full loop, output must wrap back to the same position.");
        }
    }
};

static FourTrackSyncTests fourTrackSyncTests;
