// src/plugin: the drawing and small controls used by the editor. Header only, JUCE only, no core code.
// "Gold sections" look (see SmrStyle.h): black panel, gold hairlines, cream knob caps, blue lamps for on or chosen.
// Look only: the behaviour of every control here (what a click or drag writes to which host parameter) is unchanged from
// Milestones 6 and 7; only the drawing changed.
#pragma once

#include <cmath>
#include <functional>
#include <utility>

#include <juce_audio_processors/juce_audio_processors.h>

#include "SmrStyle.h"

namespace smrui
{
    // Milestone 7: a Lock button. A plain click is a tap (the Lock hold parameter is on while the mouse is down; the core's
    // process_lock_buttons then toggles the lock). A shift-click toggles a sticky hold (the parameter stays on, drawn flashing),
    // for the gestures that need the button held while something else is turned or pressed. The Lock parameter itself is only
    // displayed here (it follows the core); this button never writes it.
    class LockButton : public juce::ToggleButton
    {
    public:
        explicit LockButton (juce::RangedAudioParameter& holdParam)
            : hold (holdParam, [this] (float v) { held = v >= 0.5f; repaint(); })
        {
            hold.sendInitialUpdate();
        }

        void mouseDown (const juce::MouseEvent& e) override
        {
            if (! isEnabled()) return;
            if (e.mods.isShiftDown())
            {
                sticky = ! sticky;
                hold.setValueAsCompleteGesture (sticky ? 1.0f : 0.0f);
                tapDown = false;
            }
            else if (! sticky)
            {
                tapDown = true;
                hold.setValueAsCompleteGesture (1.0f);
            }
            repaint();
        }

        void mouseUp (const juce::MouseEvent&) override
        {
            if (tapDown)
            {
                tapDown = false;
                hold.setValueAsCompleteGesture (0.0f);
            }
            repaint();
        }

        bool isSticky() const noexcept { return sticky; }
        bool isHeld() const noexcept { return held; }

        // Set by the editor every frame: the LED shows the opposite of the lock (Q-lock flicker, staged-clear flash: leds.c update_lock_leds) and the flash phase for a sticky hold.
        bool inverted = false;
        bool flashPhase = false;

    private:
        juce::ParameterAttachment hold;
        bool held = false, sticky = false, tapDown = false;
    };

    class SmrLookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        SmrLookAndFeel()
        {
            setColour (juce::PopupMenu::backgroundColourId, menu());
            setColour (juce::PopupMenu::textColourId, text());
            setColour (juce::PopupMenu::highlightedBackgroundColourId, lampFill());
            setColour (juce::PopupMenu::highlightedTextColourId, text());
            setColour (juce::ComboBox::textColourId, juce::Colours::transparentBlack);   // the pill draws the name itself, in capitals (drawComboBox)
            setColour (juce::ComboBox::backgroundColourId, panel());
            setColour (juce::ComboBox::outlineColourId, gold());
            setColour (juce::ComboBox::arrowColourId, gold());
            setColour (juce::BubbleComponent::backgroundColourId, menu());
            setColour (juce::BubbleComponent::outlineColourId, gold());
            setColour (juce::Label::textColourId, text());
        }

        juce::Font getComboBoxFont (juce::ComboBox&) override { return font (labelSize, false, labelSpacing); }
        juce::Font getPopupMenuFont() override { return font (13.0f); }

        int getSliderThumbRadius (juce::Slider&) override { return 7; }

