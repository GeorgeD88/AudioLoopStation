#pragma once
#include <cstdint>
#include <cstring>
#include "../Utils/TrackConfig.h"

namespace AlsFormat {

// File Structure: [0:512] Fixed Header | [512:N] JSON Metadata | [N:end] Binary Audio
constexpr uint32_t MAGIC = 0x00534C41;  // "ALS\0"
constexpr uint32_t VERSION = static_cast<uint32_t>(TrackConfig::PROJECT_VERSION);
constexpr size_t HEADER_SIZE = 512;
constexpr uint16_t BITS_PER_SAMPLE = 32;

#pragma pack(push, 1)
struct Header {
    uint32_t magic;
    uint32_t version;
    uint64_t jsonLength;       // Metadata length in bytes
    uint64_t audioLength;      // Total binary audio length
    uint32_t sampleRate;
    uint16_t bitsPerSample;
    uint16_t numChannels;      // 1=mono, 2=stereo
    uint16_t numTracks;
    uint16_t reserved1;
    uint8_t  reserved2[476];   // Padding to 512 bytes
};
#pragma pack(pop)

static_assert(sizeof(Header) == HEADER_SIZE, "Header must be exactly 512 bytes");

/**
 * Binary audio layout for active tracks:
 * [uint32_t numSamples][uint32_t numChannels][float data...]
 */

inline void initHeader(Header& h) {
    std::memset(&h, 0, sizeof(h));
    h.magic = MAGIC;
    h.version = VERSION;
    h.bitsPerSample = BITS_PER_SAMPLE;
}

} // namespace AlsFormat