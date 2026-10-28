// Milestone 6a editor: the layout of sketch v3, every control bound to its existing host parameter. Milestone 6b adds the
// displays: light ring, slider LEDs with clip flash, Env Out lights, input clip LED and (plugin only) In and Out meters, all read
// from a copy of core state published by the audio thread (DisplayModel.h). The control list and the decisions are in
// docs/open-questions.md M6-a to M6-i.
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
    static constexpr int kFrameMs = 33;     // window refresh, about 30 frames a second

private:
    class Content;

    void timerCallback() override;

    SmrAudioProcessor& proc;
    smrui::SmrLookAndFeel laf;
    std::unique_ptr<Content> content;
    double lastTickMs = 0.0;
    int tickCount = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SmrAudioProcessorEditor)
};
