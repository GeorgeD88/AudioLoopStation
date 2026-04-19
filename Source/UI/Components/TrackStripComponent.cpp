#include "TrackStripComponent.h"
#include <array>

//==============================================================================
juce::String TrackStripComponent::trackParameterPrefix() const
{
    return "Track" + juce::String(trackIndex + 1) + "_";
}

//==============================================================================
juce::Colour TrackStripComponent::getTrackColour(int index)
{
    static const std::array<juce::Colour, 4> colours = {
        juce::Colour(0xffe53935),  // red
        juce::Colour(0xff009688),  // teal
        juce::Colour(0xfffdd835),  // yellow
        juce::Colour(0xff43a047)   // green
    };
    return colours[static_cast<size_t>(index % 4)];
}

//==============================================================================
TrackStripComponent::TrackStripComponent(int trackIdx,
                                         juce::AudioProcessorValueTreeState& apvtsRef,
                                         LoopManager& loopManagerRef)
    : trackIndex(trackIdx), apvts(apvtsRef), loopManager(loopManagerRef)
{
    setupControls();
}

TrackStripComponent::~TrackStripComponent()
{
}

void TrackStripComponent::setupControls()
{
    // Track label with track colour
    trackLabel.setText("Track " + juce::String(trackIndex + 1), juce::dontSendNotification);
    trackLabel.setJustificationType(juce::Justification::centred);
    trackLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(trackLabel);

    // Volume slider
    volumeSlider.setSliderStyle(juce::Slider::LinearVertical);
    volumeSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    volumeSlider.setRange(0.0, 1.0, 0.01);
    volumeSlider.setValue(0.8);
    addAndMakeVisible(volumeSlider);

    // Pan slider
    panSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    panSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    panSlider.setRange(-1.0, 1.0, 0.01);
    panSlider.setValue(0.0);
    addAndMakeVisible(panSlider);

    // Buttons
    recordArmButton.setButtonText("ARM");
    recordArmButton.setColour(juce::TextButton::buttonColourId, juce::Colours::grey);
    recordArmButton.setColour(juce::TextButton::buttonOnColourId, juce::Colours::red);
    recordArmButton.setClickingTogglesState(true);
    recordArmButton.onClick = [this]
    {
        if (auto* track = loopManager.getTrack(static_cast<size_t>(trackIndex)))
            track->armForRecording(recordArmButton.getToggleState());
    };
    addAndMakeVisible(recordArmButton);

    muteButton.setButtonText("M");
    muteButton.setColour(juce::TextButton::buttonColourId, juce::Colours::darkgrey);
    muteButton.setColour(juce::TextButton::buttonOnColourId, juce::Colours::red);
    muteButton.setClickingTogglesState(true);
    addAndMakeVisible(muteButton);

    soloButton.setButtonText("S");
    soloButton.setColour(juce::TextButton::buttonColourId, juce::Colours::darkgrey);
    soloButton.setColour(juce::TextButton::buttonOnColourId, juce::Colours::yellow);
    soloButton.setClickingTogglesState(true);
    addAndMakeVisible(soloButton);

    clearButton.setButtonText("CLEAR");
    clearButton.setColour(juce::TextButton::buttonColourId, juce::Colours::darkgrey);
    addAndMakeVisible(clearButton);

    // APVTS attachments
    const juce::String trackPrefix = trackParameterPrefix();
    volumeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, trackPrefix + "Volume", volumeSlider);
    panAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, trackPrefix + "Pan", panSlider);
    muteAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        apvts, trackPrefix + "Mute", muteButton);
    soloAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        apvts, trackPrefix + "Solo", soloButton);
}

