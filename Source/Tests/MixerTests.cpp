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
    for (int i = 0; i < Config::NUM_TRACKS; ++i)
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

static void setTrackMuteSolo(juce::AudioProcessorValueTreeState& apvts, int track, bool mute, bool solo)
{
    const auto prefix = "Track" + juce::String(track + 1) + "_";

    if (auto* muteParam = apvts.getParameter(prefix + "Mute"))
        muteParam->setValueNotifyingHost(mute ? 1.0f : 0.0f);

    if (auto* soloParam = apvts.getParameter(prefix + "Solo"))
        soloParam->setValueNotifyingHost(solo ? 1.0f : 0.0f);
}

class CapturingMixerListener final : public MixerEngine::Listener
{
public:
    void mixerTrackUiStateChanged(const MixerTrackUiState& state) override
    {
        states.push_back(state);
    }

    std::vector<MixerTrackUiState> states;
};

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
            trackStorage.reserve(Config::NUM_TRACKS);
            std::vector<juce::AudioBuffer<float>*> inputs;
            inputs.reserve(Config::NUM_TRACKS);

            for (int i = 0; i < Config::NUM_TRACKS; ++i)
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
            trackStorage.reserve(Config::NUM_TRACKS);
            std::vector<juce::AudioBuffer<float>*> inputs;
            inputs.reserve(Config::NUM_TRACKS);

            for (int i = 0; i < Config::NUM_TRACKS; ++i)
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

            for (int i = 0; i < Config::NUM_TRACKS; ++i)
                setTrackParams(apvts, i, i == 0 ? 1.0f : 0.0f, -1.0f);

            juce::AudioBuffer<float> longTrack(2, 16);
            for (int ch = 0; ch < longTrack.getNumChannels(); ++ch)
            {
                auto* data = longTrack.getWritePointer(ch);
                for (int s = 0; s < longTrack.getNumSamples(); ++s)
                    data[s] = static_cast<float>(s) * 0.1f;
            }

            std::vector<juce::AudioBuffer<float>*> inputs;
            inputs.reserve(Config::NUM_TRACKS);
            inputs.push_back(&longTrack);
            for (int i = 1; i < Config::NUM_TRACKS; ++i)
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
            trackStorage.reserve(Config::NUM_TRACKS);
            std::vector<juce::AudioBuffer<float>*> inputs;
            inputs.reserve(Config::NUM_TRACKS);

            for (int i = 0; i < Config::NUM_TRACKS; ++i)
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

            for (int i = 0; i < Config::NUM_TRACKS; ++i)
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

class MixerTask32Tests : public juce::UnitTest
{
public:
    MixerTask32Tests() : juce::UnitTest("MixerTask32Tests") {}

