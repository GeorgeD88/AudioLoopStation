//
// Created by Vincewa Tran on 2/18/26.
//

#pragma once
#include "LoopTrack.h"
#include "AlsFormat.h"
#include "juce_audio_formats/juce_audio_formats.h"
#include "juce_audio_basics/juce_audio_basics.h"
#include "juce_data_structures/juce_data_structures.h"

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
    static juce::String getSupportedExtString();

    // === Saving and loading projects ===
    bool saveProject(const juce::File& destination,
                     const std::vector<std::unique_ptr<LoopTrack>>& tracks,
                     double sampleRate,
                     float bpm);

    bool loadProject(const juce::File& source,
                     std::vector<std::unique_ptr<LoopTrack>>& tracks,
                     double sampleRate,
                     float bpm);

    static juce::File getDefaultAudioFolder();
    static juce::File getDefaultProjectFolder();

private:
    juce::AudioFormatManager formatManager;     // Save only SamplePlayer handles load and play


    static bool writeTrackToStream(juce::OutputStream& stream, const LoopTrack& track);
    static bool readTrackFromStream(juce::InputStream& stream, LoopTrack& track);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LoopFileHandler)
};


