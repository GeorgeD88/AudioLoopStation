#include "TrackControlPanel.h"

//==============================================================================
TrackControlPanel::TrackControlPanel(AudioLoopStationAudioProcessor& processor,
                                     juce::AudioProcessorValueTreeState& apvtsRef)
    : audioProcessor(processor), apvts(apvtsRef), mixerEngine(processor.getMixerEngine())
{
    for (size_t i = 0; i < Config::NUM_TRACKS; ++i)
    {
        trackStrips[i] = std::make_unique<TrackStripComponent>(static_cast<int>(i), audioProcessor, apvts);
        trackStrips[i]->setMixerTrackUiState(mixerEngine.getTrackUiState(i));
        addAndMakeVisible(*trackStrips[i]);
    }

    mixerEngine.addListener(this);
}

TrackControlPanel::~TrackControlPanel()
{
    mixerEngine.removeListener(this);
}

void TrackControlPanel::mixerTrackUiStateChanged(const MixerTrackUiState& state)
{
    if (state.trackIndex >= trackStrips.size())
        return;

    if (auto& strip = trackStrips[state.trackIndex])
        strip->setMixerTrackUiState(state);
}

void TrackControlPanel::paint(juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));

    g.setColour(juce::Colours::white);
    g.drawRect(getLocalBounds(), 1);

    // Draw track separators
    auto bounds = getLocalBounds();
    auto trackWidth = bounds.getWidth() / static_cast<float>(Config::NUM_TRACKS);

    g.setColour(juce::Colours::darkgrey);
    for (int i = 1; i < Config::NUM_TRACKS; ++i)
    {
        auto x = static_cast<float>(i) * trackWidth;
        g.drawLine(x, 0.0f, x, static_cast<float>(bounds.getHeight()), 1.0f);
    }
}

void TrackControlPanel::resized()
{
    juce::FlexBox flexBox;
    flexBox.flexDirection = juce::FlexBox::Direction::row;
    flexBox.flexWrap = juce::FlexBox::Wrap::noWrap;
    flexBox.alignContent = juce::FlexBox::AlignContent::stretch;
    flexBox.alignItems = juce::FlexBox::AlignItems::stretch;
    flexBox.justifyContent = juce::FlexBox::JustifyContent::flexStart;

    constexpr float margin = 5.0f;

    for (int i = 0; i < Config::NUM_TRACKS; ++i)
    {
        flexBox.items.add(juce::FlexItem(*trackStrips[i]).withFlex(1.0f).withMinWidth(80.0f).withMargin(margin));
    }

    flexBox.performLayout(getLocalBounds().reduced(10));
}