    void runTest() override
    {
        beginTest("mute applies normally when no track is soloed");
        {
            DummyProcessor proc;
            juce::AudioProcessorValueTreeState apvts(proc, nullptr, "PARAMS", createMockLayout());

            MixerEngine mixer;
            mixer.attachParameters(apvts);  
            mixer.prepare(48000.0, 32);

            setTrackParams(apvts, 0, 1.0f, 0.0f);    

            setTrackParams(apvts, 1, 1.0f, 0.0f);
            setTrackMuteSolo(apvts, 0, true, false); 
            setTrackMuteSolo(apvts, 1, false, false);

            juce::AudioBuffer<float> t0(2, 32), t1(2, 32);
            fillBuffer(t0, 1.0f);
            fillBuffer(t1, 1.0f); 
            std::vector<juce::AudioBuffer<float>*> inputs { &t0, &t1 };

            juce::AudioBuffer<float> out(2, 32);
            out.clear();

            mixer.process(inputs, out);

            expect(!mixer.getIsAnyTrackSoloed(), " No solo buttons are active, so global solo state should be false .");
            expect(!mixer.isTrackAudible(0), "Muted track should be inaudible when no solo is active.");
            expect(mixer.isTrackAudible(1), "Unmuted track should be audible when no solo is active.");

            expectWithinAbsoluteError(out.getSample(0, 0), 0.25f, 0.02f, "Only one unmuted track should contribute");
            expectWithinAbsoluteError(out.getSample(1, 0), 0.25f, 0.02f, "only one unmuted track should contribute.");

        }

        beginTest("solo state overrides mute hierarchy");
        {
            DummyProcessor proc;
            juce::AudioProcessorValueTreeState apvts(proc, nullptr, "PARAMS", createMockLayout());

            MixerEngine mixer;
            mixer.attachParameters(apvts);
            mixer.prepare(48000.0, 32);

            setTrackParams(apvts, 0, 1.0f, 0.0f);
            setTrackParams(apvts, 1, 1.0f, 0.0f);  

            juce::AudioBuffer<float> t0(2, 32), t1(2, 32);
            fillBuffer(t0, 1.0f);
            fillBuffer(t1, 1.0f);
            std::vector<juce::AudioBuffer<float>*> inputs { &t0, &t1 };

            // Track 1 is soloed but also muted --  with any solo active, solo track still contributes
            setTrackMuteSolo(apvts, 0, true, true);
            setTrackMuteSolo(apvts, 1, false, false);  
 
            juce::AudioBuffer<float> out(2, 32);
            out.clear();
            mixer.process(inputs, out);

            expect(mixer.getIsAnyTrackSoloed(), " Solo listener should raise global solo state. ");
            expect(mixer.isTrackAudible(0), "Soloed track should be audible even if its mute flag is also set.");
            expect(!mixer.isTrackAudible(1), "Non-soloed track should be inaudible while any solo is active.");

            expectWithinAbsoluteError(out.getSample(0, 0), 0.25f, 0.02f, "soloed track should pass while non-solo track is ignored");
            expectWithinAbsoluteError(out.getSample(1, 0), 0.25f, 0.02f, "Soloed track should pass while non-solo track is ignored");
        }
    }
};

static MixerTask32Tests mixerTask32Tests;

class MixerTask47Tests : public juce::UnitTest
{
public:
    MixerTask47Tests() : juce::UnitTest("MixerTask47Tests") {}

    void runTest() override
    {
        beginTest("mixer publishes track UI audibility state");
        {
            DummyProcessor proc;
            juce::AudioProcessorValueTreeState apvts(proc, nullptr, "PARAMS", createMockLayout());

            MixerEngine mixer;
            mixer.attachParameters(apvts);

            CapturingMixerListener listener;
            mixer.addListener(&listener);

            setTrackMuteSolo(apvts, 0, true, false);
            expect(!mixer.getTrackUiState(0).audible,
                   "Muted track should not be effectively audible when no solo is active.");

            listener.states.clear();
            setTrackMuteSolo(apvts, 1, false, true);

            expect(listener.states.size() >= 2,
                   "Solo changes should publish mixer UI state.");
            expect(listener.states[1].trackIndex == 1 && listener.states[1].audible,
                   "Solo changes should publish the soloed track as audible.");
            expect(listener.states[0].trackIndex == 0 && !listener.states[0].audible,
                   "Solo changes should publish other tracks as not audible.");

            mixer.removeListener(&listener);
        }
    }
};

static MixerTask47Tests mixerTask47Tests;

class MixerTask34Tests : public juce::UnitTest
{
public:
    MixerTask34Tests() : juce::UnitTest("MixerTask34Tests") {}

