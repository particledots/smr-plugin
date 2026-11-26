#include "PluginEditor.h"

#include <cmath>
#include <cstring>
#include <vector>

#include "DisplayModel.h"

using namespace smrui;

namespace
{
    // The areas redrawn when something live changes (canvas units). Used by Content (what to repaint) and by liveAreas() (what the
    // checks measure), so they cannot drift apart.
    const juce::Rectangle<int> kFlowArea   { 170, 60, 660, 32 };      // the flow lines from the Odds and Evens sections to the channels
    const juce::Rectangle<int> kRingArea   { 326, 418, 348, 212 };
    const juce::Rectangle<int> kReadoutArea { 20, 772, 760, 18 };
    const juce::Rectangle<int> kStatusArea { 20, 790, 960, 16 };
    const juce::Rectangle<int> kDiagArea   { 20, 806, 960, 16 };

    juce::Rectangle<int> sliderArea (int c) { return { stripX[c] - 20, sliderTop, 40, sliderHeight }; }
    juce::Rectangle<int> lockArea (int c)   { return { stripX[c] - 20, lockY - 20, 40, 40 }; }
    juce::Rectangle<int> envArea (int c)    { return { stripX[c] - 14, envY - 14, 28, 28 }; }
    juce::Rectangle<int> clipArea()         { return { clipX - 9, clipY - 9, 18, 18 }; }
    juce::Rectangle<int> meterArea (int k)  { return { meterX[k] - 2, meterY[k] - 2, meterW + 4, meterH + 4 }; }
}

// What can change at every window update (about 30 a second): the slider LEDs, the Env Out lights, the light ring, the clip light, the
// meters and the text lines at the bottom. The flow lines (they follow the 135 | 1 and 246 | 6 switches) and the Lock buttons change
// only when something is clicked or a Lock event happens, so they are redrawn on those events and are not part of the update.
juce::Array<juce::Rectangle<int>> SmrAudioProcessorEditor::liveAreas()
{
    juce::Array<juce::Rectangle<int>> a;
    for (int c = 0; c < 6; ++c) { a.add (sliderArea (c)); a.add (envArea (c)); }
    a.add (kRingArea);
    a.add (clipArea());
    for (int k = 0; k < 4; ++k) a.add (meterArea (k));
    a.add (kReadoutArea);
    a.add (kStatusArea);
    a.add (kDiagArea);
    return a;
}

