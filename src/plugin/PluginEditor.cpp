#include "PluginEditor.h"

#include <cmath>
#include <cstring>
#include <vector>

#include "DisplayModel.h"

// The editor's content is laid out on a fixed 1000 x 824 canvas (sketch v3 at half size) and scaled to the window.
class SmrAudioProcessorEditor::Content : public juce::Component
{
public:
    explicit Content (SmrAudioProcessor& p) : apvts (p.parameters())
    {
        using APVTS = juce::AudioProcessorValueTreeState;
        static const char* const levelIds[6] = { "level1", "level2", "level3", "level4", "level5", "level6" };
        static const char* const lockIds[6]  = { "lock1", "lock2", "lock3", "lock4", "lock5", "lock6" };
        static const char* const lockHoldIds[6] = { "lockhold1", "lockhold2", "lockhold3", "lockhold4", "lockhold5", "lockhold6" };
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

            locks[i] = std::make_unique<smrui::LockButton> (*apvts.getParameter (lockHoldIds[i]));
            locks[i]->setButtonText ({});
            buttonAtt.push_back (std::make_unique<APVTS::ButtonAttachment> (apvts, lockIds[i], *locks[i]));   // displays the Lock parameter
            place (*locks[i], stripX[i], 91, 33, 33);
            addAndMakeVisible (*locks[i]);
        }

        // Freq nudge and Freq knobs, odds on the left and evens on the right.
        knob (nudge[0], "nudgeodds", 91, 111, 54);
        knob (nudge[1], "nudgeevens", 909, 111, 54);
        knob (freq[0], "freqodds", 91, 255, 44);
        knob (freq[1], "freqevens", 909, 255, 44);

        // The two Lock jacks, as trigger buttons under the Freq knobs (sketch v3 "Lock odds" and "Lock evens"; the firmware's
        // params.c process_lock_jacks flips the lock of channels 1 3 5 or 2 4 6 according to the 135 | 1 and 246 | 6 switches).
        lockOdds  = std::make_unique<smrui::PulseButton> ("Lock odds", *apvts.getParameter ("lockjack135"), 0, false, linkFor (p, 4));
        lockEvens = std::make_unique<smrui::PulseButton> ("Lock evens", *apvts.getParameter ("lockjack246"), 0, false, linkFor (p, 5));
        place (*lockOdds, 91, 317, 84, 30);
        place (*lockEvens, 909, 317, 84, 30);
        addAndMakeVisible (*lockOdds);
        addAndMakeVisible (*lockEvens);

        // The ROTATE encoder press (plugin only: the hardware has the encoder). With a Lock held it forbids, or allows again, the note that
        // channel is on; all six Locks held clear every forbidden note (params.c process_freq_blocks). It has no other effect in this plugin.
        rotPress = std::make_unique<smrui::PulseButton> ("Rotate press", *apvts.getParameter ("rotatepress"), 0, true, linkFor (p, 6));
        place (*rotPress, 253, 606, 124, 32);
        addAndMakeVisible (*rotPress);

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
        sw (envSwitch,    "envspeed",      4, false, 909, 429, 104);   // the fourth position, 1V/oct, is plugin only (Milestone 7, step 5)

        // Rotate: the two trigger buttons and the centre knob.
        rotCcw = std::make_unique<smrui::PulseButton> ("Rotate trig", *apvts.getParameter ("rotateccw"), 1, false, linkFor (p, 1));
        rotCw  = std::make_unique<smrui::PulseButton> ("Rotate trig", *apvts.getParameter ("rotatecw"), 2, false, linkFor (p, 0));
        rotCcw->setBounds (191, 547, 124, 32);
        rotCw->setBounds (685, 547, 124, 32);
        addAndMakeVisible (*rotCcw);
        addAndMakeVisible (*rotCw);
        dial = std::make_unique<smrui::RotateDial> (*apvts.getParameter ("rotatecw"), *apvts.getParameter ("rotateccw"), linkFor (p, 0), linkFor (p, 1));
        dial->setBounds (462, 586, 76, 76);
        addAndMakeVisible (*dial);

