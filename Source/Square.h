//
// Created by Vincewa Tran on 10/27/25.
//

#pragma once

#include <juce_gui_basics/juce_gui_basics.h>


class Square : public juce::Component
{
public:
    void paint (juce::Graphics&) override;
    void resized() override;
};