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

    int bufferSize = static_cast<int>(sampleRate * maxLengthSeconds);
    bufferSize = juce::jmax(bufferSize, static_cast<int>(sampleRate));

    buffer.setSize(channels, bufferSize);
    buffer.clear();
    fifo.setTotalSize(bufferSize);

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

    const auto scope = fifo.write(numSamples);

    if (scope.blockSize1 > 0)
        writeToBuffer(scope.startIndex1, source,
                      0,scope.blockSize1);
    if (scope.blockSize2 > 0)
        writeToBuffer(scope.startIndex2, source,
                      scope.blockSize1, scope.blockSize2);

    int currentWritePos = writePosition.load();
    int bufferSize = buffer.getNumSamples();
    writePosition.store((currentWritePos + numSamples) % bufferSize);

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

    if (numSamples <= 0 || currentLoopLength == 0)
    {
        destination.clear();
        return false;
    }

    if (isLooping.load())
    {
        int currentReadPosition = readPosition.load();
        int start1 = currentReadPosition;
        int size1 = juce::jmin(numSamples, currentLoopLength - currentReadPosition);
        int start2 = 0;
        int size2 = (numSamples > size1) ? (numSamples - size1) % currentLoopLength : 0;

        if (size1 > 0)
            readFromBuffer(start1, destination,
                           0, size1);
        if (size2 > 0)
            readFromBuffer(start2, destination,
                           size1, size2);
    }
    else
    {
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

        // Ensure read pos is within loop bounds
        int currentReadPosition = readPosition.load();
        if (currentReadPosition >= samples)
        {
            readPosition.store(currentReadPosition % samples);
        }

        // Ensure write position is within loop bounds
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

void CircularBuffer::writeToBuffer(int destStart, const juce::AudioBuffer<float>& source, int sourceStart,
                                   int numSamples) {
    const int numChannels = juce::jmin(buffer.getNumChannels(), source.getNumChannels());
    for (int channel = 0; channel < numChannels; ++channel)
    {
        buffer.copyFrom(channel, destStart, source,
                        channel,sourceStart, numSamples);
    }
}

void CircularBuffer::readFromBuffer(int sourceStart, juce::AudioBuffer<float>& destBuffer, int destStart,
                                    int numSamples) {
    const int numChannels = juce::jmin(buffer.getNumChannels(), destBuffer.getNumChannels());
    for (int channel = 0; channel < numChannels; ++channel)
    {
        destBuffer.copyFrom(channel, destStart, buffer,
                            channel, sourceStart, numSamples);
    }
}
