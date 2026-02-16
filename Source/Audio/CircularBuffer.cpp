//
// Created by Vincewa Tran on 2/1/26.
//

#include "CircularBuffer.h"

CircularBuffer::CircularBuffer() : fifo(1024) {}

CircularBuffer::~CircularBuffer() {
    releaseResources();
}

void CircularBuffer::prepareToPlay(double sr, int channels, double maxLengthSeconds) {
    sampleRate = sr;

    // Calculate buffer size based on max loop duration, ensuring at least 1 second of buffer
    int bufferSize = static_cast<int>(sampleRate * maxLengthSeconds);
    bufferSize = juce::jmax(bufferSize, static_cast<int>(sampleRate));

    // Resize the underlying AudioBuffer and reset the AbstractFifo helper
    buffer.setSize(channels, bufferSize);
    buffer.clear();
    fifo.setTotalSize(bufferSize);

    // Reset all state atomics to ensure a clean start
    writePosition.store(0);
    readPosition.store(0);
    loopLength.store(0);
    isLooping.store(true);
}

void CircularBuffer::releaseResources() {
    buffer.setSize(0 , 0);
    fifo.reset();

    writePosition.store(0);
    readPosition.store(0);
    loopLength.store(0);
    isLooping.store(true);
}

bool CircularBuffer::write(const juce::AudioBuffer<float> &source) {
    const int numSamples = source.getNumSamples();
    if (numSamples <= 0) return false;

    // AbstractFifo calculates where to write. It returns two blocks:
    // blockSize1: space available at the end of the buffer
    // blockSize2: space available wrapped around to the beginning
    const auto scope = fifo.write(numSamples);

    if (scope.blockSize1 > 0)
        writeToBuffer(scope.startIndex1, source,
                      0,scope.blockSize1);
    if (scope.blockSize2 > 0)
        writeToBuffer(scope.startIndex2, source,
                      scope.blockSize1, scope.blockSize2);

    // Manually track write position for the looping logic (independent of FIFO)
    int currentWritePos = writePosition.load();
    int bufferSize = buffer.getNumSamples();
    writePosition.store((currentWritePos + numSamples) % bufferSize);

    // AUTO-SET LOOP LENGTH:
    // If this is the first recording pass (length is 0), the loop length grows
    // dynamically as we write.
    if (loopLength.load() == 0)
    {
        loopLength.store(numSamples);
        readPosition.store(0);
    }

    return true;
}

bool CircularBuffer::read(juce::AudioBuffer<float> &destination) {
    const int numSamples = destination.getNumSamples();
    const int currentLoopLength = loopLength.load();

    // Safety check: cannot read if nothing has been recorded (loopLength 0)
    if (numSamples <= 0 || currentLoopLength == 0)
    {
        destination.clear();
        return false;
    }

    if (isLooping.load())
    {
        // LOOPING MODE:
        // We ignore the standard FIFO read logic and manually calculate offsets
        // based on 'readPosition' modulo 'currentLoopLength'.

        int currentReadPosition = readPosition.load();
        int start1 = currentReadPosition;

        // Calculate how much we can read before hitting the end of the loop
        int size1 = juce::jmin(numSamples, currentLoopLength - currentReadPosition);

        // If we need more samples than remain in the loop, wrap back to 0
        int start2 = 0;
        int size2 = (numSamples > size1) ? (numSamples - size1) % currentLoopLength : 0;

        if (size1 > 0)
            readFromBuffer(start1, destination,
                           0, size1);
        if (size2 > 0)
            readFromBuffer(start2, destination,
                           size1, size2);

        // Advance read pointer, wrapping it within the loop bounds
        readPosition.store((currentReadPosition + numSamples) % currentLoopLength);
    }
    else
    {
        // ONE-SHOT MODE:
        // Use standard FIFO behavior. Once read, data is "consumed".
        const auto scope = fifo.read(numSamples);
        if (scope.blockSize1 > 0)
            readFromBuffer(scope.startIndex1, destination,
                           0, scope.blockSize1);
        if (scope.blockSize2 > 0)
            readFromBuffer(scope.startIndex2, destination,
                           scope.blockSize1, scope.blockSize2);
    }

    return true;
}

void CircularBuffer::setLoopLength(int samples)
{
    if (samples > 0 && samples <= buffer.getNumSamples()) {
        loopLength.store(samples);

        // BOUNDARY SAFETY CHECKS:
        // If the new loop length is shorter than the current read/write positions,
        // we must wrap them immediately to prevent reading garbage memory or silence
        // outside the new active loop area.

        int currentReadPosition = readPosition.load();
        if (currentReadPosition >= samples)
        {
            readPosition.store(currentReadPosition % samples);
        }

        int currentWritePosition = writePosition.load();
        if (currentWritePosition >= samples)
        {
            writePosition.store(currentWritePosition % samples);
        }
    }
}

void CircularBuffer::clear()
{
    buffer.clear();
    fifo.reset();
    writePosition.store(0);
    readPosition.store(0);
    loopLength.store(0);
    isLooping.store(true);
}

// Helper to handle copying across channels
void CircularBuffer::writeToBuffer(int destStart, const juce::AudioBuffer<float>& source, int sourceStart,
                                   int numSamples) {
    const int numChannels = juce::jmin(buffer.getNumChannels(), source.getNumChannels());
    for (int channel = 0; channel < numChannels; ++channel)
    {
        buffer.copyFrom(channel, destStart, source,
                        channel,sourceStart, numSamples);
    }
}

// Helper to handle copying across channels
void CircularBuffer::readFromBuffer(int sourceStart, juce::AudioBuffer<float>& destBuffer, int destStart,
                                    int numSamples) {
    const int numChannels = juce::jmin(buffer.getNumChannels(), destBuffer.getNumChannels());
    for (int channel = 0; channel < numChannels; ++channel)
    {
        destBuffer.copyFrom(channel, destStart, buffer,
                            channel, sourceStart, numSamples);
    }
}