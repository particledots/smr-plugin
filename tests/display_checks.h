// Milestone 6b: checks of src/plugin/DisplayModel.h that need no JUCE. Included by tests/plugin_tests.cpp (run in CI as part of
// "smr_plugin_tests selftest"). The expected values are worked out by hand from third_party/SMR/led_ring.c
// (display_filter_rotation, calculate_envout_leds), leds.c and filter.c, not produced by running the code under test.
#pragma once

#include <cstdio>
#include <cstring>
#include <functional>
#include <memory>
#include <string>

#include "DisplayModel.h"

namespace display_checks
{
    using Check = std::function<void (bool, const std::string&, const std::string&)>;

    inline std::string rgb (const uint16_t* v)
    {
        char b[64];
        std::snprintf (b, sizeof b, "{%u,%u,%u}", (unsigned) v[0], (unsigned) v[1], (unsigned) v[2]);
        return b;
    }

    inline bool is (const uint16_t* v, int r, int g, int b) { return v[0] == r && v[1] == g && v[2] == b; }

    inline smr_plugin::DisplayData base()
    {
        smr_plugin::DisplayData d;
        d.valid = 1;
        for (int c = 0; c < 6; ++c)
        {
            d.note[c] = 5 + c;          // the port's start-up notes 5..10
            d.fadeto[c] = 5 + c;
            d.level[c] = 1.0f;
        }
        return d;
    }