// The editor's content is laid out on a fixed 1000 x 824 canvas and scaled to the window.
class SmrAudioProcessorEditor::Content : public juce::Component
{
public:
    explicit Content (SmrAudioProcessor& p) : apvts (p.parameters())
    {
        using APVTS = juce::AudioProcessorValueTreeState;
        static const char* const levelIds[6] = { "level1", "level2", "level3", "level4", "level5", "level6" };
        static const char* const lockIds[6]  = { "lock1", "lock2", "lock3", "lock4", "lock5", "lock6" };
        static const char* const lockHoldIds[6] = { "lockhold1", "lockhold2", "lockhold3", "lockhold4", "lockhold5", "lockhold6" };

        // The fixed artwork, drawn once into a stored picture (behind everything else).
        backdrop.setBounds (0, 0, canvasW, canvasH);
        backdrop.setInterceptsMouseClicks (false, false);
        backdrop.setBufferedToImage (true);
        addAndMakeVisible (backdrop);

        // Six channels: level slider and Lock button.
        for (int i = 0; i < 6; ++i)
        {
            auto& s = levels[i];
            s.setSliderStyle (juce::Slider::LinearVertical);
            s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
            bindSlider (s, levelIds[i]);
            s.setBounds (sliderArea (i));
            addAndMakeVisible (s);

            locks[i] = std::make_unique<smrui::LockButton> (*apvts.getParameter (lockHoldIds[i]));
            locks[i]->setButtonText ({});
            buttonAtt.push_back (std::make_unique<APVTS::ButtonAttachment> (apvts, lockIds[i], *locks[i]));   // displays the Lock parameter
            locks[i]->setBounds (lockArea (i));
            addAndMakeVisible (*locks[i]);
        }
        // Where the slider's travel starts and ends (the gold track and its ticks are drawn there).
        trackTop = (float) sliderTop + levels[0].getPositionOfValue (levels[0].getMaximum());
        trackBottom = (float) sliderTop + levels[0].getPositionOfValue (levels[0].getMinimum());

        // Freq nudge and Freq knobs, odds on the left and evens on the right.
        knob (nudge[0], "nudgeodds", oddX, 112);
        knob (nudge[1], "nudgeevens", evenX, 112);
        knob (freq[0], "freqodds", oddX, 254);
        knob (freq[1], "freqevens", evenX, 254);

        // The 135 | 1 and 246 | 6 switches (they decide which channels the Freq jack, Freq Nudge knob and Lock jack act on).
        sw (oddsSwitch,  "oddsmode",  2, false, { "1 3 5", "1 ONLY" }, SmrSwitchLook::List, oddX - 40, 176, 80);
        sw (evensSwitch, "evensmode", 2, false, { "2 4 6", "6 ONLY" }, SmrSwitchLook::List, evenX - 40, 176, 80);

        // The two Lock jacks, as trigger buttons under the Freq knobs (the firmware's params.c process_lock_jacks flips the lock of
        // channels 1 3 5 or 2 4 6 according to the 135 | 1 and 246 | 6 switches).
        lockOdds  = std::make_unique<smrui::PulseButton> ("LOCK ODDS", *apvts.getParameter ("lockjack135"), 0, false, linkFor (p, 4));
        lockEvens = std::make_unique<smrui::PulseButton> ("LOCK EVENS", *apvts.getParameter ("lockjack246"), 0, false, linkFor (p, 5));
        lockOdds->setBounds (oddX - 58, 336, 116, 24);
        lockEvens->setBounds (evenX - 58, 336, 116, 24);
        addAndMakeVisible (*lockOdds);
        addAndMakeVisible (*lockEvens);

        // Input and Level CV sections: Noise (plugin only, stand-in noise, Milestone 7 step 6) and CV slew.
        sw (noiseSwitch, "noise", 2, false, { juce::String::fromUTF8 ("NOISE \xc2\xb7 STAND-IN") }, SmrSwitchLook::OptionDashed, 22, 510, 156);
        sw (slewSwitch,  "slew",  2, false, { "CV SLEW" }, SmrSwitchLook::Option, 22, 590, 140);

        // Env Out section: Pre | Post (parameter index 0 is Post, drawn on the lower row) and the envelope speed (the fourth
        // position, 1V/oct, is plugin only: Milestone 7, step 5).
        sw (prePostSwitch, "prepost",  2, true,  { "POST", "PRE" }, SmrSwitchLook::List, 832, 428, 56);
        sw (envSwitch,     "envspeed", 4, false, { "FAST", "TRIG", "SLOW", "1V/OCT" }, SmrSwitchLook::ListDashedLast, 896, 428, 88);

        // Rotation section: the ROTATE encoder press (plugin only: the hardware has the encoder). With a Lock held it forbids, or allows
        // again, the note that channel is on; all six Locks held clear every forbidden note (params.c process_freq_blocks). It has no
        // other effect in this plugin.
        rotPress = std::make_unique<smrui::PulseButton> ("ROTATE PRESS", *apvts.getParameter ("rotatepress"), 0, true, linkFor (p, 6));
        rotPress->setBounds (200, 498, 128, 24);
        addAndMakeVisible (*rotPress);
        rotCcw = std::make_unique<smrui::PulseButton> ("ROTATE TRIG", *apvts.getParameter ("rotateccw"), 1, false, linkFor (p, 1));
        rotCw  = std::make_unique<smrui::PulseButton> ("ROTATE TRIG", *apvts.getParameter ("rotatecw"), 2, false, linkFor (p, 0));
        rotCcw->setBounds (200, 450, 140, 24);
        rotCw->setBounds (660, 450, 140, 24);
        addAndMakeVisible (*rotCcw);
        addAndMakeVisible (*rotCw);
        dial = std::make_unique<smrui::RotateDial> (*apvts.getParameter ("rotatecw"), *apvts.getParameter ("rotateccw"), linkFor (p, 0), linkFor (p, 1));
        dial->setBounds ((int) ringCx - 46, (int) ringCy - 46, 92, 92);
        addAndMakeVisible (*dial);
        sw (scaleRotSwitch, "scalerotation", 2, false, { "SCALE ROTATION" }, SmrSwitchLook::Option, 666, 496, 134);

        // Res (Q), Spread and Morph.
        knob (qKnob, "q", oddX, 700);
        knob (spread, "spread", 864, 700);
        knob (morph, "morph", 945, 700);

        // Filter, Scale and Bank (plugin only): Filter Mode, Filter Type, Bank, Scale down and up.
        filterMode = std::make_unique<smrui::ChoicePill> (*apvts.getParameter ("filtermode"), juce::StringArray { "TWO-PASS", "ONE-PASS" });
        filterType = std::make_unique<smrui::ChoicePill> (*apvts.getParameter ("filtertype"), juce::StringArray { "MAXQ", "BPRE" });
        filterMode->setBounds (262, 676, 212, 24);
        filterType->setBounds (262, 716, 212, 24);
        addAndMakeVisible (*filterMode);
        addAndMakeVisible (*filterType);
        combo (bank, "bank", 574, 676, 224, 24);
        scaleDown = std::make_unique<smrui::PulseButton> ("DOWN", *apvts.getParameter ("scaledown"), 4, true, linkFor (p, 3));
        scaleUp   = std::make_unique<smrui::PulseButton> ("UP", *apvts.getParameter ("scaleup"), 3, true, linkFor (p, 2));
        scaleDown->setBounds (574, 716, 108, 24);
        scaleUp->setBounds (690, 716, 108, 24);
        addAndMakeVisible (*scaleDown);
        addAndMakeVisible (*scaleUp);

        setSize (SmrAudioProcessorEditor::kBaseWidth, SmrAudioProcessorEditor::kBaseHeight);
    }

