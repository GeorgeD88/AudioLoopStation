#include <cmath>

#include <juce_audio_processors/juce_audio_processors.h>

#include "../Audio/LoopManager.h"
#include "../Audio/MixerEngine.h"

class MixerBridgeDummyProcessor : public juce::AudioProcessor
{
public:
    MixerBridgeDummyProcessor() = default;
    ~MixerBridgeDummyProcessor() override = default;

    const juce::String getName() const override { return "MixerBridgeDummyProcessor"; }
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

static juce::AudioProcessorValueTreeState::ParameterLayout createMixerLayout()
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

static void setTrackMuteSolo(juce::AudioProcessorValueTreeState& apvts, int track, bool mute, bool solo)
{
    const auto prefix = "Track" + juce::String(track + 1) + "_";

    if (auto* muteParam = apvts.getParameter(prefix + "Mute"))
        muteParam->setValueNotifyingHost(mute ? 1.0f : 0.0f);

    if (auto* soloParam = apvts.getParameter(prefix + "Solo"))
        soloParam->setValueNotifyingHost(solo ? 1.0f : 0.0f);
}

class LoopManagerMixerIntegrationTests : public juce::UnitTest
{
public:
    LoopManagerMixerIntegrationTests() : juce::UnitTest("LoopManagerMixerIntegrationTests") {}

    void runTest() override
    {
        beginTest("LoopManager track outputs are audible after MixerEngine process");

        constexpr double sampleRate = 48000.0;
        constexpr int blockSize = 64;
        constexpr int numChannels = 2;

        MixerBridgeDummyProcessor proc;
        juce::AudioProcessorValueTreeState apvts(proc, nullptr, "PARAMS", createMixerLayout());

        SyncEngine sync;
        sync.prepare(sampleRate, blockSize);
        // Choose tempo so samples-per-beat rounds to one block, giving a valid initial loop length.
        sync.setTempo(45000.0f);

        LoopManager loopManager(sync);
        loopManager.prepareToPlay(sampleRate, blockSize, numChannels);

        MixerEngine mixer;
        mixer.attachParameters(apvts);
        mixer.prepare(sampleRate, blockSize);

        auto* track = loopManager.getTrack(0);
        expect(track != nullptr, "Track 1 should exist.");
        if (track == nullptr)
            return;

        track->armForRecording(true);
        track->startRecording(0);

        juce::AudioBuffer<float> recordedInput(numChannels, blockSize);
        fillBuffer(recordedInput, 0.7f);
        loopManager.processBlock(recordedInput);

        track->stopRecording();

        juce::AudioBuffer<float> silentInput(numChannels, blockSize);
        silentInput.clear();
        loopManager.processBlock(silentInput);

        juce::AudioBuffer<float> masterOutput(numChannels, blockSize);
        masterOutput.clear();
        mixer.process(loopManager.getTrackOutputs(), masterOutput);

        float peak = 0.0f;
        for (int ch = 0; ch < masterOutput.getNumChannels(); ++ch)
        {
            for (int i = 0; i < masterOutput.getNumSamples(); ++i)
                peak = juce::jmax(peak, std::abs(masterOutput.getSample(ch, i)));
        }

        expect(peak > 0.01f, "Expected audible mixed output from loop track.");

        beginTest("Mixer APVTS mute/solo policy owns final audibility");

        auto renderMixedPeak = [&]() -> float
        {
            loopManager.processBlock(silentInput);
            masterOutput.clear();
            mixer.process(loopManager.getTrackOutputs(), masterOutput);

            float renderedPeak = 0.0f;
            for (int ch = 0; ch < masterOutput.getNumChannels(); ++ch)
            {
                for (int i = 0; i < masterOutput.getNumSamples(); ++i)
                    renderedPeak = juce::jmax(renderedPeak, std::abs(masterOutput.getSample(ch, i)));
            }
            return renderedPeak;
        };

        const float baselinePeak = renderMixedPeak();
        expect(baselinePeak > 0.01f, "Baseline should be audible.");

        setTrackMuteSolo(apvts, 0, true, false);
        const float mixerMutedPeak = renderMixedPeak();
        expect(mixerMutedPeak < baselinePeak * 0.1f, "APVTS mute should silence the track at mixer stage.");

        setTrackMuteSolo(apvts, 0, false, false);
        setTrackMuteSolo(apvts, 1, false, true);
        const float otherTrackSoloPeak = renderMixedPeak();
        expect(otherTrackSoloPeak < baselinePeak * 0.1f, "Solo on another track should suppress this track at mixer stage.");

        setTrackMuteSolo(apvts, 0, false, true);
        const float ownSoloPeak = renderMixedPeak();
        expect(ownSoloPeak > 0.01f, "Soloing this track in APVTS should restore audibility.");
    }
};

static LoopManagerMixerIntegrationTests loopManagerMixerIntegrationTests;
