// src/plugin: the drawing and small controls used by the editor (Milestone 6a). Header only, JUCE only, no core code.
// Colours and shapes here are this project's own design (sketch v3); none of it is taken from the firmware or the
// module's panel artwork. The channel colours in particular are placeholders: the light ring and Env Out colours
// must come from the firmware (led_ring.c) in Milestone 6b (docs/open-questions.md M6-d).
#pragma once

#include <cmath>
#include <functional>

#include <juce_audio_processors/juce_audio_processors.h>

namespace smrui
{
    struct ChannelColours { juce::uint32 fill, edge; };

    // Sketch v3 slider-thumb colours, channel 1 to 6.
    inline const ChannelColours& channelColours (int ch)
    {
        static const ChannelColours c[6] = {
            { 0xffe6effa, 0xff1f5fa8 },   // 1 blue
            { 0xffdff3ec, 0xff0f7a63 },   // 2 teal
            { 0xffe9f2dc, 0xff3d6d12 },   // 3 green
            { 0xfff0ede8, 0xff6b6359 },   // 4 grey
            { 0xfffbe9e3, 0xffa8431f },   // 5 orange
            { 0xfffbe6ee, 0xffa8305c } }; // 6 pink
        return c[juce::jlimit (0, 5, ch)];
    }

    inline juce::Colour paper()    { return juce::Colour (0xfffafaf7); }
    inline juce::Colour edge()     { return juce::Colour (0xffbdbdb8); }
    inline juce::Colour ink()      { return juce::Colour (0xff1c1c1e); }
    inline juce::Colour softInk()  { return juce::Colour (0xff55554f); }

    inline void dashedRoundedRect (juce::Graphics& g, juce::Rectangle<float> r, float corner)
    {
        juce::Path p;
        p.addRoundedRectangle (r, corner);
        juce::Path dashed;
        const float dashes[] = { 4.0f, 3.0f };
        juce::PathStrokeType (1.2f).createDashedStroke (dashed, p, dashes, 2);
        g.fillPath (dashed);
    }

    class SmrLookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        SmrLookAndFeel()
        {
            setColour (juce::ComboBox::backgroundColourId, paper());
            setColour (juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
            setColour (juce::ComboBox::textColourId, ink());
            setColour (juce::ComboBox::arrowColourId, softInk());
            setColour (juce::PopupMenu::backgroundColourId, paper());
            setColour (juce::PopupMenu::textColourId, ink());
        }

        int getSliderThumbRadius (juce::Slider&) override { return 7; }

        void drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                               float, float, juce::Slider::SliderStyle, juce::Slider& s) override
        {
            const float cx = (float) x + (float) width * 0.5f;
            g.setColour (juce::Colour (0xffc9c9c6));
            g.fillRoundedRectangle (cx - 3.5f, (float) y, 7.0f, (float) height, 3.5f);
            const juce::Rectangle<float> thumb (cx - 15.0f, sliderPos - 7.0f, 30.0f, 14.0f);
            g.setColour (s.findColour (juce::Slider::thumbColourId));
            g.fillRoundedRectangle (thumb, 4.0f);
            g.setColour (s.findColour (juce::Slider::trackColourId));
            g.drawRoundedRectangle (thumb, 4.0f, 1.6f);
        }