    juce::Component& getBackdrop() { return backdrop; }

    void setStatus (const juce::String& s)
    {
        if (s != statusText)
        {
            statusText = s;
            repaint (kStatusArea);
        }
    }

    // Temporary diagnostic for the trigger test (docs/open-questions.md M6-c): pulses sent by this window and rising edges seen by the audio side.
    void setTriggerSeen (const uint32_t* seen)
    {
        juce::String t ("triggers sent/seen:   ");
        static const char* const names[7] = { "Rot CW", "Rot CCW", "Scale up", "Scale down", "Lock 135", "Lock 246", "Rot press" };
        for (int k = 0; k < 7; ++k)
            t << (k ? "   " : "") << names[k] << " " << (juce::int64) sent[k] << "/" << (juce::int64) seen[k];
        if (t != diagText)
        {
            diagText = t;
            repaint (kDiagArea);
        }
    }

    // Milestone 6b: take the latest copy of core state from the audio side and redraw what changed. Read only: nothing goes back.
    // dtMs: time since the previous call.
    void updateDisplay (smr_plugin::DisplayHub& hub, float dtMs)
    {
        using namespace smr_plugin;

        DisplayData d = last;
        if (! hub.read (d)) d = last;          // no consistent copy this time: keep the previous frame
        last = d;

        flashAccMs += (double) dtMs;
        while (flashAccMs >= kRingFlashMs)
        {
            flashAccMs -= kRingFlashMs;
            flash = ! flash;
            // One redraw of the firmware (update_LED_ring): calculate_envout_leds runs once. Its COARSE and FINE TUNING branches are
            // evaluated here, at that rate (the fine tune blink counts these calls); between two redraws the lights keep what it gave.
            // Milestone 7 step 3b. The play and 1V/oct branches stay live below (Milestone 6b).
            if (d.valid != 0) envFwActive = envOutLedsFw (d, envFwState, envFw);
        }
        if (d.valid == 0) envFwActive = false;

        // Lock buttons (leds.c update_lock_leds, play branch): one update every QLOCK_FLASH_SPEED + 1 loop passes; a Q-locked channel
        // is drawn inverted once in 16 updates, and while a clear of the transpositions is staged a transposed channel flashes
        // (lockLedUpdate, DisplayModel.h). The rate rests on the M2-c placeholder.
        lockAccMs += (double) dtMs;
        while (lockAccMs >= kLockLedUpdateMs) { lockAccMs -= kLockLedUpdateMs; if (d.valid != 0) lockLedUpdate (d, lockLed); }
        for (int c = 0; c < 6; ++c)
        {
            const bool q = d.valid != 0 && lockLed.inverted[c];
            if (locks[c]->inverted != q || locks[c]->flashPhase != flash)
            {
                locks[c]->inverted = q;
                locks[c]->flashPhase = flash;
                locks[c]->repaint();
            }
        }

        // The flow lines from the Odds and Evens sections follow the 135 | 1 and 246 | 6 switches.
        const int oddsNow = apvts.getRawParameterValue ("oddsmode")->load() >= 0.5f ? 1 : 0;
        const int evensNow = apvts.getRawParameterValue ("evensmode")->load() >= 0.5f ? 1 : 0;
        if (oddsNow != oddsMode || evensNow != evensMode)
        {
            oddsMode = oddsNow;
            evensMode = evensNow;
            repaint (kFlowArea);
        }

        uint16_t newRing[kRingLeds][3] = {};
        uint16_t newEnv[6][3] = {};
        if (d.valid != 0)
        {
            ringLeds (d, flash, newRing);
            envOutLeds (d, newEnv);
            if (envFwActive) std::memcpy (newEnv, envFw, sizeof newEnv);   // calculate_envout_leds took its COARSE or FINE TUNING branch
        }
        if (std::memcmp (newRing, ringLed, sizeof ringLed) != 0)
        {
            std::memcpy (ringLed, newRing, sizeof ringLed);
            repaint (kRingArea);
        }
        for (int c = 0; c < 6; ++c)
            if (std::memcmp (newEnv[c], envLed[c], sizeof envLed[c]) != 0)
            {
                std::memcpy (envLed[c], newEnv[c], sizeof envLed[c]);
                repaint (envArea (c));
            }

        // Slider LEDs and the input clip LED. Clip events collected since the last frame show for this frame.
        const uint32_t bits = hub.takeClipBits();
        for (int i = 0; i < 6; ++i)
        {
            const float duty = d.valid != 0 ? sliderLedDuty (d.level[i]) : 0.0f;
            const bool clip = (bits & (1u << i)) != 0;
            if (duty != sliderDuty[i] || clip != sliderClip[i])
            {
                sliderDuty[i] = duty;
                sliderClip[i] = clip;
                levels[i].getProperties().set ("led", duty);
                levels[i].getProperties().set ("ledclip", clip);
                levels[i].repaint();
            }
        }
        const bool inClip = (bits & kInputClipBit) != 0;
        if (inClip != inClipOn)
        {
            inClipOn = inClip;
            repaint (clipArea());
        }

        // Meters (plugin only, not in the firmware): peak of each host buffer, fast rise, slow fall, peak hold.
        for (int k = 0; k < 4; ++k)
        {
            const float peak = hub.takePeak (k);
            const float db = peak > 1.0e-5f ? 20.0f * std::log10 (peak) : -100.0f;
            Meter& m = meters[k];
            m.shown = juce::jmax (db, m.shown - kMeterFallDbPerSec * dtMs / 1000.0f);
            if (db >= m.hold) { m.hold = db; m.holdAgeMs = 0.0f; }
            else
            {
                m.holdAgeMs += dtMs;
                if (m.holdAgeMs > kMeterHoldMs) m.hold = juce::jmax (m.shown, m.hold - kMeterHoldFallDbPerSec * dtMs / 1000.0f);
            }
            const int fillPx = meterPixels (m.shown), holdPx = meterPixels (m.hold);
            const bool over = m.shown > 0.0f;
            if (fillPx != m.fillPx || holdPx != m.holdPx || over != m.over)
            {
                m.fillPx = fillPx; m.holdPx = holdPx; m.over = over;
                repaint (meterArea (k));
            }
        }
    }

