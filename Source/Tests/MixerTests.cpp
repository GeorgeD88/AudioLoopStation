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

static juce::AudioProcessorValueTreeState::ParameterLayout createMockLayoutMissingPan()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "Track1_Volume", "Track1_Volume",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.8f));
    layout.add(std::make_unique<juce::AudioParameterBool>(
        "Track1_Mute", "Track1_Mute", false));
    layout.add(std::make_unique<juce::AudioParameterBool>(
        "Track1_Solo", "Track1_Solo", false));
    return layout;
}

static juce::AudioProcessorValueTreeState::ParameterLayout createMockLayoutTrack2()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "Track1_Volume", "Track1_Volume",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.8f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "Track1_Pan", "Track1_Pan",
        juce::NormalisableRange<float>(-1.0f, 1.0f, 0.01f), 0.0f));
    layout.add(std::make_unique<juce::AudioParameterBool>(
        "Track1_Mute", "Track1_Mute", false));
    layout.add(std::make_unique<juce::AudioParameterBool>(
        "Track1_Solo", "Track1_Solo", false));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "Track2_Volume", "Track2_Volume",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.8f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "Track2_Pan", "Track2_Pan",
        juce::NormalisableRange<float>(-1.0f, 1.0f, 0.01f), 0.0f));
    layout.add(std::make_unique<juce::AudioParameterBool>(
        "Track2_Mute", "Track2_Mute", false));
    layout.add(std::make_unique<juce::AudioParameterBool>(
        "Track2_Solo", "Track2_Solo", false));

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
                setTrackParams(apvts, i, 1.0f, 0.0f);
                trackStorage.emplace_back(2, 64);
                fillBuffer(trackStorage.back(), 1.0f);
                inputs.push_back(&trackStorage.back());
            }

            juce::AudioBuffer<float> output(2, 64);
            for (int i = 0; i < 8; ++i)
            {
                output.clear();
                mixer.process(inputs, output);
            }

            const float leftSample = output.getSample(0, 0);
            const float rightSample = output.getSample(1, 0);

            expectWithinAbsoluteError(leftSample, 1.0f, 0.02f, "Expected 4x track sum scaled by 0.25 headroom.");
            expectWithinAbsoluteError(rightSample, 1.0f, 0.02f, "Expected center-panned stereo sum on right channel.");
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
                    data[s] = static_cast<float>(s) * 0.1f;
            }

            std::vector<juce::AudioBuffer<float>*> inputs;
            inputs.reserve(TrackConfig::MAX_TRACKS);
            inputs.push_back(&longTrack);
            for (int i = 1; i < TrackConfig::MAX_TRACKS; ++i)
                inputs.push_back(nullptr);

            juce::AudioBuffer<float> output(2, 4);
            for (int i = 0; i < 12; ++i)
            {
                output.clear();
                mixer.process(inputs, output);
            }

            expectWithinAbsoluteError(output.getSample(0, 0), 0.15f, 0.02f, "Expected sample 6 scaled by headroom.");
            expectWithinAbsoluteError(output.getSample(0, 1), 0.175f, 0.02f, "Expected sample 7 scaled by headroom.");
            expectWithinAbsoluteError(output.getSample(0, 2), 0.2f, 0.02f, "Expected sample 8 scaled by headroom.");
            expectWithinAbsoluteError(output.getSample(0, 3), 0.225f, 0.02f, "Expected sample 9 scaled by headroom.");
        }

        beginTest("hard clips master output to [-1, 1]");
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
                setTrackParams(apvts, i, 1.0f, 0.0f);
                trackStorage.emplace_back(2, 64);
                fillBuffer(trackStorage.back(), 10.0f); // intentionally hot
                inputs.push_back(&trackStorage.back());
            }

            juce::AudioBuffer<float> output(2, 64);
            output.clear();
            mixer.process(inputs, output);

            float maxValue = -1000.0f;
            float minValue = 1000.0f;
            for (int channel = 0; channel < output.getNumChannels(); ++channel)
            {
                for (int sample = 0; sample < output.getNumSamples(); ++sample)
                {
                    const float value = output.getSample(channel, sample);
                    maxValue = juce::jmax(maxValue, value);
                    minValue = juce::jmin(minValue, value);
                }
            }
            expect(maxValue <= 1.0001f, "Positive clipping should cap at +1.0");
            expect(minValue >= -1.0001f, "Positive clipping should stay above -1.0");

            for (int i = 0; i < TrackConfig::MAX_TRACKS; ++i)
                fillBuffer(trackStorage[i], -10.0f);

            output.clear();
            mixer.process(inputs, output);

            maxValue = -1000.0f;
            minValue = 1000.0f;
            for (int channel = 0; channel < output.getNumChannels(); ++channel)
            {
                for (int sample = 0; sample < output.getNumSamples(); ++sample)
                {
                    const float value = output.getSample(channel, sample);
                    maxValue = juce::jmax(maxValue, value);
                    minValue = juce::jmin(minValue, value);
                }
            }
            expect(minValue >= -1.0001f, "Negative clipping should cap at -1.0");
            expect(maxValue <= 1.0001f, "Negative clipping should stay below +1.0");
        }
    }
};

