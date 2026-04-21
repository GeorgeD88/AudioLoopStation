# .als File Format Specification

Audio Loop Station project file format.

## Layout

| Section | Offset | Size | Description |
|---------|--------|------|-------------|
| Header | 0 | 512 bytes | Fixed binary header |
| JSON | 512 | variable | UTF-8 JSON metadata |
| Binary audio | 512 + jsonLength | variable | Concatenated per-track audio data |

## Header (512 bytes)

| Offset | Size | Field | Description |
|--------|------|-------|-------------|
| 0 | 4 | magic | 0x00534C41 ("ALS\0") - file identification |
| 4 | 4 | version | Format version (1) |
| 8 | 8 | jsonLength | Bytes of JSON metadata |
| 16 | 8 | audioLength | Bytes of binary audio |
| 24 | 4 | sampleRate | Project sample rate (Hz) |
| 28 | 2 | bitsPerSample | 32 for float |
| 30 | 2 | numChannels | 1=mono, 2=stereo per track |
| 32 | 2 | numTracks | Number of tracks |
| 34 | 476 | reserved | Future use |

## JSON Metadata

```json
{
  "version": 1,
  "bpm": 120,
  "sampleRate": 48000,
  "numTracks": 4,
  "tracks": [
    {
      "index": 0,
      "volumeDb": -6.0,
      "pan": 0,
      "mute": false,
      "solo": false,
      "reverse": false,
      "slipOffset": 0,
      "loopLengthSamples": 96000,
      "hasAudio": true,
      "sourceSampleRate": 48000
    }
  ]
}
```

## Binary Audio

For each track with `hasAudio: true`, in track index order:
- `uint32` numSamples
- `uint32` numChannels
- `float[numSamples * numChannels]` planar (ch0 all samples, ch1 all samples)