    // ------------------------------------------------------------------------------------------ live drawing
    void paintOverChildren (juce::Graphics& g) override
    {
        if (g.getClipBounds().intersects (kFlowArea))
            paintFlowLines (g);

        // Milestone 6b lights. Brightness is the firmware's 12 bit duty value (1023 is its clamp) drawn on a dark lens: 0 is the lens,
        // 1023 is the full colour. Colours are led_ring.c DEFAULT_COLOR_CH[0]. Ring: index i sits i x 18 degrees clockwise from the
        // top (docs/open-questions.md M6-i); the ellipse is the sketch's.
        // The dark lenses of the unlit lights are fixed artwork (paintBackdrop); only a lit light is drawn here, over its lens.
        const auto clip = g.getClipBounds();
        if (clip.intersects (kRingArea))
            for (int i = 0; i < smr_plugin::kRingLeds; ++i)
            {
                if (ringLed[i][0] == 0 && ringLed[i][1] == 0 && ringLed[i][2] == 0) continue;
                const float a = (float) i * juce::MathConstants<float>::twoPi / (float) smr_plugin::kRingLeds;
                drawLight (g, ringCx + ringRx * std::sin (a), ringCy - ringRy * std::cos (a), ledColour (ringLed[i], 0.5f), true, 7.0f);
            }
        for (int c = 0; c < 6; ++c)
            if (clip.intersects (envArea (c)) && (envLed[c][0] > 0 || envLed[c][1] > 0 || envLed[c][2] > 0))
                drawLight (g, (float) stripX[c], (float) envY, ledColour (envLed[c]), true, 6.5f);

        // Input clip LED (one LED between the IN jacks, panel photo). Colour from the photo; the firmware only switches a pin.
        if (inClipOn)
            drawLamp (g, (float) clipX, (float) clipY, true, 3.6f, clipLamp());

        // In and Out meters (plugin only; their dashed outlines are fixed artwork).
        for (int k = 0; k < 4; ++k)
        {
            const Meter& m = meters[k];
            if (! clip.intersects (meterArea (k))) continue;
            const float x = (float) meterX[k], y = (float) meterY[k], h = (float) meterH;
            if (m.fillPx > 0)
            {
                g.setColour (m.over ? clipLamp() : text().withAlpha (0.55f));
                g.fillRoundedRectangle (x + 1.0f, y + 1.0f, (float) m.fillPx, h - 2.0f, 1.5f);
            }
            if (m.holdPx > 0)
            {
                g.setColour (gold());
                g.fillRect (x + 1.0f + (float) m.holdPx - 1.0f, y, 1.6f, h);
            }
        }

        // Value of the control being dragged, then the status lines.
        if (readout.isNotEmpty() && clip.intersects (kReadoutArea))
            textAt (g, readout, 26.0f, 786.0f, muted(), 12.0f, juce::Justification::centredLeft, 0.0f, 770.0f);
        if (statusText.isNotEmpty() && clip.intersects (kStatusArea))
            textAt (g, statusText, 26.0f, 803.0f, muted(), 11.0f, juce::Justification::centredLeft, 0.0f, 940.0f);
        if (diagText.isNotEmpty() && clip.intersects (kDiagArea))
            textAt (g, diagText, 26.0f, 817.0f, faint(), 10.0f, juce::Justification::centredLeft, 0.0f, 940.0f);
    }

private:
    enum class SmrSwitchLook { List, ListDashedLast, Option, OptionDashed };

