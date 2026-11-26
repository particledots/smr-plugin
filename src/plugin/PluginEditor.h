// The editor, "gold sections" look: the controls of Milestones 6 and 7, each bound to its existing host parameter, grouped in
// framed sections on a black panel, with flow lines that show how the plugin works. The displays (light ring, slider LEDs with
// clip flash, Env Out lights, input clip LED and, plugin only, In and Out meters) are read from a copy of core state published
// by the audio thread (DisplayModel.h). The control list and the decisions are in docs/open-questions.md M6-a to M6-i.
//
// Drawing is split in two so the window stays light to draw: the fixed artwork (frames, titles, labels, knob arcs, slider tracks)
// is a child component drawn once into a stored picture (again only when the window opens or is resized); the live parts (the
// lights, meters, the flow lines that follow the 135 | 1 and 246 | 6 switches, the footer texts) are drawn over it, and only the
// areas that changed are redrawn.
#pragma once

#include <memory>

#include "PluginProcessor.h"
#include "SmrUi.h"

class SmrAudioProcessorEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit SmrAudioProcessorEditor (SmrAudioProcessor&);
    ~SmrAudioProcessorEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int kBaseWidth = smrui::canvasW;
    static constexpr int kBaseHeight = smrui::canvasH;
    static constexpr int kFrameMs = 33;     // window refresh, about 30 frames a second

    // For the panel checks (tests/plugin_tests.cpp, "panel"): the stored picture of the fixed artwork, and the areas (canvas
    // units) that can be redrawn when the lights, meters and texts change.
    juce::Component& getBackdrop();
    static juce::Array<juce::Rectangle<int>> liveAreas();

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
