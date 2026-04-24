//
// Created by Vincewa Tran on 4/22/26.
//

#pragma once
#include "juce_gui_basics/juce_gui_basics.h"
/**
 * Central colour scheme for AudioLoopStation.
 * Uses the track colours (red, teal, yellow, green) as accents,
 * and a dark background that matches the original UI.
 */

namespace Colours_
{
    // Background & surfaces
    const juce::Colour bg           { 0xff12121a };
    const juce::Colour surface      { 0xff1e1e2c };
    const juce::Colour surfaceLight { 0xff2a2a3c };
    const juce::Colour border       { 0xff3a3a4e };

    // Text
    const juce::Colour textPrimary  { 0xfff0f0f8 };
    const juce::Colour textDim      { 0xff8888aa };

    // Track accent colors from original TrackStripComponent::getTrackColour
    const juce::Colour trackRed     { 0xffe53935 };
    const juce::Colour trackTeal    { 0xff009688 };
    const juce::Colour trackYellow  { 0xfffdd835 };
    const juce::Colour trackGreen   { 0xff43a047 };

    // Transport / button colours (matching original TransportComponent)
    const juce::Colour rec          = trackRed;          // record button (red)
    const juce::Colour play         = trackGreen;        // play button (green)
    const juce::Colour stop         { 0xffffa500 };      // orange (original stop colour)
    const juce::Colour undo         { 0xff0000ff };      // blue (original undo colour)


    // Track state colours (used in TrackStripComponent)
    const juce::Colour dub          = trackYellow;       // overdubbing / queued
    const juce::Colour idle         { 0xff2d2d4e };      // inactive
    const juce::Colour mute         = trackRed;          // mute indicator
    const juce::Colour solo         { 0xffffd93d };      // solo (yellow)
    const juce::Colour afterloop    { 0xff00b8d4 };      // after loop (cyan)
    const juce::Colour fxReady      { 0xffbb86fc };      // FX replace ready (purple)
    const juce::Colour clear        { 0xff6e3040 };      // clear (dark red)
    const juce::Colour divMul       { 0xff3d4f7c };      // multiply/divide buttons

    // Sliders
    const juce::Colour sliderTrack  { 0xff2a2a46 };
    const juce::Colour sliderThumb  = play;              // use play green for thumb

    // Beat indicators (for TrackComponent, if used)
    const juce::Colour beatActive   { 0xffffa040 };
    const juce::Colour beatIdle     { 0xff2a2b4a };
    const juce::Colour beatMuted    { 0xff3d3050 };
}

class CustomLookAndFeel : public juce::LookAndFeel_V4
{
public:
    CustomLookAndFeel()
    {
        setColour(juce::ResizableWindow::backgroundColourId, Colours_::bg);
        setColour(juce::ComboBox::backgroundColourId,        Colours_::idle);
        setColour(juce::ComboBox::outlineColourId,           Colours_::border);
        setColour(juce::ComboBox::textColourId,              Colours_::textPrimary);
        setColour(juce::PopupMenu::backgroundColourId,       Colours_::surface);
        setColour(juce::PopupMenu::textColourId,             Colours_::textPrimary);
        setColour(juce::PopupMenu::highlightedBackgroundColourId, Colours_::surfaceLight);
        setColour(juce::TextButton::buttonColourId,          Colours_::idle);
        setColour(juce::TextButton::textColourOffId,         Colours_::textPrimary);
        setColour(juce::TextButton::textColourOnId,          Colours_::textPrimary);
    }

    void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                              const juce::Colour& bgColour,
                              bool isMouseOver, bool isButtonDown) override
    {
        auto bounds = button.getLocalBounds().toFloat().reduced(1.0f);
        float cornerSize = 6.0f;

        auto colour = bgColour;
        if (isButtonDown)    colour = colour.brighter(0.15f);
        else if (isMouseOver) colour = colour.brighter(0.08f);

        g.setColour(colour);
        g.fillRoundedRectangle(bounds, cornerSize);

        g.setColour(colour.brighter(0.12f));
        g.drawRoundedRectangle(bounds, cornerSize, 1.0f);
    }

    void drawButtonText(juce::Graphics& g, juce::TextButton& button,
                        bool isMouseOver, bool isButtonDown) override
    {
        auto font = juce::FontOptions(11.0f, juce::Font::bold);
        g.setFont(font);
        g.setColour(button.findColour(juce::TextButton::textColourOnId)
                            .withAlpha(button.isEnabled() ? 1.0f : 0.4f));

        auto bounds = button.getLocalBounds();
        g.drawText(button.getButtonText(), bounds, juce::Justification::centred, false);
    }

    void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, float /*minPos*/, float /*maxPos*/,
                          juce::Slider::SliderStyle, juce::Slider& slider) override
    {
        auto bounds = juce::Rectangle<float>((float)x, (float)y, (float)width, (float)height);
        float trackH = 4.0f;
        float trackY = bounds.getCentreY() - trackH * 0.5f;

        // Track background
        g.setColour(Colours_::sliderTrack);
        g.fillRoundedRectangle(bounds.getX(), trackY, bounds.getWidth(), trackH, 2.0f);

        // Filled portion
        g.setColour(Colours_::sliderThumb);
        g.fillRoundedRectangle(bounds.getX(), trackY, sliderPos - (float)x, trackH, 2.0f);

        // Thumb
        float thumbSize = 12.0f;
        g.setColour(Colours_::textPrimary);
        g.fillEllipse(sliderPos - thumbSize * 0.5f, bounds.getCentreY() - thumbSize * 0.5f,
                      thumbSize, thumbSize);
    }
};