    // ------------------------------------------------------------------------------------------ fixed artwork
    struct Backdrop : public juce::Component
    {
        explicit Backdrop (Content& c) : owner (c) {}
        void paint (juce::Graphics& g) override { owner.paintBackdrop (g); }
        Content& owner;
    };

    void paintBackdrop (juce::Graphics& g)
    {
        g.setColour (panel());
        g.fillRoundedRectangle (backdrop.getLocalBounds().toFloat(), 12.0f);

        // Header: the title between two ornament lines.
        {
            const auto tf = titleFont();
            const float w = widthOf ("UNOFFICIAL SMR", tf);
            logFit ("UNOFFICIAL SMR", w, titleRight - titleLeft);
            g.setColour (goldText());
            g.setFont (tf);
            g.drawText ("UNOFFICIAL SMR", juce::Rectangle<float> (500.0f - 350.0f, titleBaseline - tf.getAscent(), 700.0f, tf.getHeight()),
                        juce::Justification::centred, false);
            g.setColour (gold());
            g.fillRect (160.0f, 28.6f, ornamentLeftEnd - 160.0f, 0.8f);
            g.fillRect (ornamentRightStart, 28.6f, 840.0f - ornamentRightStart, 0.8f);
            g.fillEllipse (156.4f, 27.2f, 3.6f, 3.6f);
            g.fillEllipse (840.0f, 27.2f, 3.6f, 3.6f);
            textAt (g, "PARTICLEDOTS", 26.0f, 33.0f, muted(), 12.0f, juce::Justification::centredLeft, 2.5f, 125.0f);
        }

        // Section frames, each with its title set into the top line.
        struct Frame { float x, y, w, h; const char* title; bool centred; };
        const Frame frames[] = {
            { 14.5f, 58.5f, 162, 334, "ODDS  \xc2\xb7  1 3 5", false },  { 186.5f, 58.5f, 627, 334, "CHANNELS", true },  { 823.5f, 58.5f, 162, 334, "EVENS  \xc2\xb7  2 4 6", false },
            { 14.5f, 402.5f, 162, 144, "INPUT", false },                 { 14.5f, 556.5f, 162, 84, "LEVEL CV", false },
            { 186.5f, 402.5f, 627, 238, "ROTATION", true },
            { 823.5f, 402.5f, 162, 144, "ENV OUT", false },              { 823.5f, 556.5f, 162, 84, "OUTPUT", false },
            { 14.5f, 650.5f, 162, 118, "RESONANCE", false },             { 186.5f, 650.5f, 302, 118, "FILTER", false },
            { 498.5f, 650.5f, 315, 118, "SCALE \xc2\xb7 BANK", false },  { 823.5f, 650.5f, 162, 118, "SPREAD \xc2\xb7 MORPH", false } };
        for (const auto& f : frames)
        {
            g.setColour (gold());
            g.drawRoundedRectangle (f.x, f.y, f.w, f.h, 10.0f, 0.9f);
            const juce::String t = juce::String::fromUTF8 (f.title);
            const float tw = widthOf (t, font (frameTitleSize, false, frameTitleSpacing));
            logFit (t, tw, f.w - 32.0f);
            const float tx = f.centred ? f.x + f.w * 0.5f - tw * 0.5f : f.x + 18.0f;
            g.setColour (panel());
            g.fillRect (tx - 6.0f, f.y - 6.0f, tw + 12.0f, 12.0f);
            textAt (g, t, tx, f.y + 4.3f, goldText(), frameTitleSize, juce::Justification::centredLeft, frameTitleSpacing);
        }

        // The channels: the slider tracks with their ticks, numbers, and the row titles.
        for (int i = 0; i < 6; ++i)
        {
            const float x = (float) stripX[i];
            g.setColour (gold());
            g.drawRoundedRectangle (x - 3.0f, trackTop, 6.0f, trackBottom - trackTop, 3.0f, 0.8f);
            g.setColour (gold().withAlpha (0.6f));
            for (int k = 0; k <= 10; ++k)
            {
                const float yy = trackTop + (trackBottom - trackTop) * (float) k / 10.0f;
                g.fillRect (x - 12.0f, yy - 0.35f, 5.0f, 0.7f);
                g.fillRect (x + 7.0f, yy - 0.35f, 5.0f, 0.7f);
            }
            textAt (g, juce::String (i + 1), x, 326.0f, text(), 13.0f);
        }
        textAt (g, "LOCK", 198.0f, 104.3f, goldText(), rowSize, juce::Justification::centredLeft, rowSpacing, 62.0f);
        textAt (g, "LEVEL", 198.0f, 218.0f, goldText(), rowSize, juce::Justification::centredLeft, rowSpacing, 58.0f);
        textAt (g, "ENV OUT", 198.0f, 364.3f, goldText(), rowSize, juce::Justification::centredLeft, rowSpacing, 66.0f);

        // The knobs' range arcs (7:30 round to 4:30) with a dot at each end, and their names.
        struct K { float x, y; const char* name; float room; };
        const K ks[] = { { (float) oddX, 112, "FREQ NUDGE", 150 }, { (float) evenX, 112, "FREQ NUDGE", 150 },
                         { (float) oddX, 254, "FREQ", 150 },       { (float) evenX, 254, "FREQ", 150 },
                         { (float) oddX, 700, "RES (Q)", 150 },    { 864, 700, "SPREAD", 76 }, { 945, 700, "MORPH", 76 } };
        const float R = (float) knobBox * 0.5f;
        for (const auto& k : ks)
        {
            juce::Path arc;
            arc.addCentredArc (k.x, k.y, R, R, 0.0f, juce::degreesToRadians (-135.0f), juce::degreesToRadians (135.0f), true);
            g.setColour (gold());
            g.strokePath (arc, juce::PathStrokeType (0.9f));
            const float d = R * 0.70710678f;
            dotAt (g, k.x - d, k.y + d, 1.8f);
            dotAt (g, k.x + d, k.y + d, 1.8f);
            textAt (g, k.name, k.x, k.y + 52.0f, text(), capsSize, juce::Justification::centred, capsSpacing, k.room);
        }

        // The unlit lenses of the ring and the Env Out lights, the clip light when off, and the meters' dashed outlines. The live drawing
        // adds only what is lit or filled on top (paintOverChildren).
        for (int i = 0; i < smr_plugin::kRingLeds; ++i)
        {
            const float a = (float) i * juce::MathConstants<float>::twoPi / (float) smr_plugin::kRingLeds;
            drawLight (g, ringCx + ringRx * std::sin (a), ringCy - ringRy * std::cos (a), juce::Colours::black, false, 7.0f);
        }
        for (int c = 0; c < 6; ++c)
            drawLight (g, (float) stripX[c], (float) envY, juce::Colours::black, false, 6.5f);
        drawLamp (g, (float) clipX, (float) clipY, false, 3.6f, clipLamp());
        for (int k = 0; k < 4; ++k)
            dashedRoundedRect (g, juce::Rectangle<float> ((float) meterX[k], (float) meterY[k], (float) meterW, (float) meterH), 2.0f, 0.7f);

        // Input and Output: the meters' names. Rotation: the knob's name.
        const char* const meterName[4] = { "ODD", "EVEN", "L", "R" };
        for (int k = 0; k < 4; ++k)
            textAt (g, meterName[k], (float) meterX[k] - 8.0f, (float) meterY[k] + 6.5f, muted(), 11.0f, juce::Justification::centredRight, 1.2f, 34.0f);
        textAt (g, "CLIP", (float) clipX + 11.0f, (float) clipY + 4.3f, soft(), labelSize, juce::Justification::centredLeft, 1.5f, 100.0f);
        textAt (g, "ROTATE", ringCx, ringCy + 62.0f, text(), capsSize, juce::Justification::centred, capsSpacing, 100.0f);

        // Filter and Scale rows.
        textAt (g, "MODE", 202.0f, 690.3f, goldText(), rowSize, juce::Justification::centredLeft, rowSpacing, 50.0f);
        textAt (g, "TYPE", 202.0f, 730.3f, goldText(), rowSize, juce::Justification::centredLeft, rowSpacing, 50.0f);
        textAt (g, "BANK", 514.0f, 690.3f, goldText(), rowSize, juce::Justification::centredLeft, rowSpacing, 52.0f);
        textAt (g, "SCALE", 514.0f, 730.3f, goldText(), rowSize, juce::Justification::centredLeft, rowSpacing, 52.0f);

        // Footer: the key to the dashed outlines.
        dashedRoundedRect (g, { 808.0f, 777.0f, 22.0f, 12.0f }, 4.0f);
        textAt (g, "PLUGIN ONLY", 838.0f, 787.0f, muted(), 11.0f, juce::Justification::centredLeft, 1.6f, 130.0f);
    }

