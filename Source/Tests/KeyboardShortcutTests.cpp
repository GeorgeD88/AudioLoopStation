/**
 * KeyboardShortcutTests.cpp
 *
 * Automated tests that mirror the logic in AudioLoopStationEditor::keyPressed()
 * (PluginEditor.cpp).  No real key events are fired; instead the tests call
 * the same underlying APIs directly so the behaviour is fully verifiable
 * without a GUI or DAW host.
 *
 * Keys 1–4  -> drive the LoopTrack state machine:
 *               Empty        -> setRecording()
 *               Recording    -> setPlaying()
 *               Playing      -> setOverdubbing()
 *               Overdubbing  -> setPlaying()
 *               Stopped      -> setPlaying()
 *
 * SHIFT+1–4 -> toggle Track{N}_Mute in the APVTS
 *               (identical toggle expression used in PluginEditor::keyPressed)
 */

#include <juce_audio_processors/juce_audio_processors.h>
#include "LoopTrack.h"
#include "../Utils/Config.h"

// ---------------------------------------------------------------------------
// Minimal processor stub required by AudioProcessorValueTreeState
// ---------------------------------------------------------------------------
class KSTestProcessor : public juce::AudioProcessor
{
public:
    KSTestProcessor() = default;
    ~KSTestProcessor() override = default;

    const juce::String getName() const override { return "KSTestProcessor"; }
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

// ---------------------------------------------------------------------------
// Build a minimal APVTS that contains only Mute and Solo for all tracks
// (the keyboard handler only needs Mute; Solo is included so the layout
// matches what the rest of the app expects and tests stay isolated).
// ---------------------------------------------------------------------------
static juce::AudioProcessorValueTreeState::ParameterLayout createKSLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (int i = 0; i < Config::NUM_TRACKS; ++i)
    {
        const auto prefix = "Track" + juce::String(i + 1) + "_";
        layout.add(std::make_unique<juce::AudioParameterBool>(
            prefix + "Mute", prefix + "Mute", false));
        layout.add(std::make_unique<juce::AudioParameterBool>(
            prefix + "Solo", prefix + "Solo", false));
    }
    return layout;
}

