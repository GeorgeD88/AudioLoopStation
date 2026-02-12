//
// Created by Vincewa Tran on 2/1/26.
//
#pragma once
#include <atomic>
#include "../Utils/TrackConfig.h"
#include "juce_audio_basics/juce_audio_basics.h"
#include "juce_core/juce_core.h"
#include "gin_dsp/gin_dsp.h"

class CircularBuffer {
public:
   CircularBuffer();
   ~CircularBuffer();

   // Buffer Management
   void prepareToPlay(double sampleRate, int channels,
                      double maxLengthSeconds = TrackConfig::MAX_LOOP_LENGTH_SECONDS);
   void releaseResources();

   // Write from audio thread, read from audio/GUI threads
   bool write(const juce::AudioBuffer<float>& source);                                  // Audio Thread
   bool read(juce::AudioBuffer<float>& destination);                                    // Audio Thread

   // Loop Control
   void setLooping(bool shouldLoop) noexcept { isLooping.store(shouldLoop); }
   bool getIsLooping() const noexcept { return isLooping.load(); }
   void setLoopLength(int samples);
   int getLoopLength() const noexcept { return loopLength.load(); }
   void resetPlayback() noexcept { readPosition.store(0); }
   int getReadPosition() const noexcept { return readPosition.load(); }

   // State Management
   void clear();  // Implemented in .cpp
   bool isEmpty() const noexcept { return fifo.getNumReady() == 0; }
   bool isFull() const noexcept { return fifo.getFreeSpace() == 0; }

   // Thread-safe getters (can be called from GUI thread)
   int getAvailableSamples() const noexcept { return fifo.getNumReady(); }
   int getFreeSpace() const noexcept { return fifo.getFreeSpace(); }
   int getNumChannels() const noexcept { return buffer.getNumChannels(); }
   int getSampleRate() const noexcept { return sampleRate;}

   float getFillPercentage() const noexcept
   {
       int total = fifo.getTotalSize();
       return total > 0 ? static_cast<float>(fifo.getNumReady()) / total : 0.0f;
   }

private:
    // Audio Buffer
    juce::AudioBuffer<float> buffer;                        // each channel is a vector
    // Thread-safe FIFO (ONE writer, MULTIPLE readers)
    juce::AbstractFifo fifo;
    double sampleRate = TrackConfig::DEFAULT_SAMPLE_RATE;

    // Position trackers for UI (atomic for thread safety)
    std::atomic<int> writePosition { 0 };
    std::atomic<bool> isLooping { true };
    std::atomic<int> readPosition { 0 };
    std::atomic<int> loopLength { 0 };

    // Private helpers
    void writeToBuffer(int startIndex, const juce::AudioBuffer<float>& source,
                       int sourceStart, int numSamples);
    void readFromBuffer(int startIndex, juce::AudioBuffer<float>& destination,
                        int destStart, int numSamples);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CircularBuffer);
};


