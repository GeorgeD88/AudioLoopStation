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
    // TODO: read reverse state
    // TODO: read slip amount (int)

    // Apply settings
    track.setVolumeDb(volume);
    track.setPan(pan);
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

    auto sourceSr = static_cast<double>(TrackConfig::DEFAULT_SAMPLE_RATE);
    track.setAudioBuffer(buffer, sourceSr);

    return true;
}

bool LoopFileHandler::saveProject(const juce::File &destination, const LoopManager &loopManager,
                                  const SyncEngine &syncEngine) {
    juce::FileOutputStream stream(destination);
    if (!stream.openedOk()) {
        DBG("Save Project: Failed to open file for writing");
        return false;
    }

    AlsFormat::Header header;
    AlsFormat::initHeader(header);

    double projectSampleRate = syncEngine.getSampleRate();
    if (projectSampleRate <= 0) projectSampleRate = static_cast<double>(TrackConfig::DEFAULT_SAMPLE_RATE);
    header.sampleRate = static_cast<uint32_t>(projectSampleRate);
    header.numChannels = TrackConfig::STEREO_MODE ? 2 : 1;
    header.numTracks = static_cast<uint16_t>(TrackConfig::MAX_TRACKS);

    // Build JSON metadata
    juce::DynamicObject::Ptr root = new juce::DynamicObject();
    root->setProperty("version", static_cast<int>(AlsFormat::VERSION));
    root->setProperty("bpm", syncEngine.getTempo());
    root->setProperty("sampleRate", static_cast<int>(header.sampleRate));
    root->setProperty("numTracks", static_cast<int>(header.numTracks));

    juce::Array<juce::var> tracksArray;
    for (size_t i = 0; i < TrackConfig::MAX_TRACKS; ++i) {
        const LoopTrack* track = loopManager.getTrack(i);
        if (!track) continue;

        juce::DynamicObject::Ptr t = new juce::DynamicObject();
        t->setProperty("index", static_cast<int>(i));
        t->setProperty("volumeDb", track->getCurrentVolumeDb());
        t->setProperty("pan", track->getCurrentPan());
        t->setProperty("reverse", track->isReversed());
        t->setProperty("slipOffset", track->getSlipOffset());
        t->setProperty("loopLengthSamples", track->getLoopLengthSamples());
        t->setProperty("hasAudio", track->hasAudio());
        double sr = track->getSourceSampleRate();
        t->setProperty("sourceSampleRate", sr > 0 ? sr : projectSampleRate);
        tracksArray.add(juce::var(t.get()));
    }
    root->setProperty("tracks", tracksArray);

    juce::var jsonVar(root.get());
    juce::String jsonStr = juce::JSON::toString(jsonVar);
    size_t jsonBytes = jsonStr.getNumBytesAsUTF8();
    juce::MemoryBlock jsonBlock(jsonStr.toRawUTF8(), jsonBytes);
    header.jsonLength = jsonBlock.getSize();

    // Write binary audio to memory first so we know the length
    juce::MemoryBlock audioBlock;
    juce::MemoryOutputStream audioStream(audioBlock, true);

    int tracksWithAudio = 0;
    for (size_t i = 0; i < TrackConfig::MAX_TRACKS; ++i) {
        const LoopTrack* track = loopManager.getTrack(i);
        if (!track || !track->hasAudio()) continue;

        const auto& buf = track->getAudioBuffer();
        int numSamples = buf.getNumSamples();
        int numChannels = buf.getNumChannels();

        audioStream.writeInt(numSamples);
        audioStream.writeInt(numChannels);
        for (int ch = 0; ch < numChannels; ++ch) {
            audioStream.write(buf.getReadPointer(ch),
                              static_cast<size_t>(numSamples) * sizeof(float));
        }
        ++tracksWithAudio;
    }
    header.audioLength = audioBlock.getSize();

    // Write header
    if (!stream.write(static_cast<const void*>(&header), AlsFormat::HEADER_SIZE)) {
        DBG("Save Project: Failed to write header");
        return false;
    }
    // Write JSON
    if (!stream.write(jsonBlock.getData(), static_cast<size_t>(header.jsonLength))) {
        DBG("Save Project: Failed to write JSON");
        return false;
    }
    // Write binary audio
    if (header.audioLength > 0 && !stream.write(audioBlock.getData(), static_cast<size_t>(header.audioLength))) {
        DBG("Save Project: Failed to write audio data");
        return false;
    }

    DBG("Save Project: Saved " + juce::String(tracksWithAudio) + " tracks to " + destination.getFileName());
    return true;
}