    // The flow lines from the Odds section to the channels it acts on, and from the Evens section to its channels. The 135 | 1
    // and 246 | 6 switches decide which channels the Freq jack, Freq Nudge knob and Lock jack control (manual, Controls and Jacks),
    // so the line ends follow them. The Evens line hops over the Odds drops it crosses. A third line shows that the two Env Out
    // switches (Pre | Post, speed) feed the Env Out lights.
    void paintFlowLines (juce::Graphics& g)
    {
        constexpr float lo = 70.0f, le = 78.0f, drop = 85.0f;
        const float oddDrops[3] = { (float) stripX[0], (float) stripX[2], (float) stripX[4] };
        const float evenDrops[3] = { (float) stripX[1], (float) stripX[3], (float) stripX[5] };
        const int nOdd = oddsMode == 0 ? 3 : 1;                 // 1 | : channel 1 only
        const int firstEven = evensMode == 0 ? 0 : 2;           // | 6: channel 6 only
        const float oddEnd = oddDrops[nOdd - 1], evenEnd = evenDrops[firstEven];

        strokeGold (g, run ({ { 176.5f, lo }, { oddEnd, lo } }));
        for (int i = 0; i < nOdd; ++i)
        {
            strokeGold (g, run ({ { oddDrops[i], lo }, { oddDrops[i], drop } }));
            dotAt (g, oddDrops[i], drop + 1.0f);
        }
        node (g, 176.5f, lo);

        juce::Path evens;
        evens.startNewSubPath (823.5f, le);
        for (int i = nOdd - 1; i >= 0; --i)                     // hops over the Odds drops that lie between the ends, from the right
        {
            const float hx = oddDrops[i];
            if (hx <= evenEnd) continue;
            evens.lineTo (hx + 7.0f, le);
            evens.quadraticTo (hx, le - 9.0f, hx - 7.0f, le);
        }
        evens.lineTo (evenEnd, le);
        strokeGold (g, evens);
        for (int i = firstEven; i < 3; ++i)
        {
            strokeGold (g, run ({ { evenDrops[i], le }, { evenDrops[i], drop } }));
            dotAt (g, evenDrops[i], drop + 1.0f);
        }
        node (g, 823.5f, le);

        // Env Out switches to the Env Out lights.
        strokeGold (g, run ({ { 823.5f, 470.0f }, { 818.5f, 470.0f }, { 818.5f, (float) envY }, { 813.5f, (float) envY } }));
        node (g, 823.5f, 470.0f);
        node (g, 813.5f, (float) envY);
    }