    void runTest() override
    {
        beginTest("simd mixing matches expected summed output");
        {
            DummyProcessor proc;
            juce::AudioProcessorValueTreeState apvts(proc, nullptr, "PARAMS", createMockLayout());

            MixerEngine mixer;
            mixer.attachParameters(apvts);
            mixer.prepare(48000.0, 16);

            std::vector<juce::AudioBuffer<float>> trackStorage;
            std::vector<juce::AudioBuffer<float>*> inputs;
            trackStorage.reserve(Config::NUM_TRACKS);
            inputs.reserve(Config::NUM_TRACKS);

            for (int i = 0; i < Config::NUM_TRACKS; ++i)
            {
                setTrackParams(apvts, i, 1.0f, 0.0f);
                trackStorage.emplace_back(2, 16);
                inputs.push_back(&trackStorage.back());
            }

            for (int track = 0; track < Config::NUM_TRACKS; ++track)
            {
                for (int ch = 0; ch < 2; ++ch)
                {
                    auto* data = trackStorage[static_cast<size_t>(track)].getWritePointer(ch);
                    for (int s = 0; s < 16; ++s)
                        data[s] = static_cast<float>((track + 1) * (s + 1)) * 0.01f;
                }
            }

            juce::AudioBuffer<float> output(2, 16);
            output.clear();
            mixer.process(inputs, output);

            for (int s = 0; s < 16; ++s)
            {
                float sum = 0.0f;
                for (int track = 0; track < Config::NUM_TRACKS; ++track)
                    sum += trackStorage[static_cast<size_t>(track)].getSample(0, s);

                const float expected = juce::jlimit(-1.0f, 1.0f, sum * MixerEngine::kDefaultHeadroomScale);
                expectWithinAbsoluteError(output.getSample(0, s), expected, 0.0001f);
                expectWithinAbsoluteError(output.getSample(1, s), expected, 0.0001f);
            }
        }

        beginTest("simd gain ramp moves level across a block");
        {
            DummyProcessor proc;
            juce::AudioProcessorValueTreeState apvts(proc, nullptr, "PARAMS", createMockLayout());

            MixerEngine mixer;
            mixer.attachParameters(apvts);
            mixer.prepare(48000.0, 64);

            juce::AudioBuffer<float> track0(2, 64);
            fillBuffer(track0, 1.0f);

            std::vector<juce::AudioBuffer<float>*> inputs;
            inputs.push_back(&track0);

            // let smoother settle near zero
            setTrackParams(apvts, 0, 0.0f, 0.0f);
            juce::AudioBuffer<float> output(2, 64);
            for (int i = 0; i < 32; ++i)
            {
                output.clear();
                mixer.process(inputs, output);
            }

            //  trigger an upward ramp and verify block starts lower than it ends.
            setTrackParams(apvts, 0, 1.0f, 0.0f);
            output.clear();
            mixer.process(inputs, output);

            const float first = output.getSample(0, 0);
            const float middle = output.getSample(0, 32);
            const float last = output.getSample(0, 63);

            expect(middle > first, "expected ramped gain to increase through the block.");
            expect(last > middle, "Expected end of block to be louder than middle..");
            expect(last <= 0.26f, " Headroom should keep single-track output around 0.25 max");
        }

        beginTest("active loop volume changes avoid zipper jumps");
        {
            constexpr int blockSize = 64;

            DummyProcessor proc;
            juce::AudioProcessorValueTreeState apvts(proc, nullptr, "PARAMS", createMockLayout());

            MixerEngine mixer;
            mixer.attachParameters(apvts);
            mixer.prepare(48000.0, blockSize);

            std::atomic<std::int64_t> loopPosition { 0 };
            mixer.setGlobalSampleCounter(&loopPosition);

            juce::AudioBuffer<float> activeLoop(2, 128);
            fillBuffer(activeLoop, 1.0f);

            std::vector<juce::AudioBuffer<float>*> inputs { &activeLoop };
            juce::AudioBuffer<float> output(2, blockSize);

            setTrackParams(apvts, 0, 0.0f, 0.0f);
            for (int i = 0; i < 8; ++i)
            {
                output.clear();
                mixer.process(inputs, output);
                loopPosition.fetch_add(blockSize);
            }

            bool allSamplesFinite = true;
            bool hasPreviousSample = false;
            float previousSample = 0.0f;
            float biggestJump = 0.0f;

            auto renderVolume = [&](float targetVolume)
            {
                setTrackParams(apvts, 0, targetVolume, 0.0f);
                output.clear();
                mixer.process(inputs, output);
                loopPosition.fetch_add(blockSize);

                for (int sample = 0; sample < blockSize; ++sample)
                {
                    const float value = output.getSample(0, sample);
                    allSamplesFinite = allSamplesFinite && std::isfinite(value);

                    if (hasPreviousSample)
                        biggestJump = juce::jmax(biggestJump, std::abs(value - previousSample));

                    previousSample = value;
                    hasPreviousSample = true;
                }
            };

            renderVolume(1.0f);
            expect(output.getSample(0, blockSize - 1) < 0.12f,
                   "First ramp block should not jump straight to full volume.");

            renderVolume(0.0f);
            renderVolume(1.0f);
            renderVolume(0.0f);
            renderVolume(1.0f);

            expect(allSamplesFinite, "Rapid volume changes should not create NaN or inf samples.");
            expect(biggestJump < 0.02f, "Volume smoothing should avoid zipper jumps between samples.");
        }

        beginTest("active loop panner moves signal left and right");
        {
            constexpr int blockSize = 64;

            DummyProcessor proc;
            juce::AudioProcessorValueTreeState apvts(proc, nullptr, "PARAMS", createMockLayout());

            MixerEngine mixer;
            mixer.attachParameters(apvts);
            mixer.prepare(48000.0, blockSize);

            juce::AudioBuffer<float> activeLoop(2, 128);
            fillBuffer(activeLoop, 1.0f);

            std::vector<juce::AudioBuffer<float>*> inputs { &activeLoop };
            juce::AudioBuffer<float> output(2, blockSize);

            auto renderPan = [&](float pan)
            {
                setTrackParams(apvts, 0, 1.0f, pan);

                for (int i = 0; i < 16; ++i)
                {
                    output.clear();
                    mixer.process(inputs, output);
                }

                return std::pair<float, float>(
                    output.getMagnitude(0, 0, blockSize),
                    output.getMagnitude(1, 0, blockSize));
            };

            const auto centerPan = renderPan(0.0f);
            const auto leftPan = renderPan(-1.0f);
            const auto rightPan = renderPan(1.0f);

            expect(leftPan.first > leftPan.second + 0.05f,
                   "Left pan should make the left channel louder.");
            expectWithinAbsoluteError(centerPan.first, centerPan.second, 0.0001f,
                                      "Center pan should keep both channels balanced.");
            expect(rightPan.second > rightPan.first + 0.05f,
                   "Right pan should make the right channel louder.");
        }
    }
};

