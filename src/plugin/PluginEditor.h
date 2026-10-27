// Milestone 6a editor: the layout of sketch v3, every control bound to its existing host parameter. No LED ring, slider
// LEDs, meters or Env Out indicators yet (Milestone 6b). The control list and the decisions are in docs/open-questions.md M6-a to M6-e.
#pragma once

#include <memory>

#include "PluginProcessor.h"
#include "SmrUi.h"

class SmrAudioProcessorEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit SmrAudioProcessorEditor (SmrAudioProcessor&);
    ~SmrAudioProcessorEditor() override;
    void resized() override;

    static constexpr int kBaseWidth = 1000;
    static constexpr int kBaseHeight = 824;

private:
    class Content;

    void timerCallback() override;

    SmrAudioProcessor& proc;
    smrui::SmrLookAndFeel laf;
    std::unique_ptr<Content> content;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SmrAudioProcessorEditor)
};