        // The level slider: only the cream thumb with its white LED. The gold track and its ticks are fixed artwork.
        void drawLinearSlider (juce::Graphics& g, int x, int, int width, int, float sliderPos,
                               float, float, juce::Slider::SliderStyle, juce::Slider& s) override
        {
            const float cx = (float) x + (float) width * 0.5f;
            const juce::Rectangle<float> thumb (cx - 16.0f, sliderPos - 8.0f, 32.0f, 16.0f);
            g.setColour (cap());
            g.fillRoundedRectangle (thumb, 4.0f);
            g.setColour (capLine());
            g.drawRoundedRectangle (thumb, 4.0f, 0.8f);

            // The slider's white LED (Milestone 6b), inset in the thumb. "led" is the firmware's duty (channel_level / 4, 25 % at the
            // top, leds.c update_slider_LEDs); "ledclip" switches it fully on (filter.c process_audio_block). Off is a dark lens.
            const float duty = (float) s.getProperties().getWithDefault ("led", 0.0f);
            const bool clip = (bool) s.getProperties().getWithDefault ("ledclip", false);
            const float b = clip ? 1.0f : juce::jlimit (0.0f, 1.0f, duty);
            const juce::Rectangle<float> led (cx - 9.0f, sliderPos - 2.5f, 18.0f, 5.0f);
            if (clip)
            {
                g.setColour (juce::Colours::white.withAlpha (0.35f));
                g.fillRoundedRectangle (led.expanded (4.0f, 3.0f), 5.0f);
            }
            g.setColour (juce::Colour (0xff2a2a2a).interpolatedWith (juce::Colours::white, b));
            g.fillRoundedRectangle (led, 2.5f);
            g.setColour (juce::Colour (0xff2a2a2a));
            g.drawRoundedRectangle (led, 2.5f, 0.6f);
        }

