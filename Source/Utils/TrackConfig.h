//
// Created by Vincewa Tran on 11/7/25.
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

namespace TrackConfig {

    // Sync Engine
    constexpr float DEFAULT_BPM = 120.0f;
    constexpr float BPM_GLOBAL_MIN = 40.0f;
    constexpr float BPM_GLOBAL_MAX = 200.0f;
    constexpr float DEFAULT_BPM_INCR = 0.5f;

    // Track Configuration
    constexpr int INVALID_TRACK_ID = -1;
    constexpr int FIRST_TRACK_ID = 0;
    constexpr int MAX_TRACKS = 4;                      // MVP: 4 mono/2 stereo
    constexpr bool STEREO_MODE = true;                 // set to false for 4 mono tracks, true for two stereo tracks

    // Audio Engine
    constexpr int DEFAULT_SAMPLE_RATE = 48000;          // compile-time default
    constexpr int INIT_BUFFER_SIZE_SAMPLES = 1024;
    constexpr int DEFAULT_BUFFER_SIZE = 256;            // for ~5.3ms latency at 48k
    constexpr double MAX_LOOP_LENGTH_SECONDS = 600;     // 10 minutes

    // DSP Parameters
    constexpr float MIN_VOLUME_DB = -60.0f;
    constexpr float MAX_VOLUME_DB = 6.0f;
    constexpr float DEFAULT_VOLUME_DB = 0.8f;           // -6dB default
    constexpr double_t VOLUME_FADE_SECONDS = 0.05f;

    constexpr float MIN_PAN = -1.0f;                    // Full left
    constexpr float MAX_PAN = 1.0f;                     // Full right
    constexpr float DEFAULT_PAN = 0.0f;                 // Center
    constexpr double_t PAN_FADE_SECONDS = 0.03f;

    constexpr float MIN_PITCH_SEMITONES = -12.0f;
    constexpr float MAX_PITCH_SEMITONES = 12.0f;
    constexpr float DEFAULT_PITCH = 0.0f;               // No shift

    constexpr float MIN_STRETCH_RATIO = 0.5f;           // 50% slower
    constexpr float MAX_STRETCH_RATIO = 2.0f;           // 200% faster
    constexpr float DEFAULT_STRETCH = 1.0f;             // Normal speed

    // Performance Targets
    constexpr double MAX_LATENCY_MS = 10.0;             // <10ms target
    constexpr double UI_REFRESH_MS = 16.0;              // ~60 FPS

    // UI Constraints

    // File handler
    static constexpr int PROJECT_VERSION = 1;
    static constexpr const char* PROJECT_FILE_EXT = "als";
}