static MixerTask34Tests mixerTask34Tests;

class MixerMeteringTests : public juce::UnitTest
{
public:
    MixerMeteringTests() : juce::UnitTest("MixerMeteringTests") {}

    void runTest() override
    {
        beginTest("track peak level updates from processed audio");
        {
            DummyProcessor proc;
            juce::AudioProcessorValueTreeState apvts(proc, nullptr, "PARAMS", createMockLayout());

            MixerEngine mixer;
            mixer.attachParameters(apvts);
            mixer.prepare(48000.0, 64);

            juce::AudioBuffer<float> track(2, 64);
            fillBuffer(track, 0.5f);

            std::vector<juce::AudioBuffer<float>*> inputs { &track };
            juce::AudioBuffer<float> output(2, 64);
            output.clear();
            mixer.process(inputs, output);

            expect(mixer.getTrackPeakLevel(0) > 0.0f, "Track meter should show level after audio is processed.");
            expectWithinAbsoluteError(mixer.getTrackPeakLevel(1), 0.0f, 0.0001f,
                                      "Track without input should stay at zero.");
        }

        beginTest("muted track reports zero level");
        {
            DummyProcessor proc;
            juce::AudioProcessorValueTreeState apvts(proc, nullptr, "PARAMS", createMockLayout());

            MixerEngine mixer;
            mixer.attachParameters(apvts);
            mixer.prepare(48000.0, 64);

            setTrackMuteSolo(apvts, 0, true, false);

            juce::AudioBuffer<float> track(2, 64);
            fillBuffer(track, 0.5f);

            std::vector<juce::AudioBuffer<float>*> inputs { &track };
            juce::AudioBuffer<float> output(2, 64);
            output.clear();
            mixer.process(inputs, output);

            expectWithinAbsoluteError(mixer.getTrackPeakLevel(0), 0.0f, 0.0001f,
                                      "Muted track meter should read zero.");
        }

        beginTest("invalid track meter index returns zero");
        {
            MixerEngine mixer;
            expectWithinAbsoluteError(mixer.getTrackPeakLevel(Config::NUM_TRACKS), 0.0f, 0.0001f,
                                      "Invalid track meter index should be safe.");
        }
    }
};