// ---------------------------------------------------------------------------
// Helper: mirrors the SHIFT+digit handler in PluginEditor::keyPressed().
// Returns false if the parameter does not exist.
// ---------------------------------------------------------------------------
static bool simulateShiftDigit(juce::AudioProcessorValueTreeState& apvts, int trackIndex)
{
    const juce::String paramId = "Track" + juce::String(trackIndex + 1) + "_Mute";
    if (auto* param = apvts.getParameter(paramId))
    {
        const float toggled = param->getValue() > 0.5f ? 0.0f : 1.0f;
        param->beginChangeGesture();
        param->setValueNotifyingHost(toggled);
        param->endChangeGesture();
        return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// Helper: process one silent block so a slave track can advance its counter.
// ---------------------------------------------------------------------------
static void processOneBlock(LoopTrack& track, juce::int64 globalSamples,
                            bool isMaster, int masterLoopLength, int blockSize)
{
    juce::AudioBuffer<float> output(2, blockSize);
    juce::AudioBuffer<float> input(2, blockSize);
    juce::AudioBuffer<float> sc(2, blockSize);
    output.clear();
    input.clear();
    sc.clear();
    track.processBlock(output, input, sc, globalSamples, isMaster, masterLoopLength, false);
}

// ===========================================================================
class KeyboardShortcutTests : public juce::UnitTest
{
public:
    KeyboardShortcutTests() : juce::UnitTest("KeyboardShortcutTests") {}

    void runTest() override
    {
        // -------------------------------------------------------------------
        // Part 1 - digit key (1-4) : LoopTrack state-machine transitions
        //
        // Each sub-test mimics pressing the corresponding digit key once
        // and verifies the state change that PluginEditor::keyPressed produces.
        // -------------------------------------------------------------------

        beginTest("digit key on Empty track transitions to Recording");
        {
            LoopTrack track;
            track.prepareToPlay(48000.0, 256);

            expect(track.getState() == LoopTrack::State::Empty,
                   "Track should start Empty.");

            // Digit key on Empty → setRecording()
            track.setRecording();

            expect(track.getState() == LoopTrack::State::Recording,
                   "State should be Recording after pressing key on Empty track.");
        }

        beginTest("digit key on Recording track transitions to Playing");
        {
            LoopTrack track;
            track.prepareToPlay(48000.0, 256);

            track.setRecording();

            // Record at least one block so loopLengthSamples will be non-zero
            processOneBlock(track, 0, true, 0, 256);

            // Digit key on Recording → setPlaying()
            track.setPlaying();

            expect(track.getState() == LoopTrack::State::Playing,
                   "State should be Playing after pressing key on Recording track.");
            expect(track.hasLoop(),
                   "Track should have a loop once recording → playing transition occurs.");
        }

        beginTest("digit key on Playing track transitions to Overdubbing");
        {
            LoopTrack track;
            track.prepareToPlay(48000.0, 256);

            track.setRecording();
            processOneBlock(track, 0, true, 0, 256);
            track.setPlaying();

            expect(track.getState() == LoopTrack::State::Playing,
                   "Pre-condition: track must be Playing.");

            // Digit key on Playing → setOverdubbing()
            track.setOverdubbing();

            expect(track.getState() == LoopTrack::State::Overdubbing,
                   "State should be Overdubbing after pressing key on Playing track.");
        }

        beginTest("digit key on Overdubbing track returns to Playing");
        {
            LoopTrack track;
            track.prepareToPlay(48000.0, 256);

            track.setRecording();
            processOneBlock(track, 0, true, 0, 256);
            track.setPlaying();
            track.setOverdubbing();

            expect(track.getState() == LoopTrack::State::Overdubbing,
                   "Pre-condition: track must be Overdubbing.");

            // Digit key on Overdubbing → setPlaying()
            track.setPlaying();

            expect(track.getState() == LoopTrack::State::Playing,
                   "State should return to Playing after pressing key on Overdubbing track.");
        }

        beginTest("digit key on Stopped track resumes Playing");
        {
            LoopTrack track;
            track.prepareToPlay(48000.0, 256);

            track.setRecording();
            processOneBlock(track, 0, true, 0, 256);
            track.setPlaying();
            track.stop();

            expect(track.getState() == LoopTrack::State::Stopped,
                   "Pre-condition: track must be Stopped.");

            // Digit key on Stopped -> setPlaying()
            track.setPlaying();

            expect(track.getState() == LoopTrack::State::Playing,
                   "State should be Playing after pressing key on Stopped track.");
        }

        // -------------------------------------------------------------------
        // Part 2 - SHIFT+digit key : APVTS Mute toggle
        //
        // Mirrors exactly the expression used in PluginEditor::keyPressed():
        //   const float toggled = param->getValue() > 0.5f ? 0.0f : 1.0f;
        //   param->beginChangeGesture();
        //   param->setValueNotifyingHost(toggled);
        //   param->endChangeGesture();
        // -------------------------------------------------------------------

        beginTest("SHIFT+digit toggles Track_Mute from unmuted to muted");
        {
            KSTestProcessor proc;
            juce::AudioProcessorValueTreeState apvts(proc, nullptr, "PARAMS", createKSLayout());

            // All tracks start unmuted (default = false → 0.0f).
            auto* raw = apvts.getRawParameterValue("Track1_Mute");
            expect(raw != nullptr, "Track1_Mute parameter must exist.");
            expectWithinAbsoluteError(raw->load(), 0.0f, 0.001f,
                                      "Track1_Mute should start unmuted.");

            // Simulate SHIFT+1
            const bool handled = simulateShiftDigit(apvts, 0);
            expect(handled, "simulateShiftDigit should find the parameter and return true.");

            expectWithinAbsoluteError(raw->load(), 1.0f, 0.001f,
                                      "Track1_Mute should be muted after first SHIFT+1 press.");
        }

        beginTest("SHIFT+digit toggles Track_Mute back to unmuted on second press");
        {
            KSTestProcessor proc;
            juce::AudioProcessorValueTreeState apvts(proc, nullptr, "PARAMS", createKSLayout());

            simulateShiftDigit(apvts, 0);  // mute
            simulateShiftDigit(apvts, 0);  // unmute

            auto* raw = apvts.getRawParameterValue("Track1_Mute");
            expectWithinAbsoluteError(raw->load(), 0.0f, 0.001f,
                                      "Track1_Mute should be back to unmuted after two SHIFT+1 presses.");
        }

        beginTest("SHIFT+digit only changes the targeted track's mute parameter");
        {
            KSTestProcessor proc;
            juce::AudioProcessorValueTreeState apvts(proc, nullptr, "PARAMS", createKSLayout());

            // Mute Track 3 only
            simulateShiftDigit(apvts, 2);

            for (int i = 0; i < Config::NUM_TRACKS; ++i)
            {
                const auto paramId = "Track" + juce::String(i + 1) + "_Mute";
                const float value  = apvts.getRawParameterValue(paramId)->load();

                if (i == 2)
                    expectWithinAbsoluteError(value, 1.0f, 0.001f,
                                              "Track3_Mute should be muted.");
                else
                    expectWithinAbsoluteError(value, 0.0f, 0.001f,
                                              "Other tracks must remain unmuted.");
            }
        }

        beginTest("SHIFT+digit works independently for all four tracks");
        {
            KSTestProcessor proc;
            juce::AudioProcessorValueTreeState apvts(proc, nullptr, "PARAMS", createKSLayout());

            // Mute tracks 2 and 4 (indices 1 and 3)
            simulateShiftDigit(apvts, 1);
            simulateShiftDigit(apvts, 3);

            for (int i = 0; i < Config::NUM_TRACKS; ++i)
            {
                const auto paramId = "Track" + juce::String(i + 1) + "_Mute";
                const float value  = apvts.getRawParameterValue(paramId)->load();
                const bool expectMuted = (i == 1 || i == 3);

                if (expectMuted)
                    expectWithinAbsoluteError(value, 1.0f, 0.001f,
                                              "Track" + juce::String(i + 1) + " should be muted.");
                else
                    expectWithinAbsoluteError(value, 0.0f, 0.001f,
                                              "Track" + juce::String(i + 1) + " should be unmuted.");
            }
        }

        // -------------------------------------------------------------------
        // Part 3 - Clear button: track.clear() resets to Empty with no loop
        // -------------------------------------------------------------------

        beginTest("Clear button resets a playing track to Empty with no loop");
        {
            LoopTrack track;
            track.prepareToPlay(48000.0, 256);

            track.setRecording();
            processOneBlock(track, 0, true, 0, 256);
            track.setPlaying();

            expect(track.hasLoop(),   "Track should have a loop before Clear.");
            expect(track.getLoopLengthSamples() > 0, "Loop length should be positive before Clear.");

            track.clear();

            expect(track.getState() == LoopTrack::State::Empty,
                   "State should be Empty after Clear.");
            expect(!track.hasLoop(),
                   "hasLoop() should return false after Clear.");
            expect(track.getLoopLengthSamples() == 0,
                   "Loop length should be 0 after Clear.");
        }

        // -------------------------------------------------------------------
        // Part 4 - Mute/Solo buttons: APVTS parameter connectivity
        //
        // The buttons use ButtonAttachment -> the parameters are the ground
        // truth.  These tests verify the parameters exist, default correctly,
        // and respond to setValueNotifyingHost (same path as the button click).
        // -------------------------------------------------------------------

        beginTest("Mute and Solo parameters exist for all four tracks");
        {
            KSTestProcessor proc;
            juce::AudioProcessorValueTreeState apvts(proc, nullptr, "PARAMS", createKSLayout());

            for (int i = 0; i < Config::NUM_TRACKS; ++i)
            {
                const auto prefix = "Track" + juce::String(i + 1) + "_";
                expect(apvts.getParameter(prefix + "Mute") != nullptr,
                       prefix + "Mute parameter must exist.");
                expect(apvts.getParameter(prefix + "Solo") != nullptr,
                       prefix + "Solo parameter must exist.");
            }
        }

        beginTest("Mute parameter defaults to false and can be toggled");
        {
            KSTestProcessor proc;
            juce::AudioProcessorValueTreeState apvts(proc, nullptr, "PARAMS", createKSLayout());

            auto* muteParam = apvts.getParameter("Track2_Mute");
            expect(muteParam != nullptr, "Track2_Mute must exist.");

            // Default is false (0.0)
            expectWithinAbsoluteError(muteParam->getValue(), 0.0f, 0.001f,
                                      "Mute should default to off.");

            // Simulate button press → on
            muteParam->beginChangeGesture();
            muteParam->setValueNotifyingHost(1.0f);
            muteParam->endChangeGesture();
            expectWithinAbsoluteError(muteParam->getValue(), 1.0f, 0.001f,
                                      "Mute should be on after toggle.");

            // Simulate button press → off
            muteParam->beginChangeGesture();
            muteParam->setValueNotifyingHost(0.0f);
            muteParam->endChangeGesture();
            expectWithinAbsoluteError(muteParam->getValue(), 0.0f, 0.001f,
                                      "Mute should be off after second toggle.");
        }
    }
};

static KeyboardShortcutTests keyboardShortcutTests;