        // Plugin-only row: Filter Mode, Filter Type, Bank, Scale down and up.
        combo (filterMode, "filtermode", 176, 757, 100, 31);
        combo (filterType, "filtertype", 282, 757, 92, 31);
        combo (bank,       "bank",       380, 757, 112, 31);
        scaleDown = std::make_unique<smrui::PulseButton> ("Scale", *apvts.getParameter ("scaledown"), 4, true, linkFor (p, 3));
        scaleUp   = std::make_unique<smrui::PulseButton> ("Scale", *apvts.getParameter ("scaleup"), 3, true, linkFor (p, 2));
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
        static const char* const names[7] = { "Rot CW", "Rot CCW", "Scale up", "Scale down", "Lock 135", "Lock 246", "Rot press" };
        for (int k = 0; k < 7; ++k)
            t << (k ? "   " : "") << names[k] << " " << (juce::int64) sent[k] << "/" << (juce::int64) seen[k];
        if (t != diagText)
        {
            diagText = t;
            repaint (200, 46, 770, 24);
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
            repaint (312, 506, 376, 238);
        }
        for (int c = 0; c < 6; ++c)
            if (std::memcmp (newEnv[c], envLed[c], sizeof envLed[c]) != 0)
            {
                std::memcpy (envLed[c], newEnv[c], sizeof envLed[c]);
                repaint (kStripX[c] - 13, kEnvY - 13, 26, 26);
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
            repaint (kClipX - 8, kClipY - 8, 16, 16);
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
                repaint (meterX (k) - 2, kMeterY - 2, kMeterW + 4, kMeterH + 4);
            }
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
        g.drawText (diagText, juce::Rectangle<float> (200.0f, 48.0f, 765.0f, 18.0f), juce::Justification::centredRight, false);
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
        g.setFont (14.0f);   // smaller and centred under the switch (909): at 16 pt and centre 905 the "t" of 1V/oct touched the panel edge (Bryn, 2026-10-29)
        label (g, "Fast " + dot + " Trig " + dot + " Slow " + dot + " 1V/oct", 909.0f, 456.0f, 160.0f);
        g.setFont (16.0f);
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
        {   // the 1V/oct position of the env switch is not on the module: dashed around its quarter of the switch
            auto b = envSwitch->getBounds().toFloat();
            smrui::dashedRoundedRect (g, b.removeFromRight (b.getWidth() * 0.25f).reduced (1.0f), 6.0f);
        }
        g.setColour (smrui::softInk());
        g.setFont (13.0f);
        g.drawText ("dashed outline = plugin only", juce::Rectangle<float> (645.0f, 760.0f, 190.0f, 24.0f), juce::Justification::centredLeft, false);

        // Milestone 6b lights. Brightness is the firmware's 12 bit duty value (1023 is its clamp) drawn on a dark lens: 0 is the lens,
        // 1023 is the full colour. Colours are led_ring.c DEFAULT_COLOR_CH[0]. Ring: index i sits i x 18 degrees clockwise from the
        // top (docs/open-questions.md M6-i); the ellipse is the sketch's.
        for (int i = 0; i < smr_plugin::kRingLeds; ++i)
        {
            const float a = (float) i * juce::MathConstants<float>::twoPi / (float) smr_plugin::kRingLeds;
            const float x = kRingCx + kRingRx * std::sin (a), y = kRingCy - kRingRy * std::cos (a);
            // A lit LED is drawn larger than an unlit one, so it reads on the white panel (display choice, not firmware).
            const bool lit = ringLed[i][0] > 0 || ringLed[i][1] > 0 || ringLed[i][2] > 0;
            const float r = lit ? 11.0f : 7.0f;
            g.setColour (ledColour (ringLed[i], 0.35f));
            g.fillEllipse (x - r, y - r, 2.0f * r, 2.0f * r);
            g.setColour (juce::Colour (0xff1c1c1c));
            g.drawEllipse (x - r, y - r, 2.0f * r, 2.0f * r, 2.5f);
        }

        g.setColour (smrui::softInk());
        g.setFont (16.0f);
        label (g, "Env out", 510.0f, 475.0f, 100.0f);
        for (int c = 0; c < 6; ++c)
        {
            g.setColour (ledColour (envLed[c]));
            g.fillEllipse ((float) stripX[c] - 11.0f, (float) kEnvY - 11.0f, 22.0f, 22.0f);
            g.setColour (juce::Colour (0xff1c1c1c));
            g.drawEllipse ((float) stripX[c] - 11.0f, (float) kEnvY - 11.0f, 22.0f, 22.0f, 2.5f);
        }

        // Input clip LED (one LED between the IN jacks, panel photo). Colour from the photo; the firmware only switches a pin.
        g.setColour (inClipOn ? juce::Colour (0xffffd21f) : juce::Colour (0xff4a4430));
        g.fillEllipse ((float) kClipX - 6.0f, (float) kClipY - 6.0f, 12.0f, 12.0f);
        g.setColour (smrui::edge());
        g.drawEllipse ((float) kClipX - 6.0f, (float) kClipY - 6.0f, 12.0f, 12.0f, 1.0f);
        g.setColour (smrui::softInk());
        g.setFont (11.0f);
        label (g, "clip", (float) kClipX, (float) kClipY + 15.0f, 40.0f);