static MixerMeteringTests mixerMeteringTests;

class MasterLimiterStressTests : public juce::UnitTest
{
public:
    MasterLimiterStressTests() : juce::UnitTest("MasterLimiterStressTests") {}

    void runTest() override
    {
        beginTest("master limiter stays stable with four loud tracks");
        {
            DummyProcessor proc;
            juce::AudioProcessorValueTreeState apvts(proc, nullptr, "PARAMS", createMockLayout());

            MixerEngine mixer;
            mixer.attachParameters(apvts);
            mixer.prepare(48000.0, 128);

            std::vector<juce::AudioBuffer<float>> tracks;
            std::vector<juce::AudioBuffer<float>*> inputs;
            tracks.reserve(Config::NUM_TRACKS);
            inputs.reserve(Config::NUM_TRACKS);

            for (int i = 0; i < Config::NUM_TRACKS; ++i)
            {
                setTrackParams(apvts, i, 1.0f, 0.0f);
                tracks.emplace_back(2, 128);

                for (int ch = 0; ch < 2; ++ch)
                {
                    auto* data = tracks.back().getWritePointer(ch);
                    for (int s = 0; s < 128; ++s)
                        data[s] = (s % 2 == 0) ? 8.0f : -8.0f;
                }

                inputs.push_back(&tracks.back());
            }

            juce::AudioBuffer<float> output(2, 128);
            output.clear();
            mixer.process(inputs, output);

            bool allSamplesFinite = true;
            float maxValue = -1000.0f;
            float minValue = 1000.0f;

            for (int ch = 0; ch < output.getNumChannels(); ++ch)
            {
                for (int s = 0; s < output.getNumSamples(); ++s)
                {
                    const float sample = output.getSample(ch, s);
                    allSamplesFinite = allSamplesFinite && std::isfinite(sample);
                    maxValue = juce::jmax(maxValue, sample);
                    minValue = juce::jmin(minValue, sample);
                }
            }

            expect(allSamplesFinite, "Limiter output should not be NaN or inf.");
            expect(maxValue <= 1.0001f, "Limiter should not go above +1.0.");
            expect(minValue >= -1.0001f, "Limiter should not go below -1.0.");
            expect(maxValue >= 0.999f, "Hot positive samples should clip at the ceiling.");
            expect(minValue <= -0.999f, "Hot negative samples should clip at the floor.");
        }

        beginTest("master limiter clips immediately");
        {
            DummyProcessor proc;
            juce::AudioProcessorValueTreeState apvts(proc, nullptr, "PARAMS", createMockLayout());

            MixerEngine mixer;
            mixer.attachParameters(apvts);
            mixer.prepare(48000.0, 16);

            std::vector<juce::AudioBuffer<float>> tracks;
            std::vector<juce::AudioBuffer<float>*> inputs;
            tracks.reserve(Config::NUM_TRACKS);
            inputs.reserve(Config::NUM_TRACKS);

            for (int i = 0; i < Config::NUM_TRACKS; ++i)
            {
                setTrackParams(apvts, i, 1.0f, 0.0f);
                tracks.emplace_back(2, 16);
                tracks.back().clear();
                tracks.back().setSample(0, 0, 8.0f);
                tracks.back().setSample(1, 0, 8.0f);
                inputs.push_back(&tracks.back());
            }

            juce::AudioBuffer<float> output(2, 16);
            output.clear();
            mixer.process(inputs, output);

            expectWithinAbsoluteError(output.getSample(0, 0), 1.0f, 0.0001f,
                                      "First loud sample should clip right away.");
            expectWithinAbsoluteError(output.getSample(1, 0), 1.0f, 0.0001f,
                                      "Right channel should also clip right away.");
            expectWithinAbsoluteError(output.getSample(0, 1), 0.0f, 0.0001f,
                                      "Limiter should not delay the clipped sample.");
        }

        // ------------------------------------------------------------------ //
        // Stress test 1: sustained heavy mixing over 500 consecutive blocks.  //
        // Verifies that repeated float arithmetic does not accumulate errors   //
        // (NaN, inf, or ceiling breach) when all four tracks run loud.        //
        // ------------------------------------------------------------------ //
        beginTest("sustained 500 blocks of heavy mixing: no NaN, inf, or ceiling breach");
        {
            DummyProcessor proc;
            juce::AudioProcessorValueTreeState apvts(proc, nullptr, "PARAMS", createMockLayout());

            MixerEngine mixer;
            mixer.attachParameters(apvts);
            mixer.prepare(48000.0, 256);

            std::vector<juce::AudioBuffer<float>> tracks;
            std::vector<juce::AudioBuffer<float>*> inputs;
            tracks.reserve(Config::NUM_TRACKS);
            inputs.reserve(Config::NUM_TRACKS);

            for (int i = 0; i < Config::NUM_TRACKS; ++i)
            {
                setTrackParams(apvts, i, 1.0f, 0.0f);
                tracks.emplace_back(2, 256);
                fillBuffer(tracks.back(), 5.0f);  // loud, legitimate hot signal
                inputs.push_back(&tracks.back());
            }

            juce::AudioBuffer<float> output(2, 256);
            bool allFinite = true;
            float maxAbsValue = 0.0f;

            for (int block = 0; block < 500; ++block)
            {
                output.clear();
                mixer.process(inputs, output);

                for (int ch = 0; ch < output.getNumChannels(); ++ch)
                {
                    for (int s = 0; s < output.getNumSamples(); ++s)
                    {
                        const float sample = output.getSample(ch, s);
                        if (!std::isfinite(sample))
                            allFinite = false;
                        maxAbsValue = juce::jmax(maxAbsValue, std::abs(sample));
                    }
                }
            }

            expect(allFinite,
                   "No NaN or inf should appear across 500 consecutive blocks of heavy mixing.");
            expect(maxAbsValue <= 1.0001f,
                   "Limiter ceiling must hold for all 500 blocks of sustained heavy mixing.");
        }

        // ------------------------------------------------------------------ //
        // Stress test 2: alternating-polarity signal across all four tracks.  //
        // Each track plays a full-scale square wave; adjacent tracks use       //
        // opposite polarity so partial cancellation tests the ceiling in both  //
        // directions.  Runs 500 blocks to confirm there is no drift.          //
        // ------------------------------------------------------------------ //
        beginTest("alternating-polarity heavy mix stays within ceiling over 500 blocks");
        {
            DummyProcessor proc;
            juce::AudioProcessorValueTreeState apvts(proc, nullptr, "PARAMS", createMockLayout());

            MixerEngine mixer;
            mixer.attachParameters(apvts);
            mixer.prepare(48000.0, 128);

            std::vector<juce::AudioBuffer<float>> tracks;
            std::vector<juce::AudioBuffer<float>*> inputs;
            tracks.reserve(Config::NUM_TRACKS);
            inputs.reserve(Config::NUM_TRACKS);

            for (int i = 0; i < Config::NUM_TRACKS; ++i)
            {
                setTrackParams(apvts, i, 1.0f, 0.0f);
                tracks.emplace_back(2, 128);

                // Odd-indexed tracks are positive; even-indexed are negative.
                const float polarity = (i % 2 == 0) ? 6.0f : -6.0f;
                fillBuffer(tracks.back(), polarity);
                inputs.push_back(&tracks.back());
            }

            juce::AudioBuffer<float> output(2, 128);
            bool ceilingHeld = true;
            bool allFinite   = true;

            for (int block = 0; block < 500; ++block)
            {
                output.clear();
                mixer.process(inputs, output);

                for (int ch = 0; ch < output.getNumChannels(); ++ch)
                {
                    for (int s = 0; s < output.getNumSamples(); ++s)
                    {
                        const float sample = output.getSample(ch, s);
                        if (!std::isfinite(sample))
                            allFinite = false;
                        if (sample > 1.0001f || sample < -1.0001f)
                            ceilingHeld = false;
                    }
                }
            }

            expect(allFinite,
                   "Alternating-polarity mix must not produce NaN or inf.");
            expect(ceilingHeld,
                   "Ceiling must be respected even with alternating-polarity loud input.");
        }
    }
};

static MasterLimiterStressTests masterLimiterStressTests;