    inline void run (const Check& check)
    {
        using namespace smr_plugin;
        uint16_t ring[kRingLeds][3];
        uint16_t env[6][3];

        // Default colours at full level, nothing morphing: one LED per channel in its own colour (DEFAULT_COLOR_CH[0]).
        {
            auto d = base();
            ringLeds (d, false, ring);
            bool ok = is (ring[5], 0, 0, 761) && is (ring[6], 0, 770, 766) && is (ring[7], 0, 766, 16)
                      && is (ring[8], 700, 700, 700) && is (ring[9], 763, 154, 0) && is (ring[10], 766, 0, 112);
            for (int i = 0; i < kRingLeds; ++i)
                if (i < 5 || i > 10) ok = ok && is (ring[i], 0, 0, 0);
            check (ok, "ring: six channels at notes 5 to 10, full level, show their led_ring.c colours and the other 14 LEDs are off", rgb (ring[5]) + " " + rgb (ring[10]));
        }

        // Level floor: t_f = max (level, 0.05). 761 x 0.05 = 38.05 truncates to 38.
        {
            auto d = base();
            d.level[0] = 0.0f;
            ringLeds (d, false, ring);
            check (is (ring[5], 0, 0, 38), "ring: level 0 draws at the 5 % floor (761 x 0.05 = 38)", rgb (ring[5]));
        }

        // Morph halfway from note 5 to note 6 on channel 1: channel 1 shares LED 6 with channel 2.
        // LED 5: 761 x 0.5 = 380.5 -> 380. LED 6 is written first by channel 1's fade (0,0,380), then channel 2 adds (0,770,766):
        // (0,770,1146) clamps to 1023 in blue.
        {
            auto d = base();
            d.fadeto[0] = 6;
            d.morph[0] = 0.5f;
            ringLeds (d, false, ring);
            check (is (ring[5], 0, 0, 380) && is (ring[6], 0, 770, 1023),
                   "ring: a morphing channel splits its colour between two LEDs, sums add and clamp to 1023", rgb (ring[5]) + " " + rgb (ring[6]));
        }

        // Locked channel: shown only while the flash is on, at t_f.
        {
            auto d = base();
            d.lock[1] = 1;
            ringLeds (d, true, ring);
            const bool on = is (ring[6], 0, 770, 766);
            ringLeds (d, false, ring);
            const bool off = is (ring[6], 0, 0, 0) && is (ring[5], 0, 0, 761);
            check (on && off, "ring: a locked channel's LED is on in one flash phase and off in the other; others are steady", "");
        }

        // Blocked note shows 75,75,75 and hides any channel on it (the firmware's if / else if).
        {
            auto d = base();
            d.freqblock = (1u << 5) | (1u << 12);
            ringLeds (d, false, ring);
            check (is (ring[5], 75, 75, 75) && is (ring[12], 75, 75, 75) && is (ring[6], 0, 770, 766),
                   "ring: blocked notes show 75,75,75, even under a channel", rgb (ring[5]));
        }

        // Two channels on one note: only the first (lowest index) is drawn there (chan = 6 break).
        {
            auto d = base();
            d.note[1] = 5;
            d.fadeto[1] = 5;
            ringLeds (d, false, ring);
            check (is (ring[5], 0, 0, 761) && is (ring[6], 0, 0, 0), "ring: two channels on one note show only the lower channel", rgb (ring[5]));
        }

        // Env Out lights, play mode: colour x ENVOUT_PWM / 4096. Channel 1 at 4095: 761 x 0.99976 = 760.8 -> 760.
        // Channel 2 at 2048: 770 x 0.5 = 385 and 766 x 0.5 = 383.
        {
            auto d = base();
            d.envPwm[0] = 4095; d.envPwm[1] = 2048; d.envPwm[2] = 0;
            d.envMode = 0;
            envOutLeds (d, env);
            check (is (env[0], 0, 0, 760) && is (env[1], 0, 385, 383) && is (env[2], 0, 0, 0),
                   "env out: lights are the channel colour x ENVOUT_PWM / 4096 (4095, 2048 and 0)", rgb (env[0]) + " " + rgb (env[1]));
        }

        // 1V/oct mode: same, with a floor of 5 % of the colour (dimamt). 761 x 0.05 = 38.05 -> 38.
        {
            auto d = base();
            d.envPwm[0] = 0;
            d.envMode = (uint32_t) smr::ENV_VOLTOCT;
            envOutLeds (d, env);
            check (is (env[0], 0, 0, 38), "env out: in 1V/oct mode the light never goes below 5 % of its colour", rgb (env[0]));
        }

        // Slider LED duty: level / 4.
        check (sliderLedDuty (1.0f) == 0.25f && sliderLedDuty (0.0f) == 0.0f && sliderLedDuty (-1.0f) == 0.0f && sliderLedDuty (8.0f) == 1.0f,
               "slider LED: duty is channel_level / 4, 25 % at level 1.0", "");

        // Slider clip: (blend of filter_out[j][0] and filter_out[j+6][0]) x channel_level against 0x50000000 = 1.342e9.
        // Channel 1: 1.5e9 x 1.0 clips. Channel 2: 1.0e9 does not. Channel 3: 1.5e9 x 0.5 = 0.75e9 does not.
        // Channel 4 at morph 1 uses the second filter, 2e9, and clips. Channel 5 at morph 1 has a silent second filter and does not.
        // Channel 6 is -2e9: the firmware compares without an absolute value, so it does not.
        {
            auto e = std::make_unique<smr::Engine>();
            e->filter_out[0][0] = 1.5e9f;  e->channel_level[0] = 1.0f;
            e->filter_out[1][0] = 1.0e9f;  e->channel_level[1] = 1.0f;
            e->filter_out[2][0] = 1.5e9f;  e->channel_level[2] = 0.5f;
            e->filter_out[3][0] = 0.0f;    e->filter_out[3 + 6][0] = 2.0e9f; e->motion_morphpos[3] = 1.0f; e->channel_level[3] = 1.0f;
            e->filter_out[4][0] = 2.0e9f;  e->filter_out[4 + 6][0] = 0.0f;   e->motion_morphpos[4] = 1.0f; e->channel_level[4] = 1.0f;
            e->filter_out[5][0] = -2.0e9f; e->channel_level[5] = 1.0f;
            const uint32_t bits = sliderClipBits (*e);
            check (bits == ((1u << 0) | (1u << 3)),
                   "slider clip: channels 1 and 4 clip; 1e9, 0.75e9, a silent second filter and a negative sample do not",
                   "bits 0x" + std::to_string (bits));
        }

        // The hub: a published copy comes back whole; accumulators add and clear; before the first publish nothing is valid.
        {
            DisplayHub hub;
            DisplayData r;
            const bool gotEmpty = hub.read (r) && r.valid == 0;
            auto d = base();
            d.envPwm[3] = 1234; d.morph[2] = 0.25f; d.lock[4] = 1; d.freqblock = 0x80001;
            hub.publish (d);
            DisplayData r2;
            const bool got = hub.read (r2);
            const bool same = got && std::memcmp (&d, &r2, sizeof d) == 0;
            hub.addClipBits (1u << 2);
            hub.addClipBits (kInputClipBit);
            const uint32_t bits = hub.takeClipBits();
            const uint32_t again = hub.takeClipBits();
            hub.notePeak (0, 0.5f); hub.notePeak (0, 0.2f); hub.notePeak (1, 0.1f);
            const float p0 = hub.takePeak (0), p0b = hub.takePeak (0), p1 = hub.takePeak (1);
            check (gotEmpty && same && bits == ((1u << 2) | kInputClipBit) && again == 0 && p0 == 0.5f && p0b == 0.0f && p1 == 0.1f,
                   "display hub: empty before the first publish, copy comes back identical, clip bits and peaks accumulate and clear", "");
        }
    }
}
