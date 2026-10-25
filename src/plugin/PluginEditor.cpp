#include "PluginEditor.h"

SmrAudioProcessorEditor::SmrAudioProcessorEditor (SmrAudioProcessor& p)
    : AudioProcessorEditor (&p)
{
    setSize (420, 160);
}

void SmrAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1c1c1e));
    g.setColour (juce::Colours::white);
    g.setFont (20.0f);
    g.drawText ("Unofficial SMR", getLocalBounds().removeFromTop (90), juce::Justification::centredBottom);
    g.setFont (14.0f);
    g.setColour (juce::Colours::lightgrey);
    g.drawText ("Milestone 0: empty pass-through. Audio is not changed.",
                getLocalBounds().withTrimmedTop (95), juce::Justification::centredTop);
}
