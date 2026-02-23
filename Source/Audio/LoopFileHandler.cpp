//
// Created by Vincewa Tran on 2/18/26.
//

#include "LoopFileHandler.h"

LoopFileHandler::LoopFileHandler() {
    formatManager.registerBasicFormats(); // WAV, AIFF, FLAC, OGG
}

bool LoopFileHandler::loadAudioFile(const juce::File &file, LoopTrack& targetTrack) {
    if (!isSupportedAudioFile(file)) {
        DBG("Unsupported file type: "+file.getFileName());
        return false;
    }

    // Create reader for the file
    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));

    if (reader == nullptr) {
        DBG("Failed to create reader for: " + file.getFileName());
        return false;
    }

    // Get file info
    int numChannels = static_cast<int>(reader->numChannels);
    int numSamples = static_cast<int>(reader->lengthInSamples);
    double fileSampleRate = reader->sampleRate;

    DBG("Loading file: " + file.getFileName());
    DBG(" Channels: " + juce::String(numChannels));
    DBG(" Samples: " + juce::String(numSamples));
    DBG(" Sample Rate: " + juce::String(fileSampleRate));

    // Crate buffer and read audio
    juce::AudioSampleBuffer buffer(numChannels,numSamples);

    bool readSuccess = reader->read(&buffer, 0, numSamples, 0, true, true);

    if (!readSuccess) {
        DBG("Failed to read audio data");
        return false;
    }

    // Give buffer to track
    targetTrack.setAudioBuffer(buffer, fileSampleRate);

    DBG("Successfully loaded into Track " + juce::String(targetTrack.getTrackId()));

    return true;
}

bool LoopFileHandler::isSupportedAudioFile(const juce::File &file) {
    if (!file.exists()) {
        DBG("File does not exist: " + file.getFullPathName());
        return false;
    }

    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));

    if (reader == nullptr) {
        DBG("No reader available for: " + file.getFileName());
        return false;
    }

    return true;
}

bool LoopFileHandler::writeTrackToStream(juce::OutputStream &stream, const LoopTrack &track) {
    // Write track settings
    stream.writeFloat(track.getCurrentVolumeDb());
    stream.writeFloat(track.getCurrentPan());
    stream.writeBool(track.isMuted());
    stream.writeBool(track.isSoloed());
    // TODO: write reversed state
    // TODO: write slip offset amount to track

    // Write audio data
    const auto& buffer = track.getAudioBuffer();
    int numSamples = buffer.getNumSamples();
    int numChannels = buffer.getNumChannels();

    stream.writeInt(numSamples);
    stream.writeInt(numChannels);

    for (int ch = 0; ch < numChannels; ++ch) {
        stream.write(buffer.getReadPointer(ch),
                     static_cast<size_t>(numSamples) * sizeof(float));
    }
    return true;
}

bool LoopFileHandler::readTrackFromStream(juce::InputStream &stream, LoopTrack &track) {
    // Read track settings
    float volume = stream.readFloat();
    float pan = stream.readFloat();
    bool muted = stream.readBool();
    bool soloed = stream.readBool();
    // TODO: read reverse state
    // TODO: read slip amount (int)

    // Apply settings
    track.setVolumeDb(volume);
    track.setPan(pan);
    track.setSolo(soloed);
    track.setMute(muted);
    // TODO: Apply reverse state
    // TODO: Apply slip amount (int)

    // Read audio data
    int numSamples = stream.readInt();
    int numChannels = stream.readInt();

    juce::AudioSampleBuffer buffer(numChannels, numSamples);

    for (int ch = 0; ch <numChannels; ++ch) {
        stream.read(buffer.getWritePointer(ch),
                    static_cast<size_t>(numSamples) * sizeof(float));
    }

    track.setAudioBuffer(buffer, 48000.0);

    return true;
}

bool LoopFileHandler::saveProject(const juce::File &destination, const LoopManager &loopManager,
                                  const SyncEngine &syncEngine) {
    // TODO: Save project state
}

bool LoopFileHandler::loadProject(const juce::File &source, LoopManager &loopManager, SyncEngine &syncEngine) {
    // TODO: Handle loading full projects
}

juce::StringArray LoopFileHandler::getSupportedExtensions() {
    return {"wav", "aiff", "aif", "flac", "ogg" };
}

juce::File LoopFileHandler::getDefaultAudioFolder() {
    return juce::File::getSpecialLocation(juce::File::userMusicDirectory);
}

juce::File LoopFileHandler::getDefaultProjectFolder() {
    return juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
    .getChildFile("AudioLoopStation").getChildFile("Projects");
}