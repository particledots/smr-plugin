// src/plugin: colours, fonts, geometry and text helpers of the window ("gold sections" look, 1.0; same look as the Anamnesis
// plugin 0.3.1). Header only, JUCE only, no core code. Look only: nothing here touches a parameter.
//
// The window is laid out on a fixed 1000 x 824 canvas and scaled to the host's window. Fixed artwork (frames, titles,
// labels, knob arcs, slider tracks) is drawn once into a stored picture; 30 times a second only the live areas are
// redrawn (liveAreas() in PluginEditor.cpp).
#pragma once

#include <vector>

#include <juce_gui_basics/juce_gui_basics.h>

#include <SmrFontData.h>

namespace smrui
{
    // ------------------------------------------------------------------------------------------ colours
    inline juce::Colour panel()     { return juce::Colour (0xFF0B0B0C); }   // black panel (also behind the rounded corners)
    inline juce::Colour gold()      { return juce::Colour (0xFFB08D57); }   // frames, lines, outlines
    inline juce::Colour goldText()  { return juce::Colour (0xFFD4B27A); }   // the title, frame and row titles
    inline juce::Colour text()      { return juce::Colour (0xFFE8E1D0); }   // labels, chosen settings
    inline juce::Colour soft()      { return juce::Colour (0xFFB5AC98); }   // labels of switches that are off
    inline juce::Colour muted()     { return juce::Colour (0xFFA39886); }   // values, choices not chosen
    inline juce::Colour faint()     { return juce::Colour (0xFF5E574B); }
    inline juce::Colour lamp()      { return juce::Colour (0xFF4C9BFF); }   // blue lamps: on or chosen (the only colour besides the module's lights)
    inline juce::Colour lampFill()  { return juce::Colour (0xFF1B3A66); }   // a lit round button
    inline juce::Colour skirt()     { return juce::Colour (0xFF121212); }   // knob skirts, round buttons
    inline juce::Colour skirtLine() { return juce::Colour (0xFF2E2A22); }
    inline juce::Colour cap()       { return juce::Colour (0xFFE9E2D2); }   // knob caps, slider thumbs
    inline juce::Colour capLine()   { return juce::Colour (0xFFBFB6A2); }
    inline juce::Colour pointer()   { return juce::Colour (0xFF161514); }
    inline juce::Colour menu()      { return juce::Colour (0xFF16161A); }   // pop-up menus
    inline juce::Colour clipLamp()  { return juce::Colour (0xFFFFD21F); }   // the input clip light and an over-0 dB meter (as on the module)

    // ------------------------------------------------------------------------------------------ geometry (canvas units)
    inline constexpr int canvasW = 1000, canvasH = 824;

    // The title is centred at x = 500 and must stay between these two x positions at every window size (inside the two gold
    // ornament lines; checked in the drawn pixels by the panel checks).
    inline constexpr float titleLeft = 330.0f, titleRight = 670.0f, titleSize = 15.0f, titleBaseline = 34.0f, titleSpacing = 3.5f;
    // The title text. 15 pt (was 28 pt for "UNOFFICIAL SMR"): at 28 pt this name would need about 595 of the 340 units.
    // Baseline 34 keeps the capitals centred on the ornament lines (y 28.6).
    inline constexpr const char* titleText = "SPECTRAL HARMONIC ROTATOR";
    inline constexpr float ornamentLeftEnd = 318.0f, ornamentRightStart = 682.0f;

    inline constexpr int stripX[6] = { 280, 380, 480, 580, 680, 780 };       // the six channels
    inline constexpr int lockY = 100, envY = 360;
    inline constexpr int sliderTop = 121, sliderHeight = 186;                // slider component rows (the track is drawn from the slider's own range)
    inline constexpr int oddX = 96, evenX = 905;                             // centres of the two side columns
    inline constexpr float ringCx = 500.0f, ringCy = 524.0f, ringRx = 160.0f, ringRy = 92.0f;
    inline constexpr int clipX = 37, clipY = 486;
    inline constexpr int meterW = 98, meterH = 6;
    inline constexpr int meterX[4] = { 64, 64, 864, 864 }, meterY[4] = { 432, 452, 588, 608 };
    inline constexpr int knobBox = 62;                                       // knob component size; the gold range arc is drawn on its edge

