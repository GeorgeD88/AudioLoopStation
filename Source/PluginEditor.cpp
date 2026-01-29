#include "PluginProcessor.h"
#include "PluginEditor.h"

AudioLoopStationEditor::AudioLoopStationEditor(AudioLoopStationAudioProcessor& p)
        : AudioProcessorEditor(p), audioProcessor(p), mainComponent(p)
{
    setSize(1200, 900);
    addAndMakeVisible(mainComponent);
}