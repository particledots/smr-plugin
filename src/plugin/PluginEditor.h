// Milestone 4 editor: the host's generic parameter list plus a status line. The real GUI is Milestone 6.
#pragma once

#include "PluginProcessor.h"

class SmrAudioProcessorEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit SmrAudioProcessorEditor (SmrAudioProcessor&);
    ~SmrAudioProcessorEditor() override;
    void resized() override;

private:
    void timerCallback() override;

    SmrAudioProcessor& proc;
    juce::GenericAudioProcessorEditor generic;
    juce::Label statusLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SmrAudioProcessorEditor)
};