void TrackStripComponent::paint(juce::Graphics& g)
{
    const auto state = loopManager.getTrackState(static_cast<size_t>(trackIndex));
    const juce::String prefix = trackParameterPrefix();

    bool muted = false;
    bool solo = false;
    if (auto* muteParam = apvts.getRawParameterValue(prefix + "Mute"))
        muted = muteParam->load() > 0.5f;
    if (auto* soloParam = apvts.getRawParameterValue(prefix + "Solo"))
        solo = soloParam->load() > 0.5f;

    const bool armed = loopManager.isTrackArmed(static_cast<size_t>(trackIndex));
    const juce::Colour identity = getTrackColour(trackIndex);

    juce::Colour fillColour;
    juce::Colour headerColour;
    juce::Colour borderColour;
    int borderThickness = 1;

    if (state == LoopTrack::State::Recording)
    {
        fillColour = juce::Colours::red.withAlpha(0.28f);
        headerColour = juce::Colours::darkred.withAlpha(0.75f);
        borderColour = juce::Colours::red.withAlpha(0.9f);
        borderThickness = 2;
    }
    else if (muted)
    {
        fillColour = juce::Colours::dimgrey.withAlpha(0.45f);
        headerColour = juce::Colours::grey.withAlpha(0.55f);
        borderColour = juce::Colours::lightgrey.withAlpha(0.7f);
    }
    else if (state == LoopTrack::State::Playing)
    {
        fillColour = identity.withAlpha(0.18f);
        headerColour = identity.interpolatedWith(juce::Colours::limegreen, 0.45f).withAlpha(0.55f);
        borderColour = juce::Colours::limegreen.withAlpha(0.65f);
    }
    else if (state == LoopTrack::State::Stopped)
    {
        fillColour = identity.withAlpha(0.12f);
        headerColour = juce::Colours::darkorange.withAlpha(0.4f);
        borderColour = identity.withAlpha(0.45f);
    }
    else
    {
        fillColour = identity.withAlpha(0.15f);
        headerColour = identity.withAlpha(0.4f);
        borderColour = identity.withAlpha(0.6f);
    }

    if (armed && state != LoopTrack::State::Recording)
    {
        borderColour = borderColour.interpolatedWith(juce::Colours::orange, 0.55f);
        headerColour = headerColour.interpolatedWith(juce::Colours::orange, 0.35f);
        borderThickness = juce::jmax(borderThickness, 2);
    }

    g.fillAll(fillColour);

    auto headerBounds = getLocalBounds().removeFromTop(20);
    g.setColour(headerColour);
    g.fillRect(headerBounds);

    g.setColour(borderColour);
    g.drawRect(getLocalBounds(), borderThickness);

    if (solo)
    {
        g.setColour(juce::Colours::yellow.withAlpha(0.85f));
        g.drawRect(getLocalBounds().reduced(1), 2);
    }
}

void TrackStripComponent::resized()
{
    auto bounds = getLocalBounds();

    // 1. Manually position the label to cover the top 20 pixels.
    // This matches the 'headerBounds' I created in paint().
    trackLabel.setBounds(bounds.removeFromTop(20));

    // 2. Setup the FlexBox for the rest of the controls.
    juce::FlexBox flexBox;
    flexBox.flexDirection = juce::FlexBox::Direction::column;
    flexBox.flexWrap = juce::FlexBox::Wrap::noWrap;
    flexBox.alignContent = juce::FlexBox::AlignContent::stretch;
    flexBox.alignItems = juce::FlexBox::AlignItems::stretch;
    flexBox.justifyContent = juce::FlexBox::JustifyContent::flexStart;

    constexpr float margin = 2.0f;

    // volumeSlider
    flexBox.items.add(juce::FlexItem(volumeSlider).withFlex(1.0f).withMinHeight(60.0f).withMargin(margin));

    // panSlider
    flexBox.items.add(juce::FlexItem(panSlider).withHeight(30.0f).withMargin(margin));

    // Button row 1: ARM and Mute
    juce::FlexBox buttonRow1;
    buttonRow1.flexDirection = juce::FlexBox::Direction::row;
    buttonRow1.items.add(juce::FlexItem(recordArmButton).withFlex(1.0f).withMargin(1.0f));
    buttonRow1.items.add(juce::FlexItem(muteButton).withFlex(1.0f).withMargin(1.0f));
    flexBox.items.add(juce::FlexItem(buttonRow1).withHeight(22.0f));

    // Button row 2: Solo and Clear
    juce::FlexBox buttonRow2;
    buttonRow2.flexDirection = juce::FlexBox::Direction::row;
    buttonRow2.items.add(juce::FlexItem(soloButton).withFlex(1.0f).withMargin(1.0f));
    buttonRow2.items.add(juce::FlexItem(clearButton).withFlex(1.0f).withMargin(1.0f));
    flexBox.items.add(juce::FlexItem(buttonRow2).withHeight(22.0f));

    // 3. Perform layout on the remaining bounds (already reduced by removeFromTop).
    flexBox.performLayout(bounds.reduced(5));
}

void TrackStripComponent::syncArmButtonWithEngine()
{
    const bool armed = loopManager.isTrackArmed(static_cast<size_t>(trackIndex));
    if (recordArmButton.getToggleState() != armed)
        recordArmButton.setToggleState(armed, juce::dontSendNotification);
}
