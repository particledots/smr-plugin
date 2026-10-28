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

        // ---- Milestone 7 step 3b. Expected values worked out by hand from led_ring.c calculate_envout_leds, leds.c update_lock_leds and
        // params.c (not from running the code under test).
        auto isBlue = [] (const uint16_t* v) { return v[0] == 0 && v[1] == 100 && v[2] == 1023; };
        auto isRed = [] (const uint16_t* v) { return v[0] == 1023 && v[1] == 0 && v[2] == 0; };
        auto isOff = [] (const uint16_t* v) { return v[0] == 0 && v[1] == 0 && v[2] == 0; };
        {
            // COARSE TUNING, no transposition (cur_envled_state 12 = 0b0001100): channels 1 to 3 red, 4 to 6 blue.
            auto d = base();
            EnvOutLedState st;
            d.ongoingCoarse[0] = 1; d.curEnvled = 12;
            const bool took = envOutLedsFw (d, st, env);
            bool ok = took;
            for (int i = 0; i < 3; ++i) ok = ok && isRed (env[i]);
            for (int i = 3; i < 6; ++i) ok = ok && isBlue (env[i]);
            check (ok, "coarse tuning, state 12 (no transposition): lights 1 to 3 red, 4 to 6 blue", rgb (env[0]) + " " + rgb (env[3]));
        }
        {
            // +1 semitone = 0b0100000 (32): bit 5 is light 1, positive so blue. -1 = 0b1000001 (65 > 64): bit 0 is light 6, red.
            auto d = base();
            EnvOutLedState st;
            d.ongoingCoarse[1] = 1; d.curEnvled = 0b0100000;
            bool ok = envOutLedsFw (d, st, env) && isBlue (env[0]);
            for (int i = 1; i < 6; ++i) ok = ok && isOff (env[i]);
            check (ok, "coarse tuning, +1 semitone (state 0b0100000): light 1 blue, the others off", rgb (env[0]) + " " + rgb (env[1]));
            d.curEnvled = 0b1000001;
            ok = envOutLedsFw (d, st, env) && isRed (env[5]);
            for (int i = 0; i < 5; ++i) ok = ok && isOff (env[i]);
            check (ok, "coarse tuning, -1 semitone (state 0b1000001): light 6 red, the others off", rgb (env[5]) + " " + rgb (env[0]));
            d.curEnvled = 0b1111111;
            ok = envOutLedsFw (d, st, env);
            for (int i = 0; i < 6; ++i) ok = ok && isRed (env[i]);
            check (ok, "coarse tuning, -6 semitones (state 0b1111111): all six lights red", rgb (env[0]));
            d.curEnvled = 0b0111111;
            ok = envOutLedsFw (d, st, env);
            for (int i = 0; i < 6; ++i) ok = ok && isBlue (env[i]);
            check (ok, "coarse tuning, +6 semitones (state 0b0111111): all six lights blue", rgb (env[0]));
        }
        {
            // The COARSE branch comes first, BpRe skips both branches, and with nothing ongoing the function gives the play branch back.
            auto d = base();
            EnvOutLedState st;
            d.ongoingCoarse[0] = 1; d.ongoingFine[0] = 1; d.fineEnvled = 0b101010; d.curEnvled = 12;
            envOutLedsFw (d, st, env);
            const bool coarseFirst = isRed (env[0]);
            d.isBpre = 1;
            const bool bpre = ! envOutLedsFw (d, st, env);
            d.isBpre = 0; d.ongoingCoarse[0] = 0; d.ongoingFine[0] = 0;
            const bool idle = ! envOutLedsFw (d, st, env);
            d.fineTimeout[1] = 1;
            const bool timeoutKeeps = envOutLedsFw (d, st, env);
            check (coarseFirst && bpre && idle && timeoutKeeps,
                   "env lights: coarse before fine, BpRe skips both, nothing ongoing leaves the play branch, a running timeout keeps the fine branch", "");
        }
        {
            // FINE TUNING, odds channel 1 adjusted, nothing locked, switch at 135, no nudge (bright 0). fine_flash starts at 1:
            // call 1: not > 1, not < -1; enabled and not locked: fine_flash > 0 so it becomes -5; (-5 & 1) = 1 so bright {1023,1023,1023}.
            // call 2: -5 < -1 so -4: even so dim {75,75,75}. call 3: -3 bright. call 4: -2 dim. call 5: -1 (not < -1) bright and it stays.
            auto d = base();
            EnvOutLedState st;
            d.ongoingFine[0] = 1; d.fineEnvled = 0b101010;
            for (int i = 0; i < 6; ++i) d.freqNudge[i] = 1.0f;
            int seq[6] = {};
            bool offOthers = true;
            for (int call = 0; call < 6; ++call)
            {
                const bool took = envOutLedsFw (d, st, env);
                seq[call] = ! took ? -1 : (is (env[0], 1023, 1023, 1023) ? 1 : (is (env[0], 75, 75, 75) ? 0 : 9));
                offOthers = offOthers && isOff (env[1]) && isOff (env[3]) && isOff (env[5]);
            }
            check (seq[0] == 1 && seq[1] == 0 && seq[2] == 1 && seq[3] == 0 && seq[4] == 1 && seq[5] == 1 && offOthers,
                   "fine tuning: an enabled channel blinks twice (bright, dim, bright, dim) then stays bright; channels not adjusted are off",
                   std::to_string (seq[0]) + std::to_string (seq[1]) + std::to_string (seq[2]) + std::to_string (seq[3]) + std::to_string (seq[4]) + std::to_string (seq[5]));
        }
        {
            // A channel the switch or a Lock has disabled: fine_flash starts at 1 and stays: odd, so dim {75,75,75}, steady.
            // One that was enabled before (fine_flash -1) restarts at 5 when it gets disabled: 5 dim, 4 bright, 3 dim, 2 bright, 1 dim and it stays.
            auto d = base();
            EnvOutLedState st;
            d.ongoingFine[0] = 1; d.fineEnvled = 0b101010; d.lock[2] = 1; d.mod135One = 1;   // channel 3 locked, and channels 3 and 5 off the pot by the switch
            for (int i = 0; i < 6; ++i) d.freqNudge[i] = 1.0f;
            envOutLedsFw (d, st, env);
            const bool steadyDim = is (env[2], 75, 75, 75) && is (env[4], 75, 75, 75);
            envOutLedsFw (d, st, env);
            const bool steadyDim2 = is (env[2], 75, 75, 75) && is (env[4], 75, 75, 75);
            EnvOutLedState st2;
            st2.fineFlash[2] = -1;
            int seq[5] = {};
            for (int call = 0; call < 5; ++call)
            {
                envOutLedsFw (d, st2, env);
                seq[call] = is (env[2], 1023, 1023, 1023) ? 1 : (is (env[2], 75, 75, 75) ? 0 : 9);
            }
            check (steadyDim && steadyDim2 && seq[0] == 0 && seq[1] == 1 && seq[2] == 0 && seq[3] == 1 && seq[4] == 0,
                   "fine tuning: a locked or switched-off channel stays dim; one that was enabled blinks twice (dim, bright, dim, bright) and goes dim",
                   std::to_string (seq[0]) + std::to_string (seq[1]) + std::to_string (seq[2]) + std::to_string (seq[3]) + std::to_string (seq[4]));
        }
        {
            // Brightness: (freq_nudge / coarse_adj - 1) * 14.3. With freq_nudge 1.017483 and coarse_adj 1: 0.25, so 1023 * 0.75 = 767.25 -> 767,
            // and in the dim phase 75 * 0.75 = 56.25 -> 56. With coarse_adj 1.05946 and freq_nudge the same times it: the ratio is the same.
            auto d = base();
            EnvOutLedState st;
            d.ongoingFine[1] = 1; d.fineEnvled = 0b010101;
            for (int i = 0; i < 6; ++i) { d.freqNudge[i] = 1.017483f; d.coarseAdj[i] = 1.0f; }
            envOutLedsFw (d, st, env);
            const bool bright = is (env[1], 767, 767, 1023);
            envOutLedsFw (d, st, env);
            const bool dim = is (env[1], 56, 56, 75);
            EnvOutLedState st3;
            for (int i = 0; i < 6; ++i) { d.freqNudge[i] = 1.017483f * 1.05946309436f; d.coarseAdj[i] = 1.05946309436f; }
            envOutLedsFw (d, st3, env);
            const bool transposed = is (env[1], 767, 767, 1023);
            check (bright && dim && transposed, "fine tuning: brightness follows freq_nudge / coarse_adj, so a transposed channel shows the same fine tune",
                   rgb (env[1]));
            // Past 1 / 14.3 of nudge (about 94 % of the knob) 1 - brightness is negative: undefined in the firmware (docs/open-questions.md M7-h).
            // Provisional: the red and green parts saturate at 0, so the light stays blue. This pins that choice, it does not confirm it.
            EnvOutLedState st4;
            for (int i = 0; i < 6; ++i) { d.freqNudge[i] = 1.0745f; d.coarseAdj[i] = 1.0f; }
            envOutLedsFw (d, st4, env);
            check (is (env[1], 0, 0, 1023), "fine tuning (M7-h, provisional): above 94 % of the knob the red and green parts are 0 and the light is blue", rgb (env[1]));
        }
        {
            // update_lock_leds: a Q-locked channel inverts at the 16th update (flash1 counts 1, 2, ... 15, then 16 wraps to 0).
            auto d = base();
            LockLedState st;
            d.qlocked[2] = 1;
            int firstInv = -1, invCount = 0;
            for (int u = 1; u <= 32; ++u)
            {
                lockLedUpdate (d, st);
                if (st.inverted[2]) { ++invCount; if (firstInv < 0) firstInv = u; }
                if (st.inverted[0] || st.inverted[1] || st.inverted[3]) invCount += 100;
            }
            check (firstInv == 16 && invCount == 2, "lock LEDs: a Q-locked channel inverts at update 16 and 32 only, others never", std::to_string (firstInv) + " " + std::to_string (invCount));
        }
        {
            // A staged clear: flash2 counts 1..4, the 5th update wraps it to 0, and only channels with coarse_adj_led != 1.0 invert.
            // While it lasts the Q-lock flicker is replaced (the second block decides every LED again).
            auto d = base();
            LockLedState st;
            d.numClearStaged = 6;
            for (int i = 0; i < 6; ++i) d.coarseAdjLed[i] = 1.0f;
            d.coarseAdjLed[1] = 1.05946309436f; d.coarseAdjLed[4] = 0.8909f;
            d.qlocked[0] = 1;
            bool ok = true;
            int firstInv = -1;
            for (int u = 1; u <= 10; ++u)
            {
                lockLedUpdate (d, st);
                const bool want = (u % 5) == 0;
                ok = ok && (st.inverted[1] == want) && (st.inverted[4] == want) && ! st.inverted[0] && ! st.inverted[2] && ! st.inverted[3] && ! st.inverted[5];
                if (st.inverted[1] && firstInv < 0) firstInv = u;
            }
            check (ok && firstInv == 5, "lock LEDs: while a clear is staged the transposed channels flash every 5th update, the others and the Q-lock flicker do not", std::to_string (firstInv));
        }
        {
            // fillDisplayData copies the new members from the engine.
            auto e = std::make_unique<smr::Engine>();
            e->ongoing_fine_tuning[1] = 1; e->ongoing_coarse_tuning[0] = 1; e->fine_tuning_timeout[1] = 777;
            e->cur_envled_state = 0b0111000; e->fine_envled = 0b010101; e->num_clear_coarse_staged = 3;
            e->freq_nudge[4] = 1.03f; e->coarse_adj[4] = 1.0595f; e->coarse_adj_led[4] = 1.25f;
            e->mod_mode_135 = 1; e->mod_mode_246 = 246; e->filter_type = smr::BPRE;
            DisplayData d;
            fillDisplayData (*e, d);
            check (d.ongoingFine[1] == 1 && d.ongoingFine[0] == 0 && d.ongoingCoarse[0] == 1 && d.fineTimeout[1] == 777 && d.curEnvled == 0b0111000
                       && d.fineEnvled == 0b010101 && d.numClearStaged == 3 && d.freqNudge[4] == 1.03f && d.coarseAdj[4] == 1.0595f
                       && d.coarseAdjLed[4] == 1.25f && d.mod135One == 1 && d.mod246Six == 0 && d.isBpre == 1,
                   "fillDisplayData copies the display state of param_read_freq, the switch positions and the filter type", "");
        }
    }
}