bool LoopFileHandler::loadProject(const juce::File &source, LoopManager &loopManager, SyncEngine &syncEngine) {
    if (!source.existsAsFile()) {
        DBG("Load Project: File does not exist");
        return false;
    }

    juce::FileInputStream stream(source);
    if (!stream.openedOk()) {
        DBG("Load Project: Failed to open file");
        return false;
    }

    AlsFormat::Header header;
    if (stream.read(&header, AlsFormat::HEADER_SIZE) != static_cast<int64_t>(AlsFormat::HEADER_SIZE)) {
        DBG("Load Project: Failed to read header");
        return false;
    }

    if (header.magic != AlsFormat::MAGIC) {
        DBG("Load Project: Invalid magic (not an .als file)");
        return false;
    }
    if (header.version > AlsFormat::VERSION) {
        DBG("Load Project: Unsupported format version " + juce::String(header.version));
        return false;
    }

    // Read JSON
    juce::MemoryBlock jsonBlock(header.jsonLength);
    if (stream.read(jsonBlock.getData(), static_cast<size_t>(header.jsonLength)) != static_cast<int64_t>(header.jsonLength)) {
        DBG("Load Project: Failed to read JSON");
        return false;
    }

    juce::var jsonVar = juce::JSON::parse(juce::String::fromUTF8(
        static_cast<const char*>(jsonBlock.getData()),
        static_cast<size_t>(header.jsonLength)));
    if (!jsonVar.isObject()) {
        DBG("Load Project: Invalid JSON metadata");
        return false;
    }

    // Apply global settings
    juce::var bpmVar = jsonVar.getProperty("bpm", TrackConfig::DEFAULT_BPM);
    if (bpmVar.isDouble() || bpmVar.isInt())
        syncEngine.setTempo(static_cast<float>(static_cast<double>(bpmVar)));

    // Stop and clear all tracks before loading
    loopManager.stopAllPlayback();
    loopManager.clearAllTracks();

    double projectSampleRate = header.sampleRate > 0
        ? static_cast<double>(header.sampleRate)
        : static_cast<double>(TrackConfig::DEFAULT_SAMPLE_RATE);

    // Apply per-track metadata and load audio
    juce::var tracksVar = jsonVar.getProperty("tracks", juce::var());
    if (!tracksVar.isArray()) {
        DBG("Load Project: No tracks array in JSON");
        return true; // Empty project is valid
    }

    juce::Array<juce::var>* tracksArray = tracksVar.getArray();
    int numTracksWithAudio = 0;
    for (const auto& tVar : *tracksArray) {
        if (!tVar.isObject()) continue;

        int index = static_cast<int>(tVar.getProperty("index", 0));
        if (index < 0 || index >= TrackConfig::MAX_TRACKS) continue;

        LoopTrack* track = loopManager.getTrack(static_cast<size_t>(index));
        if (!track) continue;

        // Apply DSP parameters
        track->setVolumeDb(static_cast<float>(static_cast<double>(tVar.getProperty("volumeDb", TrackConfig::DEFAULT_VOLUME_DB))));
        track->setPan(static_cast<float>(static_cast<double>(tVar.getProperty("pan", TrackConfig::DEFAULT_PAN))));
        track->setReverse(static_cast<bool>(tVar.getProperty("reverse", false)));
        track->setSlip(static_cast<int>(tVar.getProperty("slipOffset", 0)));

        bool hasAudio = static_cast<bool>(tVar.getProperty("hasAudio", false));
        if (!hasAudio) continue;

        // Read next audio block from binary section
        int numSamples = stream.readInt();
        int numChannels = stream.readInt();
        if (numSamples <= 0 || numChannels <= 0 || numChannels > 2) {
            DBG("Load Project: Invalid audio block for track " + juce::String(index));
            continue;
        }

        juce::AudioBuffer<float> buffer(numChannels, numSamples);
        for (int ch = 0; ch < numChannels; ++ch) {
            if (stream.read(buffer.getWritePointer(ch), static_cast<size_t>(numSamples) * sizeof(float))
                != static_cast<int64_t>(numSamples) * static_cast<int64_t>(sizeof(float))) {
                DBG("Load Project: Failed to read audio for track " + juce::String(index));
                break;
            }
        }

        double trackSr = projectSampleRate;
        if (tVar.hasProperty("sourceSampleRate")) {
            trackSr = static_cast<double>(tVar.getProperty("sourceSampleRate", projectSampleRate));
        }
        track->setAudioBuffer(buffer, trackSr);
        ++numTracksWithAudio;
    }

    DBG("Load Project: Loaded " + juce::String(numTracksWithAudio) + " tracks from " + source.getFileName());
    return true;
}

juce::StringArray LoopFileHandler::getSupportedExtensions() {
    return {"wav", "aiff", "aif", "flac", "ogg" };
}

juce::String LoopFileHandler::getSupportedExtString() {
    auto extensions = getSupportedExtensions();
    juce::String wildcard;

    for (auto& ext : extensions)
    {
        if (wildcard.isNotEmpty())
            wildcard += ";";
        wildcard += "*." + ext;
    }
    return wildcard;
}

juce::File LoopFileHandler::getDefaultAudioFolder() {
    return juce::File::getSpecialLocation(juce::File::userMusicDirectory);
}

juce::File LoopFileHandler::getDefaultProjectFolder() {
    return juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
    .getChildFile("AudioLoopStation").getChildFile("Projects");
}
