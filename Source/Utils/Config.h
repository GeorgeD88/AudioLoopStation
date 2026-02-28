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

# pragma once
#include "juce_audio_basics/juce_audio_basics.h"
#include "gin_dsp/gin_dsp.h"
#include "cstdint"

namespace Config {

    /** CORE PROGRAM VALUES
     * Used in:
     * - LoopManager: Creates array of tracks
     * - MixerEngine: Processes this many tracks
     * - TrackControlPanel: Creates UI strips
     * - APVTS: Creates parameters for each track
     */
    constexpr int MAX_TRACKS = 4;                       // MVP: 4 mono/2 stereo
    constexpr double MAX_LOOP_LEN_SEC = 600.0;          // 10 minutes, int and f vers
    constexpr float MAX_LOOP_LEN_SEC_F =
            static_cast<float>(MAX_LOOP_LEN_SEC);
    constexpr int DEFAULT_BUFFER_SIZE = 256;            // for ~5.3ms latency at 48k
    constexpr int INVALID_TRACK_ID = -1;

    /** SAMPLE RATE HANDLING
     * Default sample rate (only used as fallback)
     * @note Always use actual device sample rate from prepareToPlay()!
     * This is just for initial calculations before audio starts.
     */
    namespace SampleRate {
        constexpr double DEFAULT = 48000.0;
        constexpr float DEFAULT_F = 48000.0f;

        // seconds -> samples at a given rate
        inline int secondsToSamples(double seconds, double sampleRate) {
            return static_cast<int>(std::ceil(seconds * sampleRate));
        }

        // samples -> seconds
        /** Convert samples to seconds */
        inline double samplesToSeconds(int64_t samples, double sampleRate) {
            return static_cast<double>(samples) / sampleRate;
        }

        // Convert samples to seconds (float version)
        inline float samplesToSecondsFloat(int64_t samples, double sampleRate) {
            return static_cast<float>(static_cast<double>(samples) / sampleRate);
        }

        // For debug logging - show kHz
        inline int toKHz(double sampleRate) {
            return static_cast<int>(sampleRate / 1000.0);
        }
    }

    /** SMOOTHING SETTINGS
     * Time constraints to set for parameter changes throughout ALS
     * - Key uses in audio files
     */
    namespace Smoothing {
        /** Volume fade time
         * longer for smooth gain changes
         * 50ms is
         * - short enough to feel responsive
         * - long enough to prevent zipper noise/clicks
         * - standard in professional audio
         */
        constexpr double VOL_SEC = 0.05;
        constexpr float  VOL_SEC_F = static_cast<float>(VOL_SEC);

        /** Pan fade time
         * shorter to preserve stereo imaging
         * 30ms
         * - allows pan changes to feel immediate
         * - still prevents clicks
         * - shorter than volume because pan is less critical
         */
        constexpr double PAN_SEC = 0.03;
        constexpr float  PAN_SEC_F = static_cast<float>(PAN_SEC);

        /** Mixer parameter smoothing (volume only)
         *
         * Used in:
         * - MixerEngine::volumeSmoothers[i].setTime()
         *
         * Why 10ms?
         * - Mixer handles summed signals
         * - Faster response needed for live mixing
         */
        constexpr double MIXER_SEC = 0.01;
        constexpr float  MIXER_SEC_F = static_cast<float>(MIXER_SEC);

        // Tolerance for considering values equal
        constexpr float EQUALITY_TOLERANCE = 0.0001f;

        // Helper to calculate samples for given time
        inline int getSamplesForTime(double seconds, double sampleRate) {
            return static_cast<int>(std::ceil(seconds * sampleRate));
        }

        // Helper to calculate samples for given time (float seconds)
        inline int getSamplesForTime(float seconds, double sampleRate) {
            return static_cast<int>(std::ceil(static_cast<double>(seconds) * sampleRate));
        }

        // Check if smoothing is needed between current and target values
        inline bool needsSmoothing(float current, float target) {
            return std::abs(current - target) > EQUALITY_TOLERANCE;
        }

        // Check if smoothing is needed (double version)
        inline bool needsSmoothing(double current, double target) {
            return std::abs(current - target) > static_cast<double>(EQUALITY_TOLERANCE);
        }
    }

