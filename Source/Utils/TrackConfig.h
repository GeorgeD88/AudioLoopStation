//
// Created by Vincewa Tran on 11/7/25.
//

# pragma once

namespace TrackConfig {

    // Sync Engine
    constexpr float DEFAULT_BPM = 120.0f;

    // Track Configuration
    constexpr int INVALID_TRACK_ID = -1;
    constexpr int FIRST_TRACK_ID = 0;
    constexpr int MAX_TRACKS = 4;                      // MVP: 4 mono/2 stereo
    constexpr bool STEREO_MODE = true;                 // set to false for 4 mono tracks, true for two stereo tracks

    // Audio Engine
    constexpr int DEFAULT_SAMPLE_RATE = 48000;
    constexpr int DEFAULT_BUFFER_SIZE = 256;            // for ~5.3ms latency at 48k
    constexpr double MAX_LOOP_LENGTH_SECONDS = 600;     // 10 minutes

    // DSP Parameters
    constexpr float MIN_VOLUME_DB = -60.0f;
    constexpr float MAX_VOLUME_DB = 6.0f;
    constexpr float DEFAULT_VOLUME_DB = 0.8f;              // -6dB default
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

}