static MixerTask31Tests mixerTask31Tests;

class MixerTestsExtra : public juce::UnitTest
{
public:
    MixerTestsExtra() : juce::UnitTest("MixerTestsExtra") {}

    void runTest() override
    {
        beginTest("track2 ids update");
        DummyProcessor proc;
        juce::AudioProcessorValueTreeState apvts(proc, nullptr, "PARAMS", createMockLayoutTrack2());

        MixerEngine mixer;
        mixer.attachParameters(apvts);
        mixer.prepare(48000.0, 32);

        apvts.getRawParameterValue("Track2_Volume")->store(0.2f);
        apvts.getRawParameterValue("Track2_Pan")->store(-0.5f);

        juce::AudioBuffer<float> out(2, 32);
        juce::AudioBuffer<float> track0(2, 32);
        fillBuffer(track0, 1.0f);
        std::vector<juce::AudioBuffer<float>*> inputs;
        inputs.push_back(&track0);

        mixer.process(inputs, out);

        expect(mixer.getLastVolDb(1) == 0.2f);
        expect(mixer.getLastPan(1) == -0.5f);

        beginTest("missing pan param does not crash");
        DummyProcessor proc2;
        juce::AudioProcessorValueTreeState apvtsMissing(proc2, nullptr, "PARAMS", createMockLayoutMissingPan());
        MixerEngine mixerMissing;
        mixerMissing.attachParameters(apvtsMissing);
        mixerMissing.prepare(48000.0, 32);

        apvtsMissing.getRawParameterValue("Track1_Volume")->store(0.7f);
        juce::AudioBuffer<float> out2(2, 32);
        juce::AudioBuffer<float> t2(2, 32);
        fillBuffer(t2, 1.0f);
        std::vector<juce::AudioBuffer<float>*> inputs2;
        inputs2.push_back(&t2);
        mixerMissing.process(inputs2, out2);

        expect(mixerMissing.getLastVolDb(0) == 0.7f);
        expect(mixerMissing.getLastPan(0) == 0.0f);

        beginTest("inputs smaller than tracks");
        apvts.getRawParameterValue("Track1_Volume")->store(0.3f);
        apvts.getRawParameterValue("Track1_Pan")->store(0.1f);
        apvts.getRawParameterValue("Track2_Volume")->store(0.9f);
        apvts.getRawParameterValue("Track2_Pan")->store(0.0f);

        std::vector<juce::AudioBuffer<float>*> inputsOnly1;
        inputsOnly1.push_back(&track0);
        mixer.process(inputsOnly1, out);

        expect(mixer.getLastVolDb(0) == 0.3f);
        expect(mixer.getLastVolDb(1) == 0.9f);
    }
};

static MixerTestsExtra mixerTestsExtra;
