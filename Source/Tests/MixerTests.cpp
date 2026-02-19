#include <atomic>
#include <cmath>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>

#include "../Audio/MixerEngine.h"

class DummyProcessor : public juce::AudioProcessor
{
public:
    DummyProcessor() = default;
    ~DummyProcessor() override = default;

    const juce::String getName() const override { return "dummy"; }
    void prepareToPlay(double, int) override {}
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout&) const override { return true; }
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}
    using juce::AudioProcessor::processBlock;
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override {}
    void setStateInformation(const void*, int) override {}
};

static juce::AudioProcessorValueTreeState::ParameterLayout createMockLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (int i = 0; i < TrackConfig::MAX_TRACKS; ++i)
    {
        const auto prefix = "Track" + juce::String(i + 1) + "_";
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            prefix + "Volume", prefix + "Volume",
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 1.0f));
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            prefix + "Pan", prefix + "Pan",
            juce::NormalisableRange<float>(-1.0f, 1.0f, 0.01f), 0.0f));
        layout.add(std::make_unique<juce::AudioParameterBool>(
            prefix + "Mute", prefix + "Mute", false));
        layout.add(std::make_unique<juce::AudioParameterBool>(
            prefix + "Solo", prefix + "Solo", false));
    }
    return layout;
}

static void fillBuffer(juce::AudioBuffer<float>& buffer, float value)
{
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        auto* data = buffer.getWritePointer(ch);
        for (int s = 0; s < buffer.getNumSamples(); ++s)
            data[s] = value;
    }
}

static void setTrackParams(juce::AudioProcessorValueTreeState& apvts, int track, float volume, float pan)
{
    const auto prefix = "Track" + juce::String(track + 1) + "_";
    apvts.getRawParameterValue(prefix + "Volume")->store(volume);
    apvts.getRawParameterValue(prefix + "Pan")->store(pan);
}

class MixerTask31Tests : public juce::UnitTest
{
public:
    MixerTask31Tests() : juce::UnitTest("MixerTask31Tests") {}

    void runTest() override
    {
        beginTest("sums four tracks into stereo master with headroom");
        {
            DummyProcessor proc;
            juce::AudioProcessorValueTreeState apvts(proc, nullptr, "PARAMS", createMockLayout());

            MixerEngine mixer;
            mixer.attachParameters(apvts);
            mixer.prepare(48000.0, 64);

            std::vector<juce::AudioBuffer<float>> trackStorage;
            trackStorage.reserve(TrackConfig::MAX_TRACKS);
            std::vector<juce::AudioBuffer<float>*> inputs;
            inputs.reserve(TrackConfig::MAX_TRACKS);

            for (int i = 0; i < TrackConfig::MAX_TRACKS; ++i)
            {
                setTrackParams(apvts, i, 1.0f, -1.0f);
                trackStorage.emplace_back(2, 64);
                fillBuffer(trackStorage.back(), 1.0f);
                inputs.push_back(&trackStorage.back());
            }

            juce::AudioBuffer<float> output(2, 64);
            output.clear();
            mixer.process(inputs, output);

            const float leftSample = output.getSample(0, 0);
            const float rightSample = output.getSample(1, 0);

            expectWithinAbsoluteError(leftSample, 1.0f, 0.02f, "Expected 4x track sum scaled by 0.25 headroom.");
            expect(std::abs(rightSample) < 0.02f,
                   "Hard-left panned inputs should be nearly silent on right channel. Right sample="
                   + juce::String(rightSample));
        }

        beginTest("applies track gain and pan before summing");
        {
            DummyProcessor proc;
            juce::AudioProcessorValueTreeState apvts(proc, nullptr, "PARAMS", createMockLayout());

            MixerEngine mixer;
            mixer.attachParameters(apvts);
            mixer.prepare(48000.0, 64);

            std::vector<juce::AudioBuffer<float>> trackStorage;
            trackStorage.reserve(TrackConfig::MAX_TRACKS);
            std::vector<juce::AudioBuffer<float>*> inputs;
            inputs.reserve(TrackConfig::MAX_TRACKS);

            for (int i = 0; i < TrackConfig::MAX_TRACKS; ++i)
            {
                trackStorage.emplace_back(2, 64);
                fillBuffer(trackStorage.back(), 1.0f);
                inputs.push_back(&trackStorage.back());
            }

            // One track left, three tracks right. Right should dominate.
            setTrackParams(apvts, 0, 1.0f, -1.0f);
            setTrackParams(apvts, 1, 1.0f, 1.0f);
            setTrackParams(apvts, 2, 1.0f, 1.0f);
            setTrackParams(apvts, 3, 1.0f, 1.0f);

            juce::AudioBuffer<float> output(2, 64);
            for (int i = 0; i < 16; ++i)
            {
                output.clear();
                mixer.process(inputs, output);
            }

            const float leftBefore = output.getSample(0, 0);
            const float rightBefore = output.getSample(1, 0);
            expect(rightBefore > leftBefore, "Panning should route more energy to the right channel.");

            // Fade one of the right tracks to zero; right channel should reduce.
            setTrackParams(apvts, 3, 0.0f, 1.0f);
            for (int i = 0; i < 16; ++i)
            {
                output.clear();
                mixer.process(inputs, output);
            }

            const float rightAfter = output.getSample(1, 0);
            expect(rightAfter < rightBefore * 0.85f, "Track gain should reduce that track's contribution to master output.");
        }

        beginTest("uses global sample counter for aligned track reads");
        {
            DummyProcessor proc;
            juce::AudioProcessorValueTreeState apvts(proc, nullptr, "PARAMS", createMockLayout());
            std::atomic<std::int64_t> sampleCounter { 6 };

            MixerEngine mixer;
            mixer.attachParameters(apvts);
            mixer.setGlobalSampleCounter(&sampleCounter);
            mixer.prepare(48000.0, 4);

            for (int i = 0; i < TrackConfig::MAX_TRACKS; ++i)
                setTrackParams(apvts, i, i == 0 ? 1.0f : 0.0f, -1.0f);

            juce::AudioBuffer<float> longTrack(2, 16);
            for (int ch = 0; ch < longTrack.getNumChannels(); ++ch)
            {
                auto* data = longTrack.getWritePointer(ch);
                for (int s = 0; s < longTrack.getNumSamples(); ++s)
                    data[s] = static_cast<float>(s);
            }

            std::vector<juce::AudioBuffer<float>*> inputs;
            inputs.reserve(TrackConfig::MAX_TRACKS);
            inputs.push_back(&longTrack);
            for (int i = 1; i < TrackConfig::MAX_TRACKS; ++i)
                inputs.push_back(nullptr);

            juce::AudioBuffer<float> output(2, 4);
            output.clear();
            mixer.process(inputs, output);

            expectWithinAbsoluteError(output.getSample(0, 0), 1.5f, 0.02f, "Expected sample 6 scaled by headroom.");
            expectWithinAbsoluteError(output.getSample(0, 1), 1.75f, 0.02f, "Expected sample 7 scaled by headroom.");
            expectWithinAbsoluteError(output.getSample(0, 2), 2.0f, 0.02f, "Expected sample 8 scaled by headroom.");
            expectWithinAbsoluteError(output.getSample(0, 3), 2.25f, 0.02f, "Expected sample 9 scaled by headroom.");
            const float rightSample = output.getSample(1, 0);
            expect(std::abs(rightSample) < 0.02f,
                   "Hard-left panning should keep right channel near zero. Right sample="
                   + juce::String(rightSample));
        }
    }
};

static MixerTask31Tests mixerTask31Tests;
