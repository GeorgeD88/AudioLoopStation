/**
 * Test thread-safe parameter communication (UI -> Audio)
 */

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_core/juce_core.h>
#include <atomic>
#include <thread>
#include <chrono>

#include "../Utils/TrackConfig.h"

//==============================================================================
/** Minimal processor with the same APVTS parameter layout as AudioLoopStation */
class TestProcessor : public juce::AudioProcessor
{
public:
    TestProcessor()
        : AudioProcessor(BusesProperties().withOutput("Out", juce::AudioChannelSet::stereo(), true)),
          apvts(*this, nullptr, "PARAMS", createLayout()) {}

    ~TestProcessor() override = default;

    const juce::String getName() const override { return "TestProcessor"; }
    void prepareToPlay(double, int) override {}
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout&) const override { return true; }
    void processBlock(juce::AudioBuffer<float>& b, juce::MidiBuffer& m) override
    {
        juce::ignoreUnused(b, m);
    }
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

    juce::AudioProcessorValueTreeState& getApvts() { return apvts; }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        juce::AudioProcessorValueTreeState::ParameterLayout layout;
        for (int i = 0; i < TrackConfig::MAX_TRACKS; ++i)
        {
            auto prefix = "Track" + juce::String(i + 1) + "_";
            layout.add(std::make_unique<juce::AudioParameterFloat>(
                prefix + "Volume", prefix + "Volume",
                juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.8f));
            layout.add(std::make_unique<juce::AudioParameterFloat>(
                prefix + "Pan", prefix + "Pan",
                juce::NormalisableRange<float>(-1.0f, 1.0f, 0.01f), 0.0f));
            layout.add(std::make_unique<juce::AudioParameterBool>(prefix + "Mute", prefix + "Mute", false));
            layout.add(std::make_unique<juce::AudioParameterBool>(prefix + "Solo", prefix + "Solo", false));
        }
        return layout;
    }

    juce::AudioProcessorValueTreeState apvts;
};

//==============================================================================
class ParameterThreadSafetyTests : public juce::UnitTest
{
public:
    ParameterThreadSafetyTests() : juce::UnitTest("ParameterThreadSafetyTests") {}

    void runTest() override
    {
        beginTest("Parameter values are readable from audio thread after UI write");

        TestProcessor proc;
        proc.prepareToPlay(48000.0, 64);

        auto& apvts = proc.getApvts();
        auto* volParam = apvts.getRawParameterValue("Track1_Volume");
        auto* panParam = apvts.getRawParameterValue("Track1_Pan");

        expect(volParam != nullptr, "Track1_Volume parameter should exist");
        expect(panParam != nullptr, "Track1_Pan parameter should exist");

        // Simulate UI thread: set values via atomic store
        volParam->store(0.5f);
        panParam->store(-0.5f);

        // Simulate audio thread: read values via atomic load (as processBlock would)
        float vol = volParam->load();
        float pan = panParam->load();

        expect(vol == 0.5f, "Audio thread should read volume value set by UI");
        expect(pan == -0.5f, "Audio thread should read pan value set by UI");

        beginTest("Concurrent UI write and audio read completes without crash");

        std::atomic<bool> stopFlag{false};
        std::atomic<int> audioReadCount{0};
        std::exception_ptr audioException;

        // UI-like thread: continuously write to parameters
        std::thread uiThread([&apvts, &stopFlag]() {
            float v = 0.0f;
            while (!stopFlag.load())
            {
                for (int t = 0; t < TrackConfig::MAX_TRACKS; ++t)
                {
                    auto prefix = "Track" + juce::String(t + 1) + "_";
                    if (auto* vp = apvts.getRawParameterValue(prefix + "Volume"))
                        vp->store(v);
                    if (auto* pp = apvts.getRawParameterValue(prefix + "Pan"))
                        pp->store(v * 2.0f - 1.0f);
                    if (auto* mp = apvts.getRawParameterValue(prefix + "Mute"))
                        mp->store((t % 2) ? 1.0f : 0.0f);
                    if (auto* sp = apvts.getRawParameterValue(prefix + "Solo"))
                        sp->store((t % 3) ? 1.0f : 0.0f);
                }
                v += 0.01f;
                if (v > 1.0f) v = 0.0f;
            }
        });

        // Audio-like thread: continuously read parameters (as processBlock would)
        std::thread audioThread([&proc, &apvts, &stopFlag, &audioReadCount, &audioException]() {
            try
            {
                juce::AudioBuffer<float> buffer(2, 64);
                juce::MidiBuffer midi;

                while (!stopFlag.load())
                {
                    for (int t = 0; t < TrackConfig::MAX_TRACKS; ++t)
                    {
                        auto prefix = "Track" + juce::String(t + 1) + "_";
                        if (auto* vp = apvts.getRawParameterValue(prefix + "Volume"))
                            juce::ignoreUnused(vp->load());
                        if (auto* pp = apvts.getRawParameterValue(prefix + "Pan"))
                            juce::ignoreUnused(pp->load());
                        if (auto* mp = apvts.getRawParameterValue(prefix + "Mute"))
                            juce::ignoreUnused(mp->load());
                        if (auto* sp = apvts.getRawParameterValue(prefix + "Solo"))
                            juce::ignoreUnused(sp->load());
                    }
                    proc.processBlock(buffer, midi);
                    audioReadCount.fetch_add(1);
                }
            }
            catch (...)
            {
                audioException = std::current_exception();
            }
        });

        // Run for a short period to stress-test
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        stopFlag.store(true);

        uiThread.join();
        audioThread.join();

        expect(audioException == nullptr, "Audio thread should not throw during concurrent access");
        expect(audioReadCount.load() > 0, "Audio thread should have completed at least one processBlock");

        beginTest("All track parameters exist and are accessible");

        for (int t = 0; t < TrackConfig::MAX_TRACKS; ++t)
        {
            auto prefix = "Track" + juce::String(t + 1) + "_";
            expect(apvts.getRawParameterValue(prefix + "Volume") != nullptr, "Volume param exists");
            expect(apvts.getRawParameterValue(prefix + "Pan") != nullptr, "Pan param exists");
            expect(apvts.getRawParameterValue(prefix + "Mute") != nullptr, "Mute param exists");
            expect(apvts.getRawParameterValue(prefix + "Solo") != nullptr, "Solo param exists");
        }
    }
};

static ParameterThreadSafetyTests parameterThreadSafetyTests;
