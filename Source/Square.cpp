//
// Created by Vincewa Tran on 10/27/25.
//

#include "Square.h"

void Square::paint (juce::Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll (juce::Colours::orange);

    g.setColour (juce::Colours::white);
    g.setFont (15.0f);
    g.drawFittedText ("Audio Loop Station!", getLocalBounds(), juce::Justification::centred, 1);
}

void Square::resized()
{

}