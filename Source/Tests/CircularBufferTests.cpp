//
// Created by George Doujaiji on 2/21/26.
//

#include <juce_audio_processors/juce_audio_processors.h>
#include "CircularBuffer.h"

class CircularBufferTests : public juce::UnitTest
{
public:
    CircularBufferTests() : juce::UnitTest("CircularBufferTests") {}

    void runTest() override
    {
        beginTest("Write then read returns same samples");
        {
            CircularBuffer buffer;
            buffer.prepareToPlay(48000.0, 2, 10.0); // 48kHz, stereo, 10 sec max

            // Create test signal: 1.0 on left, -1.0 on right
            juce::AudioBuffer<float> writeBuffer(2, 64);
            writeBuffer.clear();
            for (int i = 0; i < 64; ++i)
            {
                writeBuffer.setSample(0, i, 1.0f);  // left
                writeBuffer.setSample(1, i, -1.0f); // right
            }

            // Write
            bool writeSuccess = buffer.write(writeBuffer);
            expect(writeSuccess, "Write should succeed");

            // Read back
            juce::AudioBuffer<float> readBuffer(2, 64);
            bool readSuccess = buffer.read(readBuffer);
            expect(readSuccess, "Read should succeed");

            // Verify samples match
            for (int ch = 0; ch < 2; ++ch)
            {
                for (int i = 0; i < 64; ++i)
                {
                    float written = writeBuffer.getSample(ch, i);
                    float readBack = readBuffer.getSample(ch, i);
                    expectWithinAbsoluteError(readBack, written, 0.0001f,
                        "Sample mismatch at ch=" + juce::String(ch) + " i=" + juce::String(i));
                }
            }
        }

        beginTest("Loop wraps correctly at boundary");
        {
            CircularBuffer buffer;
            buffer.prepareToPlay(48000.0, 1, 1.0); // 48kHz, mono

            // Set small loop length
            int loopLen = 100;
            buffer.setLoopLength(loopLen);
            buffer.setLooping(true);

            // Write loop-length worth of ascending ramp
            juce::AudioBuffer<float> writeBuffer(1, loopLen);
            for (int i = 0; i < loopLen; ++i)
                writeBuffer.setSample(0, i, static_cast<float>(i) / loopLen);

            buffer.write(writeBuffer);

            // Read multiple times to ensure loop wraps
            for (int iteration = 0; iteration < 3; ++iteration)
            {
                juce::AudioBuffer<float> readBuffer(1, loopLen);
                bool success = buffer.read(readBuffer);
                expect(success, "Read iteration " + juce::String(iteration) + " should succeed");

                // Verify ramp repeated
                for (int i = 0; i < loopLen; ++i)
                {
                    float expected = static_cast<float>(i) / loopLen;
                    float actual = readBuffer.getSample(0, i);
                    expectWithinAbsoluteError(actual, expected, 0.0001f,
                        "Loop wrap failed at iteration=" + juce::String(iteration) + " sample=" + juce::String(i));
                }
            }
        }

        beginTest("Read with partial fill returns correct samples");
        {
            CircularBuffer buffer;
            buffer.prepareToPlay(48000.0, 1, 1.0);

            // Write only 32 samples
            juce::AudioBuffer<float> writeBuffer(1, 32);
            for (int i = 0; i < 32; ++i)
                writeBuffer.setSample(0, i, 0.5f);
            buffer.write(writeBuffer);

            // Try to read 64 samples (more than available)
            juce::AudioBuffer<float> readBuffer(1, 64);
            readBuffer.clear();
            bool success = buffer.read(readBuffer);

            // Should either fail or read what's available
            if (success)
            {
                // If it succeeded, at least first 32 should be correct
                for (int i = 0; i < 32; ++i)
                    expectWithinAbsoluteError(readBuffer.getSample(0, i), 0.5f, 0.0001f,
                        "Partial read sample mismatch");
            }
        }

        beginTest("Empty buffer read returns false or zeros");
        {
            CircularBuffer buffer;
            buffer.prepareToPlay(48000.0, 1, 1.0);

            juce::AudioBuffer<float> readBuffer(1, 64);
            readBuffer.clear();
            for (int i = 0; i < 64; ++i)
                readBuffer.setSample(0, i, 999.0f); // sentinel value

            bool success = buffer.read(readBuffer);

            if (!success)
            {
                // Expected: read fails on empty buffer
                expect(true, "Read correctly failed on empty buffer");
            }
            else
            {
                // Or: read succeeds but returns zeros
                bool allZero = true;
                for (int i = 0; i < 64; ++i)
                {
                    if (std::abs(readBuffer.getSample(0, i)) > 0.0001f)
                    {
                        allZero = false;
                        break;
                    }
                }
                expect(allZero, "Empty buffer should return zeros if read succeeds");
            }
        }

        beginTest("Clear resets buffer state");
        {
            CircularBuffer buffer;
            buffer.prepareToPlay(48000.0, 1, 1.0);

            // Write some data
            juce::AudioBuffer<float> writeBuffer(1, 64);
            for (int i = 0; i < 64; ++i)
                writeBuffer.setSample(0, i, 1.0f);
            buffer.write(writeBuffer);

            // Verify data is there
            int available = buffer.getAvailableSamples();
            expect(available > 0, "Should have available samples after write");

            // Clear
            buffer.clear();

            // Verify empty
            int availableAfterClear = buffer.getAvailableSamples();
            expect(availableAfterClear == 0, "Should have 0 samples after clear");
        }
    }
};

static CircularBufferTests circularBufferTests;