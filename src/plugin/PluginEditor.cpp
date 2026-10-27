#include "PluginEditor.h"

#include <vector>

// The editor's content is laid out on a fixed 1000 x 824 canvas (sketch v3 at half size) and scaled to the window.
class SmrAudioProcessorEditor::Content : public juce::Component
{
public:
    explicit Content (SmrAudioProcessor& p) : apvts (p.parameters())
    {
        using APVTS = juce::AudioProcessorValueTreeState;
        static const char* const levelIds[6] = { "level1", "level2", "level3", "level4", "level5", "level6" };
        static const char* const lockIds[6]  = { "lock1", "lock2", "lock3", "lock4", "lock5", "lock6" };
        static const int stripX[6] = { 231, 342, 454, 566, 678, 790 };

        // Six channels: level slider and Lock button.
        for (int i = 0; i < 6; ++i)
        {
            auto& s = levels[i];
            s.setSliderStyle (juce::Slider::LinearVertical);
            s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
            s.setColour (juce::Slider::thumbColourId, juce::Colour (smrui::channelColours (i).fill));
            s.setColour (juce::Slider::trackColourId, juce::Colour (smrui::channelColours (i).edge));
            bindSlider (s, levelIds[i]);
            place (s, stripX[i], 265, 40, 224);
            addAndMakeVisible (s);

            locks[i].setButtonText ({});
            buttonAtt.push_back (std::make_unique<APVTS::ButtonAttachment> (apvts, lockIds[i], locks[i]));
            place (locks[i], stripX[i], 91, 33, 33);
            addAndMakeVisible (locks[i]);
        }

        // Freq nudge and Freq knobs, odds on the left and evens on the right.
        knob (nudge[0], "nudgeodds", 91, 111, 54);
        knob (nudge[1], "nudgeevens", 909, 111, 54);
        knob (freq[0], "freqodds", 91, 255, 44);
        knob (freq[1], "freqevens", 909, 255, 44);

        // Res (Q), Morph, Spread.
        knob (qKnob, "q", 106, 723, 72);
        knob (morph, "morph", 860, 723, 56);
        knob (spread, "spread", 941, 723, 56);

        // Switches.
        sw (oddsSwitch,   "oddsmode",      2, false, 91, 182, 52);
        sw (evensSwitch,  "evensmode",     2, false, 909, 182, 52);
        sw (slewSwitch,   "slew",          2, false, 91, 367, 52);
        sw (scaleRotSwitch, "scalerotation", 2, false, 91, 429, 52);
        sw (prePostSwitch, "prepost",      2, true,  909, 367, 52);    // parameter index 0 is Post, drawn on the right
        sw (envSwitch,    "envspeed",      3, false, 909, 429, 78);

        // Rotate: the two trigger buttons and the centre knob.
        rotCcw = std::make_unique<smrui::PulseButton> ("Rotate trig", *apvts.getParameter ("rotateccw"), 1, false, &sent[1]);
        rotCw  = std::make_unique<smrui::PulseButton> ("Rotate trig", *apvts.getParameter ("rotatecw"), 2, false, &sent[0]);
        rotCcw->setBounds (191, 547, 124, 32);
        rotCw->setBounds (685, 547, 124, 32);
        addAndMakeVisible (*rotCcw);
        addAndMakeVisible (*rotCw);
        dial = std::make_unique<smrui::RotateDial> (*apvts.getParameter ("rotatecw"), *apvts.getParameter ("rotateccw"), &sent[0], &sent[1]);
        dial->setBounds (462, 586, 76, 76);
        addAndMakeVisible (*dial);

        // Plugin-only row: Filter Mode, Filter Type, Bank, Scale down and up.
        combo (filterMode, "filtermode", 176, 757, 100, 31);
        combo (filterType, "filtertype", 282, 757, 92, 31);
        combo (bank,       "bank",       380, 757, 112, 31);
        scaleDown = std::make_unique<smrui::PulseButton> ("Scale", *apvts.getParameter ("scaledown"), 4, true, &sent[3]);
        scaleUp   = std::make_unique<smrui::PulseButton> ("Scale", *apvts.getParameter ("scaleup"), 3, true, &sent[2]);
        scaleDown->setBounds (500, 757, 64, 31);
        scaleUp->setBounds (568, 757, 64, 31);
        addAndMakeVisible (*scaleDown);
        addAndMakeVisible (*scaleUp);

        setSize (SmrAudioProcessorEditor::kBaseWidth, SmrAudioProcessorEditor::kBaseHeight);
    }

    void setStatus (const juce::String& s)
    {
        if (s != statusText)
        {
            statusText = s;
            repaint (430, 20, 540, 30);
        }
    }