    // ------------------------------------------------------------------------------------------ the lights
    // Milestone 6b geometry on the canvas. Plugin-only meter ballistics (not from the firmware): -60 dBFS at the left, 0 dBFS at the right.
    static constexpr float kMeterFallDbPerSec = 24.0f, kMeterHoldMs = 1000.0f, kMeterHoldFallDbPerSec = 12.0f;
    static int meterPixels (float db) { return juce::roundToInt (juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f) * (float) (meterW - 2)); }

    // A lens that is dark at 0 and shows the full colour at 1023 (the firmware's clamp; the driver is 12 bit, docs/m6b-led-notes.md).
    // gamma 1.0 is linear (Env out lights). The ring uses 0.5 so the firmware's 5 % floor (value 38 of 1023) shows as a dim dot
    // rather than vanishing into the lens (Bryn, 2026-10-27: on the real SMR a channel at the bottom is dimmed, not out).
    // kLedFull: the brightest firmware colour component is 770 (DEFAULT_COLOR_CH[0] ch2), so 770 is drawn as the full colour; sums
    // above it clamp. Plugin display choice, not firmware.
    static constexpr float kLedFull = 770.0f;
    static juce::Colour ledColour (const uint16_t* rgb, float gamma = 1.0f)
    {
        constexpr float base = 0.17f;
        auto ch = [gamma] (uint16_t v) { return juce::jmin (1.0f, base + std::pow ((float) v / kLedFull, gamma) * (1.0f - base)); };
        return juce::Colour::fromFloatRGBA (ch (rgb[0]), ch (rgb[1]), ch (rgb[2]), 1.0f);
    }

    struct Meter { float shown = -100.0f, hold = -100.0f, holdAgeMs = 0.0f; int fillPx = 0, holdPx = 0; bool over = false; };

    Backdrop backdrop { *this };
    smr_plugin::DisplayData last;
    uint16_t ringLed[smr_plugin::kRingLeds][3] = {};
    uint16_t envLed[6][3] = {};
    float sliderDuty[6] = {};
    bool sliderClip[6] = {};
    bool inClipOn = false;
    bool flash = false;
    int oddsMode = 0, evensMode = 0;
    float trackTop = 128.0f, trackBottom = 300.0f;
    double flashAccMs = 0.0;
    double lockAccMs = 0.0;
    smr_plugin::LockLedState lockLed;
    smr_plugin::EnvOutLedState envFwState;
    uint16_t envFw[6][3] = {};
    bool envFwActive = false;
    Meter meters[4];

    // Trigger k: 0 Rotate CW, 1 Rotate CCW, 2 Scale Up, 3 Scale Down, 4 Lock jack 1 3 5, 5 Lock jack 2 4 6, 6 Rotate press. The processor outlives the editor.
    smrui::TriggerLink linkFor (SmrAudioProcessor& p, int k)
    {
        smrui::TriggerLink l;
        l.rising  = [&p, k] { return p.triggerRisingCount (k); };
        l.falling = [&p, k] { return p.triggerFallingCount (k); };
        l.sent = &sent[k];
        return l;
    }

    void showValue (int paramIndex, float value)
    {
        readout = juce::String (smr_plugin::spec (paramIndex).name) + "   " + juce::String (smr_plugin::formatValue (paramIndex, value));
        repaint (kReadoutArea);
    }

    void bindSlider (juce::Slider& s, const char* id)
    {
        const int idx = smr_plugin::indexOfId (id);
        sliderAtt.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, id, s));
        s.setDoubleClickReturnValue (true, (double) smr_plugin::paramDefault (idx));
        s.onValueChange = [this, &s, idx] { if (s.isMouseButtonDown()) showValue (idx, (float) s.getValue()); };
    }

    void knob (juce::Slider& s, const char* id, int cx, int cy)
    {
        s.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        s.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
        s.setMouseDragSensitivity (200);
        bindSlider (s, id);
        s.setBounds (cx - knobBox / 2, cy - knobBox / 2, knobBox, knobBox);
        addAndMakeVisible (s);
    }

    void sw (std::unique_ptr<smrui::SlideSwitch>& slot, const char* id, int positions, bool reversed, juce::StringArray labels,
             SmrSwitchLook look, int x, int y, int w)
    {
        const bool option = look == SmrSwitchLook::Option || look == SmrSwitchLook::OptionDashed;
        const int dashed = look == SmrSwitchLook::OptionDashed ? 0 : (look == SmrSwitchLook::ListDashedLast ? positions - 1 : -1);
        slot = std::make_unique<smrui::SlideSwitch> (*apvts.getParameter (id), positions, reversed, std::move (labels),
                                                     option ? smrui::SlideSwitch::Look::Option : smrui::SlideSwitch::Look::List, dashed);
        slot->setBounds (x, y, w, smrui::SlideSwitch::heightFor (option ? smrui::SlideSwitch::Look::Option : smrui::SlideSwitch::Look::List, positions));
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
    uint32_t sent[7] = {};

    juce::Slider levels[6], nudge[2], freq[2], qKnob, morph, spread;
    std::unique_ptr<smrui::LockButton> locks[6];
    juce::ComboBox bank;
    std::unique_ptr<smrui::ChoicePill> filterMode, filterType;
    std::unique_ptr<smrui::SlideSwitch> oddsSwitch, evensSwitch, slewSwitch, scaleRotSwitch, noiseSwitch, prePostSwitch, envSwitch;
    std::unique_ptr<smrui::PulseButton> rotCcw, rotCw, scaleDown, scaleUp, lockOdds, lockEvens, rotPress;
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
    startTimer (kFrameMs);
}