    /** TEMPO/BPM SETTINGS
     * Universal values for time verification and beat alignment
     */
    namespace Tempo {
        constexpr float DEFAULT = 120.0f;
        constexpr float MIN = 40.0f;
        constexpr float MAX = 200.0f;

        // UI adjustment step
        constexpr float STEP = 0.5f;

        // Beats per bar (loop alignment)
        constexpr int BEATS_PER_BAR = 4;

        // Calculate samples per beat at a given sample rate
        inline int getSamplesPerBeat(float bpm, double sampleRate) {
            if (bpm <= 0.0f) return 0;
            double secondsPerBeat = 60.0 / static_cast<double>(bpm);
            return static_cast<int>(std::round(secondsPerBeat * sampleRate));
        }

        // Calculate samples per bar
        inline int getSamplesPerBar(float bpm, double sampleRate) {
            return getSamplesPerBeat(bpm, sampleRate) * BEATS_PER_BAR;
        }

        // Format BPM for display
        inline juce::String toString(float bpm) {
            return juce::String(bpm, 1) + " BPM";
        }
    }

    /** VOLUME SETTINGS
     *  Set volume default, ranges, and other calculated values
     *  for DSP, Mixer, and UI.
     */
    namespace Volume {
        constexpr float MIN_DB = -60.0f;
        constexpr float MAX_DB = 6.0f;

        /** Default volume (-6dB = 0.5 linear)
         * -6dB Provides headroom for summing multiple tracks
         * 4 tracks at -6dB each sum to 0dB (full scale)
         */
        constexpr float DEFAULT_DB = -6.0f;

        // Fade time in seconds (from Smoothing namespace)
        constexpr double FADE_SEC = Smoothing::VOL_SEC;
        constexpr float  FADE_SEC_F = Smoothing::VOL_SEC_F;

        // Monitor gain when armed but not recording (50% volume)
        constexpr float MONITOR_GAIN = 0.5f;

        // Convert dB to linear gain
        inline float dbToGain(float db) {
            return juce::Decibels::decibelsToGain(db, MIN_DB);
        }

        // Convert dB to linear gain (double version)
        inline double dbToGain(double db) {
            return juce::Decibels::decibelsToGain(static_cast<float>(db), MIN_DB);
        }

        // Convert linear gain to dB
        inline float gainToDb(float gain) {
            return juce::Decibels::gainToDecibels(gain, MIN_DB);
        }

        // Get fade samples at given sample rate
        inline int getFadeSamples(double sampleRate) {
            return Smoothing::getSamplesForTime(FADE_SEC, sampleRate);
        }

        // Get default gain as linear value
        inline float getDefaultGain() {
            return dbToGain(DEFAULT_DB);
        }

        // Format volume for display (as percentage)
        inline juce::String toString(float gain) {
            return juce::String(gain * 100.0f, 1) + "%";
        }

        // Format volume with dB
        inline juce::String toDbString(float db) {
            return juce::String(db, 1) + " dB";
        }
    }

    /** PAN SETTINGS
     *
     */
    namespace Pan {
        // Full left
        constexpr float MIN = -1.0f;

        // Full right
        constexpr float MAX = 1.0f;

        // Center
        constexpr float DEFAULT = 0.0f;

        // Fade time in seconds (from Smoothing namespace)
        constexpr double FADE_SEC = Smoothing::PAN_SEC;
        constexpr float  FADE_SEC_F = Smoothing::PAN_SEC_F;

        // Law for equal-power panning
        constexpr auto PANNER_RULE = juce::dsp::PannerRule::squareRoot3dB;

        // Get fade samples at given sample rate
        inline int getFadeSamples(double sampleRate) {
            return Smoothing::getSamplesForTime(FADE_SEC, sampleRate);
        }

        // Format pan for display
        inline juce::String toString(float pan) {
            if (pan < -0.01f) return juce::String(pan * 100.0f, 1) + "% L";
            if (pan > 0.01f) return juce::String(pan * 100.0f, 1) + "% R";
            return "Center";
        }
    }

    /** REVERSE PLAYBACK
     * - Required feature
     */
    namespace Reverse {
        // Whether reverse is enabled by default
        constexpr bool DEFAULT = false;
    }

