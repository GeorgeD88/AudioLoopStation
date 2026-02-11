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

// base mock APVTS with Track1 params only
static juce::AudioProcessorValueTreeState::ParameterLayout createMockLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    // TODO: add tracks 1-3 later, this is just to get started
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

    return layout;
}

// missing pan param on purpose (null pointer path)
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

// Track1 + Track2 params so we can test indexing
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

// main mixer tests (basic + rough checks)
class MixerTests : public juce::UnitTest
{
public:
    MixerTests() : juce::UnitTest("MixerTests") {}

    void runTest() override
    {
        // just checks the basic APVTS params exist
        beginTest("attach params (basic test for now)");

        DummyProcessor proc;
        juce::AudioProcessorValueTreeState apvts(proc, nullptr, "PARAMS", createMockLayout());

        MixerEngine mixer;
        mixer.attachParameters(apvts);
        mixer.prepare(48000.0, 64);

        // TODO: make real checks later (gain/pan stuff)
        expect(apvts.getRawParameterValue("Track1_Volume") != nullptr); 
        expect(apvts.getRawParameterValue("Track1_Pan") != nullptr);
        expect(apvts.getRawParameterValue("Track1_Mute") != nullptr);   
        expect(apvts.getRawParameterValue("Track1_Solo") != nullptr); 

        // sets APVTS values, then makes sure process() reads them
        beginTest("read params in process (basic test)");
        apvts.getRawParameterValue("Track1_Volume")->store(0.5f);
        apvts.getRawParameterValue("Track1_Pan")->store(0.25f);

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

        expect(mixer.getLastVolDb(0) == 0.5f);
        expect(mixer.getLastPan(0) == 0.25f);

        // checks that smoothing actually changes the buffer a bit
        beginTest("volume smoothing changes buffer"); 
        auto firstSample = track0.getSample(0, 0);

        auto lastSample = track0.getSample(0, track0.getNumSamples() - 1);
        expect(lastSample < firstSample);

        // basic pan test: pan should change balance (direction doesn't matter here)
        beginTest("panning left/right "); 
        mixer.prepare(48000.0, 64);

        // pan left
        apvts.getRawParameterValue("Track1_Volume")->store(1.0f);
        apvts.getRawParameterValue("Track1_Pan")->store(-1.0f);  
        for (int ch = 0; ch < track0.getNumChannels(); ++ch)
        {
            auto* data = track0.getWritePointer(ch);
            for (int s = 0; s < track0.getNumSamples(); ++s) 
                data[s] = 1.0f;
        }
        mixer.process(inputs, out);
        auto left0 = track0.getSample(0, 0);
        auto right0 = track0.getSample(1, 0);

        float diffLeft = left0 - right0;
        expect(std::abs(diffLeft) > 0.001f);

        // pan right
        apvts.getRawParameterValue("Track1_Pan")->store(1.0f);
        for (int ch = 0; ch < track0.getNumChannels(); ++ch)
        {
            auto* data = track0.getWritePointer(ch);
            for (int s = 0; s < track0.getNumSamples(); ++s)
                data[s] = 1.0f;
        }
        mixer.process(inputs, out);
        left0 = track0.getSample(0, 0);
        right0 = track0.getSample(1, 0);
        float diffRight = left0 - right0;
        expect(std::abs(diffRight) > 0.001f);
        expect(std::abs(diffLeft - diffRight) > 0.001f);
    }
};

static MixerTests mixerTests;

// extra tests for edge cases (track2, missing params, smaller inputs)
class MixerTestsExtra : public juce::UnitTest
{
public:
    MixerTestsExtra() : juce::UnitTest("MixerTestsExtra") {}

    void runTest() override
    {
        beginTest("track2 ids update ");
        DummyProcessor proc;
        juce::AudioProcessorValueTreeState apvts(proc, nullptr, "PARAMS", createMockLayoutTrack2());

        MixerEngine mixer;

        mixer.attachParameters(apvts);
        mixer.prepare(48000.0, 32);

        apvts.getRawParameterValue("Track2_Volume")->store(0.2f);
        apvts.getRawParameterValue("Track2_Pan")->store(-0.5f);

        juce::AudioBuffer<float> out(2, 32);
        juce::AudioBuffer<float> track0(2, 32);
        for (int ch = 0; ch < track0.getNumChannels(); ++ch)
            for (int s = 0; s < track0.getNumSamples(); ++s)
                track0.getWritePointer(ch)[s] = 1.0f;
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
        for (int ch = 0; ch < t2.getNumChannels(); ++ch)
            for (int s = 0; s < t2.getNumSamples(); ++s)
                t2.getWritePointer(ch)[s] = 1.0f;
        
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