    // Temporary diagnostic for the trigger test (docs/open-questions.md M6-c): pulses sent by this window and rising edges seen by the audio side.
    void setTriggerSeen (const uint32_t* seen)
    {
        juce::String t ("triggers sent/seen:   ");
        static const char* const names[4] = { "Rot CW", "Rot CCW", "Scale up", "Scale down" };
        for (int k = 0; k < 4; ++k)
            t << (k ? "   " : "") << names[k] << " " << (juce::int64) sent[k] << "/" << (juce::int64) seen[k];
        if (t != diagText)
        {
            diagText = t;
            repaint (330, 46, 640, 24);
        }
    }

    void paint (juce::Graphics& g) override
    {
        static const int stripX[6] = { 231, 342, 454, 566, 678, 790 };
        const juce::String dot = juce::String::fromUTF8 ("\xc2\xb7");

        g.fillAll (juce::Colour (0xffe9e9e5));
        g.setColour (juce::Colour (0xfff7f7f4));
        g.fillRoundedRectangle (15.0f, 15.0f, 970.0f, 794.0f, 26.0f);
        g.setColour (smrui::edge());
        g.drawRoundedRectangle (15.0f, 15.0f, 970.0f, 794.0f, 26.0f, 1.2f);

        g.setColour (smrui::ink());
        g.setFont (24.0f);
        g.drawText ("Unofficial SMR", juce::Rectangle<float> (45.0f, 24.0f, 300.0f, 32.0f), juce::Justification::centredLeft, false);

        g.setColour (smrui::softInk());
        g.setFont (13.0f);
        g.drawText (statusText, juce::Rectangle<float> (430.0f, 24.0f, 535.0f, 22.0f), juce::Justification::centredRight, false);
        g.setFont (12.0f);
        g.drawText (diagText, juce::Rectangle<float> (330.0f, 48.0f, 635.0f, 18.0f), juce::Justification::centredRight, false);
        g.setFont (13.0f);

        // Channel strips.
        for (int i = 0; i < 6; ++i)
        {
            const auto strip = juce::Rectangle<float> ((float) stripX[i] - 51.0f, 118.0f, 103.0f, 300.0f);
            g.setColour (smrui::paper());
            g.fillRoundedRectangle (strip, 12.0f);
            g.setColour (juce::Colour (0xffe2e2de));
            g.drawRoundedRectangle (strip, 12.0f, 1.0f);
            g.setColour (smrui::softInk());
            g.setFont (17.0f);
            label (g, juce::String (i + 1), (float) stripX[i], 136.0f, 40.0f);
            label (g, juce::String (i + 1), (float) stripX[i], 392.0f, 40.0f);
        }
        g.setFont (17.0f);
        g.setColour (smrui::softInk());
        g.drawText ("Lock", juce::Rectangle<float> (150.0f, 80.0f, 56.0f, 22.0f), juce::Justification::centredRight, false);

        // Labels.
        g.setFont (16.0f);
        label (g, "Freq nudge", 91.0f, 150.0f, 120.0f);
        label (g, "Freq nudge", 909.0f, 150.0f, 120.0f);
        label (g, "1" + dot + "3" + dot + "5 | 1", 91.0f, 212.0f, 120.0f);
        label (g, "2" + dot + "4" + dot + "6 | 6", 909.0f, 212.0f, 120.0f);
        label (g, "Freq", 91.0f, 285.0f, 100.0f);
        label (g, "Freq", 909.0f, 285.0f, 100.0f);
        label (g, "CV slew", 91.0f, 387.0f, 120.0f);
        label (g, "Scale rotation", 91.0f, 456.0f, 130.0f);
        label (g, "Pre " + dot + " Post", 909.0f, 387.0f, 120.0f);
        label (g, "Fast " + dot + " Trigger " + dot + " Slow", 909.0f, 456.0f, 150.0f);
        label (g, "Res (Q)", 106.0f, 770.0f, 100.0f);
        label (g, "Morph", 860.0f, 765.0f, 80.0f);
        label (g, "Spread", 941.0f, 765.0f, 80.0f);
        g.setFont (20.0f);
        g.setColour (smrui::ink());
        label (g, "Rotate", 500.0f, 549.0f, 120.0f);

        // Plugin-only controls get a dashed outline (the two Scale buttons draw their own).
        g.setColour (smrui::edge());
        for (auto* c : { &filterMode, &filterType, &bank })
            smrui::dashedRoundedRect (g, c->getBounds().toFloat().reduced (1.0f), 6.0f);
        g.setColour (smrui::softInk());
        g.setFont (13.0f);
        g.drawText ("dashed outline = plugin only", juce::Rectangle<float> (645.0f, 760.0f, 190.0f, 24.0f), juce::Justification::centredLeft, false);

        // Value of the control being dragged.
        g.setFont (14.0f);
        g.drawText (readout, juce::Rectangle<float> (176.0f, 796.0f, 640.0f, 20.0f), juce::Justification::centredLeft, false);
    }

private:
    static void place (juce::Component& c, int cx, int cy, int w, int h) { c.setBounds (cx - w / 2, cy - h / 2, w, h); }