    /** SLIP SETTINGS
     * - Required feature
     */
    namespace Slip {
        // Default slip offset (samples)
        constexpr int DEFAULT_OFFSET = 0;

        /** Maximum slip offset as percentage of loop length
         *  100% = full loop length
         */
        constexpr double MAX_OFFSET_PERCENT = 1.0;
        constexpr float  MAX_OFFSET_PERCENT_F = static_cast<float>(MAX_OFFSET_PERCENT);

        // UI step size for slip control (as percentage)
        constexpr double UI_STEP_PERCENT = 0.01;  // 1% steps
        constexpr float  UI_STEP_PERCENT_F = static_cast<float>(UI_STEP_PERCENT);

        // Get max offset samples for a given loop length
        inline int getMaxOffsetSamples(int loopLengthSamples) {
            return static_cast<int>(loopLengthSamples * MAX_OFFSET_PERCENT);
        }

        // Convert percentage to samples
        inline int percentToSamples(float percent, int loopLengthSamples) {
            return static_cast<int>(static_cast<float>(loopLengthSamples) * percent);
        }

        // Format slip for display
        inline juce::String toString(float percent) {
            return juce::String(percent * 100.0f, 1) + "%";
        }
    }


    /** RECORDING SETTINGS
     *
     */
    namespace Recording {
        // Whether to auto-start playback after recording
        constexpr bool AUTO_PLAY_AFTER_RECORD = true;

        // Whether to quantize loop length to beats
        constexpr bool QUANTIZE_TO_BEATS = true;

        // Minimum loop length in samples (safety)
        constexpr int MIN_LOOP_SAMPLES = 1024;

        // Maximum loop length in samples (capped by MAX_LOOP_LENGTH_SEC)
        inline int getMaxLoopSamples(double sampleRate) {
            return SampleRate::secondsToSamples(Config::MAX_LOOP_LEN_SEC, sampleRate);
        }
    }

    /** MIXER SETTINGS
     *
     */
    namespace Mixer {
         // For 4 tracks at full scale: 4 * 0.25 = 1.0 (0dB)
        constexpr float HEADROOM_SCALE = 0.25f;

        // Stereo channels constant
        constexpr int STEREO_CHANNELS = 2;

        // Smoothing time for parameter changes (from Smoothing namespace)
        constexpr double SMOOTHING_SEC = Smoothing::MIXER_SEC;
        constexpr float  SMOOTHING_SEC_F = Smoothing::MIXER_SEC_F;
    }

/**
    // =========================================================================
    // OPTIONAL FEATURE VALUES
    // =========================================================================
    namespace Stretch {
        // Minimum stretch ratio (50% speed)
        constexpr float MIN_RATIO = 0.5f;

        // Maximum stretch ratio (200% speed)
        constexpr float MAX_RATIO = 2.0f;

        // Default (no stretch)
        constexpr float DEFAULT = 1.0f;

        // UI step size
        constexpr float STEP = 0.01f;

        // Optional TODO: implement time stretching

    }

    namespace Pitch {
        // Minimum pitch shift (-1 octave)
        constexpr float MIN_SEMITONES = -12.0f;

        // Maximum pitch shift (+1 octave)
        constexpr float MAX_SEMITONES = 12.0f;

        // Default (no shift)
        constexpr float DEFAULT = 0.0f;

        // UI step size (1 semitone)
        constexpr float STEP = 1.0f;

        // Convert semitones to playback ratio
        inline float semitonesToRatio(float semitones) {
            return std::pow(2.0f, semitones / 12.0f);
        }

        // Format semitones for display
        inline juce::String toString(float semitones) {
            if (semitones > 0) return "+" + juce::String(semitones, 1) + " st";
            if (semitones < 0) return juce::String(semitones, 1) + " st";
            return "0 st";
        }
    }

    namespace Trim {
        // Minimum loop length in beats (for auto-trim)
        constexpr int MIN_BEATS = 1;

        // Maximum loop length in beats
        constexpr int MAX_BEATS = 64;

        // Default to trim to nearest bar
        constexpr int DEFAULT_BEATS = 4;

        // Tolerance for zero-crossing detection (samples)
        constexpr int ZERO_CROSSING_TOLERANCE = 4;
    }

*/

}