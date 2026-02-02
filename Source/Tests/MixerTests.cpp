#include <juce_audio_processors/juce_audio_processors.h>

#include "../Audio/MixerEngine.h"

class DummyProcessor : public juce::AudioProcessor
{
public:
    DummyProcessor() {}
    ~DummyProcessor() override {}
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
    // TODO: add tracks 1-3 later, this is just to get started
    layout.add(std::make_unique<juce::AudioParameterFloat>(
            "track_0_vol", "track_0_vol",
            juce::NormalisableRange<float>(-60.0f, 6.0f, 0.01f), -6.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
            "track_0_pan", "track_0_pan",
            juce::NormalisableRange<float>(-1.0f, 1.0f, 0.01f), 0.0f));
    layout.add(std::make_unique<juce::AudioParameterBool>(
            "track_0_mute", "track_0_mute", false));
    layout.add(std::make_unique<juce::AudioParameterBool>(
            "track_0_solo", "track_0_solo", false));

    return layout;
}

class MixerTests : public juce::UnitTest
{
public:
    MixerTests() : juce::UnitTest("MixerTests") {}

    void runTest() override
    {
        beginTest("attach params (basic test for now)");

        DummyProcessor proc;
        juce::AudioProcessorValueTreeState apvts(proc, nullptr, "PARAMS", createMockLayout());

        MixerEngine mixer;
        mixer.attachParameters(apvts);
        mixer.prepare(48000.0, 64);

        // TODO: make real checks later (gain/pan stuff)
        expect(apvts.getRawParameterValue("track_0_vol") != nullptr);
        expect(apvts.getRawParameterValue("track_0_pan") != nullptr);
        expect(apvts.getRawParameterValue("track_0_mute") != nullptr);
        expect(apvts.getRawParameterValue("track_0_solo") != nullptr);

        beginTest("read params in process (basic test)");
        apvts.getRawParameterValue("track_0_vol")->store(-12.0f);
        apvts.getRawParameterValue("track_0_pan")->store(0.25f);

        juce::AudioBuffer<float> out(2, 64);
        juce::AudioBuffer<float> track0(2, 64);
        for (int ch = 0; ch < track0.getNumChannels(); ++ch)
        {
            auto* data = track0.getWritePointer(ch);
            for (int s = 0; s < track0.getNumSamples(); ++s)
                data[s] = 1.0f;
        }
        std::vector<juce::AudioBuffer<float>*> inputs;
        inputs.push_back(&track0);
        mixer.process(inputs, out);

        expect(mixer.getLastVolDb(0) == -12.0f);
        expect(mixer.getLastPan(0) == 0.25f);

        beginTest("volume smoothing changes buffer ");
        auto firstSample = track0.getSample(0, 0);
        auto lastSample = track0.getSample(0, track0.getNumSamples() - 1);
        expect(lastSample < firstSample);
    }
};

static MixerTests mixerTests;
