#include "TrackControlPanel.h"
#include "../../PluginProcessor.h"

//==============================================================================
TrackControlPanel::TrackControlPanel(AudioLoopStationAudioProcessor& processor)
    : apvts(processor.getApvts())
{
    for (int i = 0; i < TrackConfig::MAX_TRACKS; ++i)
    {
        trackStrips[i] = std::make_unique<TrackStripComponent>(i, apvts, processor.getLoopManager());
        addAndMakeVisible(*trackStrips[i]);
    }
    startTimerHz(15);
}

TrackControlPanel::~TrackControlPanel()
{
    stopTimer();
}

void TrackControlPanel::timerCallback()
{
    for (auto& strip : trackStrips)
        if (strip != nullptr)
            strip->syncArmButtonWithEngine();
    for (auto& strip : trackStrips)
        if (strip != nullptr)
            strip->repaint();
}

void TrackControlPanel::paint(juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));

    g.setColour(juce::Colours::white);
    g.drawRect(getLocalBounds(), 1);

    // Draw track separators
    auto bounds = getLocalBounds();
    auto trackWidth = bounds.getWidth() / static_cast<float>(TrackConfig::MAX_TRACKS);

    g.setColour(juce::Colours::darkgrey);
    for (int i = 1; i < TrackConfig::MAX_TRACKS; ++i)
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

    for (int i = 0; i < TrackConfig::MAX_TRACKS; ++i)
    {
        flexBox.items.add(juce::FlexItem(*trackStrips[i]).withFlex(1.0f).withMinWidth(80.0f).withMargin(margin));
    }

    flexBox.performLayout(getLocalBounds().reduced(10));
}