    static void label (juce::Graphics& g, const juce::String& t, float cx, float cy, float w)
    {
        g.drawText (t, juce::Rectangle<float> (cx - w * 0.5f, cy - 11.0f, w, 22.0f), juce::Justification::centred, false);
    }

    void showValue (int paramIndex, float value)
    {
        readout = juce::String (smr_plugin::spec (paramIndex).name) + "   " + juce::String (smr_plugin::formatValue (paramIndex, value));
        repaint (176, 796, 640, 20);
    }

    void bindSlider (juce::Slider& s, const char* id)
    {
        const int idx = smr_plugin::indexOfId (id);
        sliderAtt.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, id, s));
        s.setDoubleClickReturnValue (true, (double) smr_plugin::paramDefault (idx));
        s.onValueChange = [this, &s, idx] { if (s.isMouseButtonDown()) showValue (idx, (float) s.getValue()); };
    }

    void knob (juce::Slider& s, const char* id, int cx, int cy, int size)
    {
        s.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        s.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
        s.setMouseDragSensitivity (200);
        bindSlider (s, id);
        place (s, cx, cy, size, size);
        addAndMakeVisible (s);
    }

    void sw (std::unique_ptr<smrui::SlideSwitch>& slot, const char* id, int positions, bool reversed, int cx, int cy, int w)
    {
        slot = std::make_unique<smrui::SlideSwitch> (*apvts.getParameter (id), positions, reversed);
        place (*slot, cx, cy, w, 26);
        addAndMakeVisible (*slot);
    }

    void combo (juce::ComboBox& c, const char* id, int x, int y, int w, int h)
    {
        if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (id)))
            c.addItemList (choice->choices, 1);
        comboAtt.push_back (std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, id, c));
        c.setBounds (x, y, w, h);
        addAndMakeVisible (c);
    }

    juce::AudioProcessorValueTreeState& apvts;
    juce::String statusText, readout, diagText;
    uint32_t sent[4] = {};

    juce::Slider levels[6], nudge[2], freq[2], qKnob, morph, spread;
    juce::ToggleButton locks[6];
    juce::ComboBox filterMode, filterType, bank;
    std::unique_ptr<smrui::SlideSwitch> oddsSwitch, evensSwitch, slewSwitch, scaleRotSwitch, prePostSwitch, envSwitch;
    std::unique_ptr<smrui::PulseButton> rotCcw, rotCw, scaleDown, scaleUp;
    std::unique_ptr<smrui::RotateDial> dial;

    // Declared last so they are destroyed first, while the controls they point to still exist.
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sliderAtt;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> buttonAtt;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>> comboAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Content)
};

SmrAudioProcessorEditor::SmrAudioProcessorEditor (SmrAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    content = std::make_unique<Content> (p);
    content->setLookAndFeel (&laf);
    addAndMakeVisible (*content);

    setResizeLimits (600, 494, 1600, 1318);
    getConstrainer()->setFixedAspectRatio ((double) kBaseWidth / (double) kBaseHeight);
    setResizable (true, true);
    setSize (900, 742);

    timerCallback();
    startTimer (250);
}

SmrAudioProcessorEditor::~SmrAudioProcessorEditor()
{
    stopTimer();
    content->setLookAndFeel (nullptr);
    content.reset();
}

void SmrAudioProcessorEditor::resized()
{
    content->setTransform ({});
    content->setBounds (0, 0, kBaseWidth, kBaseHeight);
    content->setTransform (juce::AffineTransform::scale ((float) getWidth() / (float) kBaseWidth));
}

void SmrAudioProcessorEditor::timerCallback()
{
    const auto s = proc.status();
    juce::String t;
    if (s.rateSupported)
        t << "Host " << juce::String (s.hostRate, 0) << " Hz, latency " << s.latencySamples << " samples, underruns " << (juce::int64) s.underruns;
    else
        t << "Host rate " << juce::String (s.hostRate, 0) << " Hz is not supported: audio passes through unchanged.";
    content->setStatus (t);
    content->setTriggerSeen (s.trigSeen);
}