        // A knob: a black skirt with a cream cap and a dark pointer line. Its gold range arc is fixed artwork on the panel.
        void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                               float startAngle, float endAngle, juce::Slider&) override
        {
            const auto r = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
            const float u = juce::jmin (r.getWidth(), r.getHeight()) / (float) knobBox;      // one canvas unit
            const auto c = r.getCentre();
            auto disc = [&] (float radius, juce::Colour fill, juce::Colour line, float lw)
            {
                g.setColour (fill);
                g.fillEllipse (c.x - radius, c.y - radius, radius * 2.0f, radius * 2.0f);
                g.setColour (line);
                g.drawEllipse (c.x - radius, c.y - radius, radius * 2.0f, radius * 2.0f, lw);
            };
            disc (22.0f * u, skirt(), skirtLine(), 1.2f * u);
            disc (16.0f * u, cap(), capLine(), 0.8f * u);
            const float a = startAngle + pos * (endAngle - startAngle);
            const float sx = std::sin (a), sy = -std::cos (a);
            juce::Path p;
            p.startNewSubPath (c.x + 5.0f * u * sx, c.y + 5.0f * u * sy);
            p.lineTo (c.x + 15.0f * u * sx, c.y + 15.0f * u * sy);
            g.setColour (pointer());
            g.strokePath (p, juce::PathStrokeType (2.0f * u, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // Lock buttons: a round button, lit blue when locked.
        void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool, bool down) override
        {
            const auto r = b.getLocalBounds().toFloat();
            const float cx = r.getCentreX(), cy = r.getCentreY(), rad = 13.0f;
            bool on = b.getToggleState();
            bool stickyFlash = false;
            if (auto* lb = dynamic_cast<LockButton*> (&b))
            {
                if (lb->inverted) on = ! on;                       // leds.c update_lock_leds: a Q-locked channel inverts once in 16 updates; a staged clear flashes
                stickyFlash = lb->isSticky() && lb->flashPhase;    // plugin-only: shows that the button is being held by shift-click
            }
            if (on)
            {
                g.setColour (lamp().withAlpha (0.18f));
                g.fillEllipse (cx - 19.0f, cy - 19.0f, 38.0f, 38.0f);
                g.setColour (lampFill());
                g.fillEllipse (cx - rad, cy - rad, rad * 2.0f, rad * 2.0f);
                g.setColour (lamp());
                g.drawEllipse (cx - rad, cy - rad, rad * 2.0f, rad * 2.0f, (down || stickyFlash) ? 2.2f : 1.4f);
            }
            else
            {
                g.setColour (skirt());
                g.fillEllipse (cx - rad, cy - rad, rad * 2.0f, rad * 2.0f);
                g.setColour (gold());
                g.drawEllipse (cx - rad, cy - rad, rad * 2.0f, rad * 2.0f, (down || stickyFlash) ? 2.2f : 1.1f);
            }
            if (stickyFlash)
            {
                g.setColour (text());
                g.drawEllipse (cx - rad + 4.0f, cy - rad + 4.0f, (rad - 4.0f) * 2.0f, (rad - 4.0f) * 2.0f, 1.2f);
            }
        }

        // The Bank pop-up: a dashed pill (plugin only) with a chevron.
        void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box) override
        {
            const auto r = juce::Rectangle<float> (0.0f, 0.0f, (float) w, (float) h).reduced (0.5f);
            dashedRoundedRect (g, r, r.getHeight() * 0.5f);
            textAt (g, box.getText().toUpperCase(), (float) (w - 28) * 0.5f, (float) h * 0.5f + 4.3f, text(), labelSize,
                    juce::Justification::centred, labelSpacing, (float) (w - 28 - 16));
            juce::Path chevron;
            const float cx = (float) w - 16.0f, cy = (float) h * 0.5f;
            chevron.startNewSubPath (cx - 4.0f, cy - 2.5f);
            chevron.lineTo (cx, cy + 2.5f);
            chevron.lineTo (cx + 4.0f, cy - 2.5f);
            g.setColour (gold());
            g.strokePath (chevron, juce::PathStrokeType (0.9f));
        }

        // The corner handle that resizes the window: three gold hairlines.
        void drawCornerResizer (juce::Graphics& g, int w, int h, bool, bool) override
        {
            g.setColour (gold().withAlpha (0.7f));
            for (int i = 1; i <= 3; ++i)
                g.drawLine ((float) w - 4.0f * (float) i - 2.0f, (float) h - 1.0f, (float) w - 1.0f, (float) h - 4.0f * (float) i - 2.0f, 0.8f);
        }

        void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
        {
            label.setBorderSize (juce::BorderSize<int> (0));
            label.setBounds (0, 0, box.getWidth() - 28, box.getHeight());
            label.setJustificationType (juce::Justification::centred);
            label.setFont (font (labelSize, false, labelSpacing));
        }
    };

    // A switch bound to a choice or bool parameter, drawn as a list of lamps (List: one row per position) or as one lamp with its
    // label (Option, for a plain on/off). A click on a 2 position switch toggles it; on a longer switch the row that is clicked is
    // selected. "reversed" draws parameter index 0 on the last row. dashedRow: the visible row (List) that is plugin only, or for
    // an Option any value >= 0 draws the dashed outline.
    class SlideSwitch : public juce::Component
    {
    public:
        enum class Look { List, Option };
        static constexpr float rowH = 18.0f, optionH = 20.0f;

        SlideSwitch (juce::RangedAudioParameter& p, int numPositions, bool reversed, juce::StringArray rowLabels, Look lookKind, int dashed = -1)
            : n (numPositions), rev (reversed), labels (std::move (rowLabels)), look (lookKind), dashedRow (dashed),
              attach (p, [this] (float v) { index = juce::jlimit (0, n - 1, juce::roundToInt (v)); repaint(); })
        {
            attach.sendInitialUpdate();
        }

        static int heightFor (Look l, int n) { return l == Look::Option ? (int) optionH : (int) rowH * n; }

        void paint (juce::Graphics& g) override
        {
            const auto b = getLocalBounds().toFloat();
            if (look == Look::Option)
            {
                const bool on = index == 1;
                const float cy = b.getCentreY();
                drawLamp (g, b.getX() + 9.0f, cy, on, on ? 3.6f : 3.4f);
                textAt (g, labels[0], b.getX() + 20.0f, cy + 4.3f, on ? text() : soft(), labelSize,
                        juce::Justification::centredLeft, 1.2f, b.getWidth() - 20.0f - (dashedRow >= 0 ? 8.0f : 0.0f));
                if (dashedRow >= 0)
                    dashedRoundedRect (g, { b.getX(), cy - 10.0f, widthOf (labels[0], font (labelSize, false, 1.2f)) + 28.0f, 20.0f }, 6.0f);
                return;
            }
            for (int row = 0; row < n; ++row)
            {
                const int pos = rev ? (n - 1 - row) : row;           // parameter index shown on this row
                const bool on = index == pos;
                const float cy = b.getY() + rowH * ((float) row + 0.5f);
                drawLamp (g, b.getX() + 9.0f, cy, on, on ? 3.6f : 3.4f);
                textAt (g, labels[pos], b.getX() + 20.0f, cy + 4.3f, on ? text() : muted(), labelSize,
                        juce::Justification::centredLeft, labelSpacing, b.getWidth() - 20.0f - (row == dashedRow ? 8.0f : 0.0f));
                if (row == dashedRow)
                    dashedRoundedRect (g, { b.getX(), cy - rowH * 0.5f + 1.0f, widthOf (labels[pos], font (labelSize, false, labelSpacing)) + 28.0f, rowH - 2.0f }, 6.0f);
            }
        }

        void mouseDown (const juce::MouseEvent& e) override
        {
            int newIndex = index;
            if (n == 2)
                newIndex = 1 - index;
            else
            {
                const int row = juce::jlimit (0, n - 1, (int) (e.position.y / rowH));
                newIndex = rev ? (n - 1 - row) : row;
            }
            attach.setValueAsCompleteGesture ((float) newIndex);
        }

    private:
        int n;
        bool rev;
        juce::StringArray labels;
        Look look;
        int dashedRow;
        int index = 0;
        juce::ParameterAttachment attach;
    };

    // A row of choices in a dashed gold pill (Filter Mode, Filter Type: plugin only); the chosen one has a blue lamp. A click selects
    // the segment that is clicked.
    class ChoicePill : public juce::Component
    {
    public:
        ChoicePill (juce::RangedAudioParameter& p, juce::StringArray segmentLabels)
            : labels (std::move (segmentLabels)),
              attach (p, [this] (float v) { index = juce::jlimit (0, labels.size() - 1, juce::roundToInt (v)); repaint(); })
        {
            attach.sendInitialUpdate();
        }

        void paint (juce::Graphics& g) override
        {
            const auto r = getLocalBounds().toFloat().reduced (0.5f);
            const int n = labels.size();
            dashedRoundedRect (g, r, r.getHeight() * 0.5f);
            const float cw = r.getWidth() / (float) n;
            g.setColour (gold());
            for (int i = 1; i < n; ++i)
                g.fillRect (r.getX() + cw * (float) i - 0.35f, r.getY() + 0.5f, 0.7f, r.getHeight() - 1.0f);
            for (int i = 0; i < n; ++i)
            {
                const bool on = index == i;
                const float cx = r.getX() + cw * ((float) i + 0.5f), cy = r.getCentreY();
                textAt (g, labels[i], cx, cy + 4.3f, on ? text() : muted(), labelSize, juce::Justification::centred, labelSpacing, cw - 22.0f);
                if (on) drawLamp (g, cx - widthOf (labels[i], font (labelSize, false, labelSpacing)) * 0.5f - 8.0f, cy, true, 2.6f);
            }
        }

        void mouseDown (const juce::MouseEvent& e) override
        {
            const int n = labels.size();
            attach.setValueAsCompleteGesture ((float) juce::jlimit (0, n - 1, (int) (e.position.x / ((float) getWidth() / (float) n))));
        }

    private:
        juce::StringArray labels;
        int index = 0;
        juce::ParameterAttachment attach;
    };

    // How a trigger button learns what the audio side has seen (see Pulser). sent: optional counter for the window's diagnostic line.
    struct TriggerLink
    {
        std::function<uint32_t()> rising, falling;
        uint32_t* sent = nullptr;
    };

    // Fires a trigger parameter the way a patch cable would, and makes sure the audio side saw it. The plugin reads parameters once
    // per host block and the core acts on the rising edge (ParamMap.h ParamApplier::apply). A fixed 60 ms pulse was lost in Logic
    // (Bryn's Intel Mac test, 2026-10-27: 10 sent, 6 and 8 seen; docs/open-questions.md M6-c), so this holds the value at 1 until the
    // audio side has counted the rising edge, then sets 0 and waits until it has counted that too, then sends the next queued pulse.
    // If the audio side does not answer within 3 s (for example the host is not processing audio), the pulse is dropped and so is
    // the queue. Up to 8 pulses wait their turn.
    class Pulser : private juce::Timer
    {
    public:
        Pulser (juce::RangedAudioParameter& p, TriggerLink l) : attach (p, [] (float) {}), link (std::move (l)) {}

        ~Pulser() override
        {
            stopTimer();
            if (state == State::WaitRise)
                attach.setValueAsCompleteGesture (0.0f);     // never leave the parameter high
        }

        void request()
        {
            if (pending < kMaxPending) ++pending;
            if (state == State::Idle) startNext();
        }

    private:
        enum class State { Idle, WaitRise, WaitFall };
        static constexpr int kPollMs = 5;
        static constexpr int kGiveUpMs = 3000;
        static constexpr int kMaxPending = 8;

        void startNext()
        {
            if (pending == 0)
            {
                state = State::Idle;
                stopTimer();
                return;
            }
            --pending;
            risingBase = link.rising();
            attach.setValueAsCompleteGesture (1.0f);
            if (link.sent != nullptr) ++*link.sent;
            state = State::WaitRise;
            since = juce::Time::getMillisecondCounter();
            startTimer (kPollMs);
        }

        void timerCallback() override
        {
            const int elapsed = (int) (juce::Time::getMillisecondCounter() - since);
            if (state == State::WaitRise)
            {
                const bool seen = link.rising() != risingBase;
                if (seen || elapsed > kGiveUpMs)
                {
                    if (! seen) pending = 0;                  // the audio side is not answering: drop the queue too
                    fallingBase = link.falling();
                    attach.setValueAsCompleteGesture (0.0f);
                    state = State::WaitFall;
                    since = juce::Time::getMillisecondCounter();
                }
            }
            else if (state == State::WaitFall)
            {
                if (link.falling() != fallingBase || elapsed > kGiveUpMs)
                    startNext();
            }
        }

        juce::ParameterAttachment attach;
        TriggerLink link;
        State state = State::Idle;
        int pending = 0;
        uint32_t risingBase = 0, fallingBase = 0, since = 0;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Pulser)
    };

    // A button that fires one trigger parameter. glyph: 0 none, 1 left, 2 right, 3 up, 4 down (a small triangle).
    class PulseButton : public juce::Button
    {
    public:
        PulseButton (const juce::String& text, juce::RangedAudioParameter& p, int glyphKind, bool dashedOutline, TriggerLink link)
            : juce::Button (text), pulser (p, std::move (link)), glyph (glyphKind), dashed (dashedOutline)
        {
            onClick = [this] { pulser.request(); };
        }

        void paintButton (juce::Graphics& g, bool, bool down) override
        {
            const auto r = getLocalBounds().toFloat().reduced (0.5f);
            const float corner = r.getHeight() * 0.5f;
            if (down)
            {
                g.setColour (lampFill());
                g.fillRoundedRectangle (r, corner);
            }
            if (dashed && ! down) dashedRoundedRect (g, r, corner);
            else
            {
                g.setColour (down ? lamp() : gold());
                g.drawRoundedRectangle (r, corner, down ? 1.1f : 0.8f);
            }

            const auto label = getButtonText();
            const auto f = font (labelSize, false, labelSpacing);
            const float tw = widthOf (label, f);
            const float gw = glyph != 0 ? 10.0f : 0.0f;
            const float gap = glyph != 0 ? 8.0f : 0.0f;
            const float total = tw + gw + gap;
            logFit (label, total, r.getWidth() - 12.0f);
            float x = r.getCentreX() - total * 0.5f;
            const float cy = r.getCentreY();
            auto drawTriangle = [&] (float gx)
            {
                juce::Path t;
                if (glyph == 1)      t.addTriangle (gx + 9.0f, cy - 5.0f, gx + 9.0f, cy + 5.0f, gx, cy);
                else if (glyph == 2) t.addTriangle (gx, cy - 5.0f, gx, cy + 5.0f, gx + 9.0f, cy);
                else if (glyph == 3) t.addTriangle (gx, cy + 4.0f, gx + 10.0f, cy + 4.0f, gx + 5.0f, cy - 5.0f);
                else if (glyph == 4) t.addTriangle (gx, cy - 4.0f, gx + 10.0f, cy - 4.0f, gx + 5.0f, cy + 5.0f);
                g.setColour (smrui::text());
                g.fillPath (t);
            };
            if (glyph == 1 || glyph == 4)
            {
                drawTriangle (x);
                x += gw + gap;
            }
            textAt (g, label, x, cy + 4.3f, smrui::text(), labelSize, juce::Justification::centredLeft, labelSpacing);
            if (glyph == 2 || glyph == 3)
                drawTriangle (x + tw + gap);
        }

    private:
        Pulser pulser;
        int glyph;
        bool dashed;
    };

    // The big centre knob. It has no position of its own: turning it fires the Rotate Clockwise or Counter-clockwise
    // trigger once per step of turn (the same two parameters as the Rotate trig buttons). The mouse wheel does the same
    // one step per notch. (The module's knob also has a push action for Scale mode; the plugin has no parameter for it.)
    // Drawn as an endless knob: a full gold ring with a tick every step (20 degrees), a black skirt and a cream cap.
    class RotateDial : public juce::Component
    {
    public:
        RotateDial (juce::RangedAudioParameter& cw, juce::RangedAudioParameter& ccw, TriggerLink linkCw, TriggerLink linkCcw)
            : pulseCw (cw, std::move (linkCw)), pulseCcw (ccw, std::move (linkCcw)) {}

        void paint (juce::Graphics& g) override
        {
            const auto c = getLocalBounds().toFloat().getCentre();
            const float u = (float) juce::jmin (getWidth(), getHeight()) / 92.0f;     // one canvas unit (the dial is 92 units wide)
            g.setColour (gold());
            g.drawEllipse (c.x - 40.0f * u, c.y - 40.0f * u, 80.0f * u, 80.0f * u, 0.9f * u);
            for (int k = 0; k < 18; ++k)
            {
                const float a = juce::degreesToRadians ((float) k * 20.0f);
                g.drawLine (c.x + 40.0f * u * std::sin (a), c.y - 40.0f * u * std::cos (a),
                            c.x + 44.0f * u * std::sin (a), c.y - 44.0f * u * std::cos (a), 0.8f * u);
            }
            g.setColour (skirt());
            g.fillEllipse (c.x - 30.0f * u, c.y - 30.0f * u, 60.0f * u, 60.0f * u);
            g.setColour (skirtLine());
            g.drawEllipse (c.x - 30.0f * u, c.y - 30.0f * u, 60.0f * u, 60.0f * u, 1.2f * u);
            g.setColour (cap());
            g.fillEllipse (c.x - 22.0f * u, c.y - 22.0f * u, 44.0f * u, 44.0f * u);
            g.setColour (capLine());
            g.drawEllipse (c.x - 22.0f * u, c.y - 22.0f * u, 44.0f * u, 44.0f * u, 0.8f * u);
            juce::Path p;
            p.startNewSubPath (0.0f, -7.0f * u);
            p.lineTo (0.0f, -20.0f * u);
            g.setColour (pointer());
            g.strokePath (p, juce::PathStrokeType (2.4f * u, juce::PathStrokeType::curved, juce::PathStrokeType::rounded),
                          juce::AffineTransform::rotation (shown).translated (c.x, c.y));
        }

        void mouseDown (const juce::MouseEvent& e) override { last = angleAt (e.position); }

        void mouseDrag (const juce::MouseEvent& e) override
        {
            const float a = angleAt (e.position);
            float d = a - last;
            if (d > juce::MathConstants<float>::pi)  d -= juce::MathConstants<float>::twoPi;
            if (d < -juce::MathConstants<float>::pi) d += juce::MathConstants<float>::twoPi;
            last = a;
            shown += d;
            acc += d;
            while (acc >= kStep)  { acc -= kStep; pulseCw.request(); }
            while (acc <= -kStep) { acc += kStep; pulseCcw.request(); }
            repaint();
        }

        void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w) override
        {
            if (w.deltaY > 0.0f)       { shown += kStep; pulseCw.request(); }
            else if (w.deltaY < 0.0f)  { shown -= kStep; pulseCcw.request(); }
            repaint();
        }

    private:
        static constexpr float kStep = juce::MathConstants<float>::pi / 9.0f;   // 20 degrees per trigger

        float angleAt (juce::Point<float> p) const
        {
            const auto c = getLocalBounds().toFloat().getCentre();
            return std::atan2 (p.y - c.y, p.x - c.x);     // grows clockwise on screen (y points down)
        }

        Pulser pulseCw, pulseCcw;
        float last = 0.0f, acc = 0.0f, shown = 0.0f;
    };
}
