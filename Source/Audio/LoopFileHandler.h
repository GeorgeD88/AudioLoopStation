//
// Created by Vincewa Tran on 2/18/26.
//

#pragma once
#include "LoopTrack.h"
#include "LoopManager.h"
#include "juce_audio_formats/juce_audio_formats.h"
#include "juce_audio_basics/juce_audio_basics.h"
#include "../Utils/TrackConfig.h"

/**
 * Handles playback from file and storage
 */
class LoopFileHandler {
public:
    LoopFileHandler();
    ~LoopFileHandler() = default;

    // === Load audio file from disk ===
    bool loadAudioFile(const juce::File& file, LoopTrack& targetTrack);
    bool isSupportedAudioFile(const juce::File& file);
    static juce::StringArray getSupportedExtensions();

    // === Saving and loading projects ===
    bool saveProject(const juce::File& destination,
                     const LoopManager& loopManager,
                     const SyncEngine& syncEngine);

    bool loadProject(const juce::File& source,
                     LoopManager& loopManager,
                     SyncEngine& syncEngine);

    static juce::File getDefaultAudioFolder();
    static juce::File getDefaultProjectFolder();

private:
    juce::AudioFormatManager formatManager;     // Save only SamplePlayer handles load and play

    bool writeTrackToStream(juce::OutputStream& stream, const LoopTrack& track);
    bool readTrackFromStream(juce::InputStream& stream, LoopTrack& track);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LoopFileHandler)
};


