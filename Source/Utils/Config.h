//
// Created by Vincewa Tran on 2/25/26.
//
// AUDIO LOOP STATION - MASTER CONFIGURATION FILE
// Centralizes all reused configurable parameters. Change values here.
// Adjust performance and verify values here.
//
// Set values according to project requirements
// per https://eecs.engineering.oregonstate.edu/capstone/submission/pages/viewSingleProject.php?id=53bwHGZsurEf8EXi
// - at least 4 mono tracks OR 2 stereo tracks
// - simultaneous record/playback
// - at least 3 features from:
//   time stretch, pitch shift, loop trim, reverse, slip, pan

#pragma once
#include "juce_audio_basics/juce_audio_basics.h"
#include <cmath>
#include <cstdint>

namespace Config {

    /** CORE PROGRAM VALUES */
    constexpr int NUM_TRACKS = 4;                       // From PluginProcessor.h
    constexpr int MAX_LOOP_LENGTH_SECONDS = 300;        // From LoopTrack.h (5 minutes)
    constexpr int CROSSFADE_SAMPLES = 128;              // From LoopTrack.cpp
    constexpr int DEFAULT_BUFFER_SIZE = 256;
    constexpr int INVALID_TRACK_ID = -1;

    /** MIDI CLOCK SETTINGS */
    namespace MidiClock {
        constexpr int PPQN = 24;                        // Pulses per quarter note
        constexpr int PULSE_NOTE = 60;                  // C4 as sync pulse (from PluginProcessor)
    }

    /** SAMPLE RATE HANDLING */
    namespace SampleRate {
        constexpr double DEFAULT = 48000.0;
        constexpr float DEFAULT_F = 48000.0f;

        // seconds -> samples at a given rate
        inline int secondsToSamples(double seconds, double sampleRate) {
            return static_cast<int>(std::ceil(seconds * sampleRate));
        }

        // samples -> seconds
        inline double samplesToSeconds(int64_t samples, double sampleRate) {
            return static_cast<double>(samples) / sampleRate;
        }

        // For debug logging - show kHz
        inline int toKHz(double sampleRate) {
            return static_cast<int>(sampleRate / 1000.0);
        }
    }

    /** LOOP TRACK SETTINGS */
    namespace LoopTrack {
        // Minimum loop length to avoid degenerate loops (from LoopTrack.cpp)
        constexpr int MIN_LOOP_SAMPLES = 256;

        // Maximum length multiplier (from LoopTrack.h)
        constexpr float MAX_MULTIPLIER = 64.0f;
        constexpr float MIN_MULTIPLIER = 1.0f / 64.0f;

        // Default multiplier
        constexpr float DEFAULT_MULTIPLIER = 1.0f;

        // Get max loop samples at given sample rate
        inline int getMaxLoopSamples(double sampleRate) {
            return SampleRate::secondsToSamples(MAX_LOOP_LENGTH_SECONDS, sampleRate);
        }

        // Clamp multiplier to valid range
        inline float clampMultiplier(float mult) {
            if (mult < MIN_MULTIPLIER) return MIN_MULTIPLIER;
            if (mult > MAX_MULTIPLIER) return MAX_MULTIPLIER;
            return mult;
        }
    }

    /** VOLUME SETTINGS */
    namespace Volume {
        constexpr float MIN_DB = -60.0f;
        constexpr float MAX_DB = 6.0f;
        constexpr float DEFAULT_DB = -6.0f;      // 0.5 linear gain
        constexpr float DEFAULT_GAIN = 1.0f;      // Full volume from LoopTrack.h

        // Convert dB to linear gain
        inline float dbToGain(float db) {
            return juce::Decibels::decibelsToGain(db, MIN_DB);
        }

        // Convert linear gain to dB
        inline float gainToDb(float gain) {
            return juce::Decibels::gainToDecibels(gain, MIN_DB);
        }

        // Get default gain as linear value
        inline float getDefaultGain() {
            return dbToGain(DEFAULT_DB);
        }
    }

    /** PROGRESSIVE REPLACE SETTINGS */
    namespace ProgressiveReplace {
        // How many blocks worth of samples to copy per callback
        // (from LoopTrack.cpp: blockSize * 16)
        constexpr int BLOCK_MULTIPLIER = 16;
    }

    /** RETROSPECTIVE BUFFER SETTINGS */
    namespace RetroBuffer {
        constexpr int MAX_SECONDS = 300;              // 5 minutes (from PluginProcessor.cpp)