        // In and Out meters: plugin only, so dashed.
        for (int k = 0; k < 4; ++k)
        {
            const Meter& m = meters[k];
            const float x = (float) meterX (k), y = (float) kMeterY, w = (float) kMeterW, h = (float) kMeterH;
            if (m.fillPx > 0)
            {
                g.setColour (m.over ? juce::Colour (0xffe07a3c) : juce::Colour (0xff7cc4ad));
                g.fillRoundedRectangle (x, y + h - (float) m.fillPx, w, (float) m.fillPx, 2.0f);
            }
            if (m.holdPx > 0)
            {
                g.setColour (juce::Colour (0xff0f7a63));
                g.fillRect (x, y + h - (float) m.holdPx - 1.0f, w, 2.0f);
            }
            g.setColour (smrui::edge());
            smrui::dashedRoundedRect (g, juce::Rectangle<float> (x - 1.0f, y - 1.0f, w + 2.0f, h + 2.0f), 3.0f);
        }
        g.setColour (smrui::softInk());
        g.setFont (13.0f);
        label (g, "In: odds " + dot + " evens", 74.0f, 641.0f, 130.0f);
        label (g, "Out: L " + dot + " R", 926.0f, 641.0f, 100.0f);

        // Value of the control being dragged.
        g.setFont (14.0f);
        g.drawText (readout, juce::Rectangle<float> (176.0f, 796.0f, 640.0f, 20.0f), juce::Justification::centredLeft, false);
    }

private:
    // Milestone 6b geometry on the 1000 x 824 canvas (sketch v3 at half size).
    static constexpr int kStripX[6] = { 231, 342, 454, 566, 678, 790 };
    static constexpr int kEnvY = 450;
    static constexpr float kRingCx = 500.0f, kRingCy = 625.0f, kRingRx = 176.0f, kRingRy = 106.0f;
    static constexpr int kClipX = 126, kClipY = 556;
    static constexpr int kMeterY = 547, kMeterW = 14, kMeterH = 75;
    static int meterX (int k) { static const int x[4] = { 53, 80, 906, 932 }; return x[k & 3]; }
    // Plugin-only meter ballistics (not from the firmware): -60 dBFS at the bottom, 0 dBFS at the top.
    static constexpr float kMeterFallDbPerSec = 24.0f, kMeterHoldMs = 1000.0f, kMeterHoldFallDbPerSec = 12.0f;
    static int meterPixels (float db) { return juce::roundToInt (juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f) * (float) kMeterH); }

    // A lens that is dark at 0 and shows the full colour at 1023 (the firmware's clamp; the driver is 12 bit, docs/m6b-led-notes.md).
    // gamma 1.0 is linear (Env out lights). The ring uses 0.5 so the firmware's 5 % floor (value 38 of 1023) shows as a dim dot
    // rather than vanishing into the lens (Bryn, 2026-10-27: on the real SMR a channel at the bottom is dimmed, not out).
    // kLedFull: the brightest firmware colour component is 770 (DEFAULT_COLOR_CH[0] ch2), so 770 is drawn as the full colour; sums
    // above it clamp. Raised contrast on the white panel (Bryn, 2026-10-27). Plugin display choice, not firmware.
    static constexpr float kLedFull = 770.0f;
    static juce::Colour ledColour (const uint16_t* rgb, float gamma = 1.0f)
    {
        constexpr float base = 0.17f;
        auto ch = [gamma] (uint16_t v) { return juce::jmin (1.0f, base + std::pow ((float) v / kLedFull, gamma) * (1.0f - base)); };
        return juce::Colour::fromFloatRGBA (ch (rgb[0]), ch (rgb[1]), ch (rgb[2]), 1.0f);
    }

    struct Meter { float shown = -100.0f, hold = -100.0f, holdAgeMs = 0.0f; int fillPx = 0, holdPx = 0; bool over = false; };

    smr_plugin::DisplayData last;
    uint16_t ringLed[smr_plugin::kRingLeds][3] = {};
    uint16_t envLed[6][3] = {};
    float sliderDuty[6] = {};
    bool sliderClip[6] = {};
    bool inClipOn = false;
    bool flash = false;
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
    uint32_t sent[7] = {};

    juce::Slider levels[6], nudge[2], freq[2], qKnob, morph, spread;
    std::unique_ptr<smrui::LockButton> locks[6];
    juce::ComboBox filterMode, filterType, bank;
    std::unique_ptr<smrui::SlideSwitch> oddsSwitch, evensSwitch, slewSwitch, scaleRotSwitch, prePostSwitch, envSwitch;
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