        void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                               float startAngle, float endAngle, juce::Slider&) override
        {
            const auto r = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (2.0f);
            const float radius = juce::jmin (r.getWidth(), r.getHeight()) * 0.5f;
            const auto c = r.getCentre();
            g.setColour (paper());
            g.fillEllipse (c.x - radius, c.y - radius, radius * 2.0f, radius * 2.0f);
            g.setColour (edge());
            g.drawEllipse (c.x - radius, c.y - radius, radius * 2.0f, radius * 2.0f, 1.2f);
            const float a = startAngle + pos * (endAngle - startAngle);
            juce::Path p;
            p.startNewSubPath (0.0f, -radius * 0.80f);
            p.lineTo (0.0f, -radius * 0.28f);
            g.setColour (ink());
            g.strokePath (p, juce::PathStrokeType (2.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded),
                          juce::AffineTransform::rotation (a).translated (c.x, c.y));
        }

        // Lock buttons: a round button, lit when locked.
        void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool, bool down) override
        {
            const auto r = b.getLocalBounds().toFloat().reduced (1.5f);
            const bool on = b.getToggleState();
            g.setColour (on ? juce::Colour (0xffffeeb0) : paper());
            g.fillEllipse (r);
            g.setColour (on ? juce::Colour (0xffb98a00) : edge());
            g.drawEllipse (r, down ? 2.4f : 1.6f);
        }
    };

    // A 2 or 3 position switch bound to a choice or bool parameter. A click on a 2 position switch toggles it; on a
    // 3 position switch the third of the switch that is clicked is selected. "reversed" draws parameter index 0 on the right.
    class SlideSwitch : public juce::Component
    {
    public:
        SlideSwitch (juce::RangedAudioParameter& p, int numPositions, bool reversed)
            : n (numPositions), rev (reversed),
              attach (p, [this] (float v) { index = juce::jlimit (0, n - 1, juce::roundToInt (v)); repaint(); })
        {
            attach.sendInitialUpdate();
        }

        void paint (juce::Graphics& g) override
        {
            const auto r = getLocalBounds().toFloat().reduced (1.0f);
            g.setColour (paper());
            g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
            g.setColour (edge());
            g.drawRoundedRectangle (r, r.getHeight() * 0.5f, 1.2f);
            const int vis = rev ? (n - 1 - index) : index;
            const float d = r.getHeight() - 6.0f;
            const float x0 = r.getX() + 3.0f;
            const float span = r.getWidth() - 6.0f - d;
            const float x = x0 + (n > 1 ? span * (float) vis / (float) (n - 1) : 0.0f);
            g.setColour (juce::Colour (0xff4a4a47));
            g.fillEllipse (x, r.getY() + 3.0f, d, d);
        }

        void mouseDown (const juce::MouseEvent& e) override
        {
            int newIndex = index;
            if (n == 2)
                newIndex = 1 - index;
            else
            {
                const int vis = juce::jlimit (0, n - 1, (int) (e.position.x / ((float) getWidth() / (float) n)));
                newIndex = rev ? (n - 1 - vis) : vis;
            }
            attach.setValueAsCompleteGesture ((float) newIndex);
        }

    private:
        int n;
        bool rev;
        int index = 0;
        juce::ParameterAttachment attach;
    };

    // Fires a trigger parameter the way a patch cable would: the value goes to 1, stays there long enough for the audio
    // side to see it (the processor reads parameters once per host block, and the core acts on the rising edge, see
    // ParamMap.h ParamApplier::apply), then goes back to 0. Steps asked for while a pulse is running wait their turn.
    class Pulser : private juce::Timer
    {
    public:
        explicit Pulser (juce::RangedAudioParameter& p) : attach (p, [] (float) {}) {}
        ~Pulser() override { stopTimer(); }

        void request()
        {
            if (pending < kMaxPending) ++pending;
            if (! isTimerRunning())
            {
                tick();
                startTimer (kHalfPeriodMs);
            }
        }

    private:
        static constexpr int kHalfPeriodMs = 60;
        static constexpr int kMaxPending = 8;

        void timerCallback() override { tick(); }

        void tick()
        {
            if (high)
            {
                attach.setValueAsCompleteGesture (0.0f);
                high = false;
                if (pending == 0) stopTimer();
            }
            else if (pending > 0)
            {
                --pending;
                attach.setValueAsCompleteGesture (1.0f);
                high = true;
            }
            else
                stopTimer();
        }

        juce::ParameterAttachment attach;
        int pending = 0;
        bool high = false;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Pulser)
    };

    // A button that fires one trigger parameter. glyph: 0 none, 1 left, 2 right, 3 up, 4 down (a small triangle).
    class PulseButton : public juce::Button
    {
    public:
        PulseButton (const juce::String& text, juce::RangedAudioParameter& p, int glyphKind, bool dashedOutline)
            : juce::Button (text), pulser (p), glyph (glyphKind), dashed (dashedOutline)
        {
            onClick = [this] { pulser.request(); };
        }

        void paintButton (juce::Graphics& g, bool, bool down) override
        {
            const auto r = getLocalBounds().toFloat().reduced (1.0f);
            g.setColour (down ? juce::Colour (0xffe9e9e4) : paper());
            g.fillRoundedRectangle (r, 6.0f);
            g.setColour (edge());
            if (dashed) dashedRoundedRect (g, r, 6.0f);
            else g.drawRoundedRectangle (r, 6.0f, 1.2f);

            g.setColour (softInk());
            g.setFont (15.0f);
            const auto text = getButtonText();
            const float tw = g.getCurrentFont().getStringWidthFloat (text);
            const float gw = glyph != 0 ? 12.0f : 0.0f;
            const float gap = glyph != 0 ? 6.0f : 0.0f;
            const float total = tw + gw + gap;
            float x = r.getCentreX() - total * 0.5f;
            const float cy = r.getCentreY();
            auto drawTriangle = [&] (float gx)
            {
                juce::Path t;
                if (glyph == 1)      t.addTriangle (gx + 10.0f, cy - 5.0f, gx + 10.0f, cy + 5.0f, gx, cy);
                else if (glyph == 2) t.addTriangle (gx, cy - 5.0f, gx, cy + 5.0f, gx + 10.0f, cy);
                else if (glyph == 3) t.addTriangle (gx, cy + 4.0f, gx + 10.0f, cy + 4.0f, gx + 5.0f, cy - 5.0f);
                else if (glyph == 4) t.addTriangle (gx, cy - 4.0f, gx + 10.0f, cy - 4.0f, gx + 5.0f, cy + 5.0f);
                g.fillPath (t);
            };
            if (glyph == 1 || glyph == 3 || glyph == 4)
            {
                drawTriangle (x);
                x += gw + gap;
            }
            g.drawText (text, juce::Rectangle<float> (x, r.getY(), tw + 2.0f, r.getHeight()), juce::Justification::centredLeft, false);
            if (glyph == 2)
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
    class RotateDial : public juce::Component
    {
    public:
        RotateDial (juce::RangedAudioParameter& cw, juce::RangedAudioParameter& ccw) : pulseCw (cw), pulseCcw (ccw) {}

        void paint (juce::Graphics& g) override
        {
            const auto r = getLocalBounds().toFloat().reduced (2.0f);
            const float radius = juce::jmin (r.getWidth(), r.getHeight()) * 0.5f;
            const auto c = r.getCentre();
            g.setColour (paper());
            g.fillEllipse (c.x - radius, c.y - radius, radius * 2.0f, radius * 2.0f);
            g.setColour (edge());
            g.drawEllipse (c.x - radius, c.y - radius, radius * 2.0f, radius * 2.0f, 1.2f);
            juce::Path p;
            p.startNewSubPath (0.0f, -radius * 0.82f);
            p.lineTo (0.0f, -radius * 0.30f);
            g.setColour (ink());
            g.strokePath (p, juce::PathStrokeType (2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded),
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