        inline int getSizeSamples(double sampleRate) {
            return SampleRate::secondsToSamples(MAX_SECONDS, sampleRate);
        }
    }

    /** BPM/TEMPO SETTINGS */
    namespace Tempo {
        constexpr float MIN_BPM = 40.0f;
        constexpr float MAX_BPM = 200.0f;
        constexpr float DEFAULT_BPM = 120.0f;

        // Beats per bar for loop alignment
        constexpr int BEATS_PER_BAR = 4;

        // Calculate samples per beat
        inline int getSamplesPerBeat(float bpm, double sampleRate) {
            if (bpm <= 0.0f) return 0;
            double secondsPerBeat = 60.0 / static_cast<double>(bpm);
            return static_cast<int>(std::round(secondsPerBeat * sampleRate));
        }

        // Calculate samples per bar
        inline int getSamplesPerBar(float bpm, double sampleRate) {
            return getSamplesPerBeat(bpm, sampleRate) * BEATS_PER_BAR;
        }

        // Clamp BPM to valid range
        inline float clampBpm(float bpm) {
            if (bpm < MIN_BPM) return MIN_BPM;
            if (bpm > MAX_BPM) return MAX_BPM;
            return bpm;
        }
    }

    /** PARAMETER NAMES (for APVTS) */
    namespace ParamNames {
        inline juce::String volume(int trackIndex) {
            return "Track" + juce::String(trackIndex + 1) + "_Volume";
        }
        inline juce::String recPlay(int trackIndex) {
            return "Track" + juce::String(trackIndex + 1) + "Record";
        }
        inline juce::String mute(int trackIndex) {
            return "Track" + juce::String(trackIndex + 1) + "_Mute";
        }
        inline juce::String stop(int trackIndex) {
            return "Track" + juce::String(trackIndex + 1) + "_Stop";
        }
        inline juce::String solo(int trackIndex) {
            return "Track" + juce::String(trackIndex + 1) + "_Solo";
        }
        inline juce::String afterLoop(int trackIndex) {
            return "Track" + juce::String(trackIndex + 1) + "_AfterLoop";
        }
        inline juce::String clear(int trackIndex) {
            return "Track" + juce::String(trackIndex + 1) + "_Clear";
        }
        inline juce::String undo(int trackIndex) {
            return "Track" + juce::String(trackIndex + 1) + "_Undo";
        }
        inline juce::String multiply(int trackIndex) {
            return "Track" + juce::String(trackIndex + 1) + "_Multiply";
        }
        inline juce::String divide(int trackIndex) {
            return "Track" + juce::String(trackIndex + 1) + "_Divide";
        }
        inline juce::String outputSelect(int trackIndex) {
            return "Track" + juce::String(trackIndex + 1) + "_OutputSelect";
        }
        inline juce::String fxReplace(int trackIndex) {
            return "Track" + juce::String(trackIndex + 1) + "_FxReplace";
        }

        constexpr const char* BOUNCE_BACK = "BounceBack";
        constexpr const char* RESET_ALL = "ResetAll";
        constexpr const char* MIDI_SYNC_CHANNEL = "MidiSyncChannel";
    }

    /** OUTPUT BUS CONFIGURATION */
    namespace OutputBus {
        constexpr int MAIN_MONITOR = 0;
        constexpr int MAX_BUSES = 7;                    // 0 + 6 additional outputs
        const juce::StringArray NAMES = {
                "Monitor 1/2", "Output 3/4", "Output 5/6", "Output 7/8",
                "Output 9/10", "Output 11/12", "Output 13/14"
        };
    }

    /** FX RETURN INPUT BUS CONFIGURATION */
    namespace FxReturnBus {
        constexpr int FIRST_BUS = 1;                    // Bus 0 is main input
        constexpr int MAX_BUSES = 6;                    // Buses 1-6
    }
}

// Helper macro for debug logging that's actually used in your code
#ifndef LOG
#define LOG(msg) DebugLogger::getInstance().log(msg)
#endif

#ifndef LOG_VALUE
#define LOG_VALUE(name, value) LOG(name + ": " + juce::String(value))
#endif

#ifndef LOG_TRACK
#define LOG_TRACK(track, action, msg) LOG("Track " + juce::String(track) + " " + action + " " + msg)
#endif

#ifndef LOG_SEP
#define LOG_SEP(msg) LOG("=== " + juce::String(msg) + " ===")
#endif