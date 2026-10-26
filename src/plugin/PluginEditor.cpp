#include "PluginEditor.h"

SmrAudioProcessorEditor::SmrAudioProcessorEditor (SmrAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p), generic (p)
{
    addAndMakeVisible (generic);
    statusLabel.setJustificationType (juce::Justification::centredLeft);
    statusLabel.setColour (juce::Label::backgroundColourId, juce::Colour (0xff1c1c1e));
    statusLabel.setColour (juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible (statusLabel);
    setSize (520, 640);
    setResizable (true, true);
    timerCallback();
    startTimerHz (4);
}

SmrAudioProcessorEditor::~SmrAudioProcessorEditor() { stopTimer(); }

void SmrAudioProcessorEditor::resized()
{
    auto r = getLocalBounds();
    statusLabel.setBounds (r.removeFromTop (44));
    generic.setBounds (r);
}

void SmrAudioProcessorEditor::timerCallback()
{
    const auto s = proc.status();
    juce::String t = "Unofficial SMR - Milestone 4: parameters wired to the core. GUI is Milestone 6.\n";
    if (s.rateSupported)
        t << "Host " << juce::String (s.hostRate, 0) << " Hz, latency " << s.latencySamples << " samples, underruns " << (juce::int64) s.underruns;
    else
        t << "Host rate " << juce::String (s.hostRate, 0) << " Hz is not supported: audio passes through unchanged.";
    statusLabel.setText (t, juce::dontSendNotification);
}