    // ------------------------------------------------------------------------------------------ fonts
    // Space Grotesk (text) and Marcellus (title) are built in, so every Mac and the checks measure the same.
    namespace detail
    {
        struct Faces
        {
            juce::Typeface::Ptr regular = juce::Typeface::createSystemTypefaceFor (SmrFontData::SpaceGroteskRegular_ttf, (size_t) SmrFontData::SpaceGroteskRegular_ttfSize);
            juce::Typeface::Ptr medium  = juce::Typeface::createSystemTypefaceFor (SmrFontData::SpaceGroteskMedium_ttf, (size_t) SmrFontData::SpaceGroteskMedium_ttfSize);
            juce::Typeface::Ptr title   = juce::Typeface::createSystemTypefaceFor (SmrFontData::MarcellusRegular_ttf, (size_t) SmrFontData::MarcellusRegular_ttfSize);
        };
        inline Faces& faces() { static Faces f; return f; }

        inline juce::Font make (juce::Typeface::Ptr face, float size, float spacing)
        {
            // If a font could not be read for any reason, fall back to the system font rather than draw nothing.
            juce::FontOptions o = face != nullptr ? juce::FontOptions (face) : juce::FontOptions();
            juce::Font f (o.withPointHeight (size));
            if (spacing > 0.0f)
                f = f.withExtraKerningFactor (spacing / f.getHeight());
            return f;
        }
    }

    inline juce::Font font (float size, bool medium = false, float spacing = 0.0f)
    {
        return detail::make (medium ? detail::faces().medium : detail::faces().regular, size, spacing);
    }
    inline juce::Font titleFont() { return detail::make (detail::faces().title, titleSize, titleSpacing); }

    inline float widthOf (const juce::String& s, const juce::Font& f)
    {
        juce::GlyphArrangement ga;
        ga.addLineOfText (f, s, 0.0f, 0.0f);
        return ga.getBoundingBox (0, -1, true).getWidth();
    }
    inline float textWidth (const juce::String& s, float size, float spacing = 0.0f) { return widthOf (s, font (size, false, spacing)); }
    inline float titleWidth() { return widthOf (titleText, titleFont()); }

    // The sizes used for the window's text (canvas units).
    inline constexpr float labelSize = 12.0f, labelSpacing = 1.4f;       // switch labels, buttons
    inline constexpr float capsSize = 12.0f, capsSpacing = 2.0f;         // knob names
    inline constexpr float rowSize = 11.5f, rowSpacing = 1.5f;           // row titles ("MODE", "BANK", ...)
    inline constexpr float frameTitleSize = 12.0f, frameTitleSpacing = 2.4f;

    // ------------------------------------------------------------------------------------------ text-fit log
    // The panel checks switch this on, draw a fresh window once, and read what every text needed against the room it has.
    struct TextFit { juce::String text; float width = 0.0f, room = 0.0f; };
    inline std::vector<TextFit>*& fitLog() { static std::vector<TextFit>* p = nullptr; return p; }
    inline void logFit (const juce::String& s, float width, float room)
    {
        if (auto* l = fitLog()) l->push_back ({ s, width, room });
    }

    // Text with y as its baseline, like the draft's SVG text. room (> 0) is how wide the text may be; it is logged for the checks.
    // Returns the text's width only while the checks are logging (otherwise 0: the layout is not measured twice).
    inline float textAt (juce::Graphics& g, const juce::String& s, float x, float y, juce::Colour c, float size = labelSize,
                         juce::Justification just = juce::Justification::centred, float spacing = 0.0f, float room = 0.0f,
                         bool medium = false)
    {
        const juce::Font f = font (size, medium, spacing);
        const float w = (room > 0.0f && fitLog() != nullptr) ? widthOf (s, f) : 0.0f;    // measured only while the checks are logging
        if (w > 0.0f) logFit (s, w, room);
        g.setColour (c);
        g.setFont (f);
        const float asc = f.getAscent(), h = f.getHeight(), box = 700.0f;
        juce::Rectangle<float> r;
        if (just == juce::Justification::centredLeft)       r = { x, y - asc, box, h };
        else if (just == juce::Justification::centredRight) r = { x - box, y - asc, box, h };
        else                                                 r = { x - box * 0.5f, y - asc, box, h };
        g.drawText (s, r, just, false);
        return w;
    }

