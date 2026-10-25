#pragma once

#include "PluginProcessor.h"

class SmrAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit SmrAudioProcessorEditor (SmrAudioProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override {}

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SmrAudioProcessorEditor)
};
