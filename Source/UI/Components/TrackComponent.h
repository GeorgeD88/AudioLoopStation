//
// Created by Vincewa Tran on 4/22/26.
//
#pragma once

#include "../../Audio/LoopTrack.h"

class AudioLoopStationAudioProcessor;

class TrackComponent  : public juce::Component,
                        public juce::Timer
{
public:
    TrackComponent(AudioLoopStationAudioProcessor& processor, LoopTrack& trackToControl, int trackIndex);
    ~TrackComponent() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

private:
    AudioLoopStationAudioProcessor& processor;
    LoopTrack& track;
    int trackID;

    juce::TextButton recPlayButton   { "REC" };
    juce::TextButton stopButton      { "STOP" };
    juce::TextButton undoButton      { "UNDO" };
    juce::TextButton afterLoopButton { "AFTER" };
    juce::TextButton clearButton     { juce::CharPointer_UTF8("\xc3\x97") };
    juce::TextButton fxReplaceButton { "FX" };
    juce::TextButton muteButton      { "M" };
    juce::TextButton soloButton      { "S" };
    juce::Slider     volumeSlider;
    juce::ComboBox   mOutputSelector;

    void updateButtonVisuals();

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>   mVolAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>   mRecAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>   mStopAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>   mClearAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>   mMuteAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>   mSoloAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>   mUndoAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>   mAfterLoopAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> mOutSelectAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>   mResampleAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TrackComponent)
};