    // ------------------------------------------------------------------------------------------ small drawings
    // A lamp: lit = blue with a soft halo; off = a thin ring.
    inline void drawLamp (juce::Graphics& g, float cx, float cy, bool lit, float r = 3.6f, juce::Colour col = lamp())
    {
        if (lit)
        {
            g.setColour (col.withAlpha (0.22f));
            g.fillEllipse (cx - r * 2.2f, cy - r * 2.2f, r * 4.4f, r * 4.4f);
            g.setColour (col);
            g.fillEllipse (cx - r, cy - r, r * 2.0f, r * 2.0f);
        }
        else
        {
            g.setColour (muted());
            g.drawEllipse (cx - r + 0.3f, cy - r + 0.3f, r * 2.0f - 0.6f, r * 2.0f - 0.6f, 0.9f);
        }
    }

    inline void dashedRoundedRect (juce::Graphics& g, juce::Rectangle<float> r, float corner, float w = 0.8f)
    {
        juce::Path p;
        p.addRoundedRectangle (r, corner);
        juce::Path dashed;
        const float dashes[] = { 4.0f, 3.0f };
        juce::PathStrokeType (w).createDashedStroke (dashed, p, dashes, 2);
        g.setColour (gold());
        g.fillPath (dashed);
    }

    // A path with softly rounded corners through the given points.
    inline juce::Path run (std::initializer_list<juce::Point<float>> pts)
    {
        juce::Path p;
        std::vector<juce::Point<float>> v (pts);
        p.startNewSubPath (v[0]);
        for (size_t i = 1; i < v.size(); ++i) p.lineTo (v[i]);
        return p.createPathWithRoundedCorners (4.0f);
    }

    inline void strokeGold (juce::Graphics& g, const juce::Path& p, float w = 0.9f)
    {
        g.setColour (gold());
        g.strokePath (p, juce::PathStrokeType (w));
    }

    // A flow-line end: a small ring on the panel colour.
    inline void node (juce::Graphics& g, float x, float y)
    {
        g.setColour (panel());
        g.fillEllipse (x - 2.6f, y - 2.6f, 5.2f, 5.2f);
        g.setColour (gold());
        g.drawEllipse (x - 2.6f, y - 2.6f, 5.2f, 5.2f, 0.9f);
    }
    inline void dotAt (juce::Graphics& g, float x, float y, float r = 2.0f)
    {
        g.setColour (gold());
        g.fillEllipse (x - r, y - r, r * 2.0f, r * 2.0f);
    }

    // The module's lights (ring, Env Out): brightness is the firmware's 12 bit duty value drawn on a dark lens (see PluginEditor.cpp).
    // A lit light gets a soft halo of its own colour; an unlit one is a dark lens with a thin ring.
    inline void drawLight (juce::Graphics& g, float cx, float cy, juce::Colour c, bool lit, float r)
    {
        if (lit)
        {
            g.setColour (c.withAlpha (0.22f));
            g.fillEllipse (cx - r * 2.0f, cy - r * 2.0f, r * 4.0f, r * 4.0f);
            g.setColour (c);
            g.fillEllipse (cx - r, cy - r, r * 2.0f, r * 2.0f);
        }
        else
        {
            const float q = r * 0.75f;
            g.setColour (juce::Colour (0xFF1A1A1A));
            g.fillEllipse (cx - q, cy - q, q * 2.0f, q * 2.0f);
            g.setColour (faint());
            g.drawEllipse (cx - q, cy - q, q * 2.0f, q * 2.0f, 0.9f);
        }
    }
}