SmrAudioProcessorEditor::~SmrAudioProcessorEditor()
{
    stopTimer();
    content->setLookAndFeel (nullptr);
    content.reset();
}

juce::Component& SmrAudioProcessorEditor::getBackdrop() { return content->getBackdrop(); }

void SmrAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (smrui::panel());
}

void SmrAudioProcessorEditor::resized()
{
    content->setTransform ({});
    content->setBounds (0, 0, kBaseWidth, kBaseHeight);
    content->setTransform (juce::AffineTransform::scale ((float) getWidth() / (float) kBaseWidth));
}

void SmrAudioProcessorEditor::timerCallback()
{
    // Lights and meters every frame (Milestone 6b), the status lines about four times a second.
    const double now = juce::Time::getMillisecondCounterHiRes();
    const float dtMs = lastTickMs > 0.0 ? (float) juce::jlimit (1.0, 250.0, now - lastTickMs) : (float) kFrameMs;
    lastTickMs = now;
    content->updateDisplay (proc.displayHub(), dtMs);
    if (tickCount++ % 8 != 0)
        return;

    const auto s = proc.status();
    juce::String t;
    if (s.rateSupported)
        t << "Host " << juce::String (s.hostRate, 0) << " Hz, latency " << s.latencySamples << " samples, underruns " << (juce::int64) s.underruns;
    else
        t << "Host rate " << juce::String (s.hostRate, 0) << " Hz is not supported: audio passes through unchanged.";
    content->setStatus (t);
    content->setTriggerSeen (s.trigSeen);
}
