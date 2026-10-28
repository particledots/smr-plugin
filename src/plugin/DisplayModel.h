// Milestone 6b: what the window shows of the core, and the firmware's rules for turning it into light. Header only, no JUCE,
// no changes to src/core. Everything here is read-only with respect to the core: the audio thread copies a few members of
// smr::Engine after each host block (fillDisplayData) and the editor reads the copy.
//
// Rules ported from third_party/SMR (v5.1.1, 79bc953), see docs/m6b-led-notes.md:
//   led_ring.c   display_filter_rotation()   ring            -> ringLeds()
//   led_ring.c   calculate_envout_leds()     Env Out lights  -> envOutLeds()
//   led_ring.c   DEFAULT_COLOR_CH[0]         colours         -> kColours
//   leds.c       update_slider_LEDs()        slider LED duty -> sliderLedDuty()
//   filter.c     process_audio_block()       slider clip     -> sliderClipBits()
//   audio_util.c audio_convert_2x16_to_stereo24()  input clip -> kInputClipFloat
#pragma once

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <type_traits>

#include "Engine.h"

namespace smr_plugin
{
    constexpr int kRingLeds = 20;                         // NUM_FILTS

    // globals.h: SLIDER_CLIP_LEVEL 0x50000000 and INPUT_LED_CLIP_LEVEL 0x58000000 (32-bit sample scale).
    constexpr float kSliderClipLevel = 1342177280.0f;     // 0x50000000
    // BlockAdapter.h maps host 1.0 to the largest int32 below 2^31 (placeholder M1-n), so the input clip level in host units is
    // 0x58000000 / 2^31 = 0.6875. Positive samples only: audio_util.c compares without an absolute value.
    constexpr float kInputClipFloat = 0.6875f;

    // Bits of the clip accumulator in DisplayHub: 0..5 slider clip of channel 1..6, 6 input clip.
    constexpr uint32_t kInputClipBit = 1u << 6;

    // The ring redraws every 2001 main-loop passes in the firmware (led_ring.c update_LED_ring); the locked-channel flash toggles
    // once per redraw. The main-loop rate is the placeholder of docs/open-questions.md M2-c (smr::kLoopRateHz), so this is a placeholder too.
    constexpr double kRingFlashMs = 2001.0 * 1000.0 / (double) smr::kLoopRateHz;

    // led_ring.c DEFAULT_COLOR_CH[0] (cur_colsch = 0): red, green, blue per channel, 12 bit duty values.
    constexpr float kColours[6][3] = {
        { 0, 0, 761 }, { 0, 770, 766 }, { 0, 766, 16 }, { 700, 700, 700 }, { 763, 154, 0 }, { 766, 0, 112 } };

    // Plain copy of the core members the window needs. Only 32-bit integers and floats: no padding, copied word by word.
    struct DisplayData
    {
        uint32_t valid = 0;               // 0: the audio side has not published (not running, or host rate unsupported)
        uint32_t freqblock = 0;
        uint32_t envMode = 0;             // smr::EnvOutMode: 0 slow, 1 fast, 2 trigger, 3 1V/oct
        int32_t  note[6] = {};
        int32_t  fadeto[6] = {};          // motion_fadeto_note
        uint32_t lock[6] = {};
        uint32_t qlocked[6] = {};
        float    morph[6] = {};           // motion_morphpos
        float    level[6] = {};           // channel_level
        uint32_t envPwm[6] = {};          // ENVOUT_PWM, 0..4095
    };
    static_assert (std::is_trivially_copyable<DisplayData>::value, "DisplayData is copied word by word");
    static_assert (sizeof (DisplayData) % 4 == 0, "DisplayData is a whole number of 32-bit words");

    // One writer (the audio thread), any number of readers. No locks, no allocation. A sequence counter around the copy
    // (seqlock): a reader that sees the counter change or stay odd tries again and gives up after 8 tries.
    // The accumulators catch events between two window frames: the audio side adds, the window takes and clears.
    class DisplayHub
    {
    public:
        // audio thread
        void publish (const DisplayData& d) noexcept
        {
            uint32_t w[kWords];
            std::memcpy (w, &d, sizeof d);
            const uint32_t s = seq.load (std::memory_order_relaxed);
            seq.store (s + 1, std::memory_order_relaxed);
            std::atomic_thread_fence (std::memory_order_release);
            for (int i = 0; i < kWords; ++i) words[i].store (w[i], std::memory_order_relaxed);
            seq.store (s + 2, std::memory_order_release);
        }
        void addClipBits (uint32_t bits) noexcept { if (bits != 0) clipBits.fetch_or (bits, std::memory_order_relaxed); }
        void notePeak (int which, float v) noexcept
        {
            auto& a = peak[which & 3];
            if (v > a.load (std::memory_order_relaxed)) a.store (v, std::memory_order_relaxed);
        }

        // window (message thread)
        bool read (DisplayData& d) const noexcept
        {
            for (int tries = 0; tries < 8; ++tries)
            {
                const uint32_t s1 = seq.load (std::memory_order_acquire);
                if (s1 & 1u) continue;
                uint32_t w[kWords];
                for (int i = 0; i < kWords; ++i) w[i] = words[i].load (std::memory_order_relaxed);
                std::atomic_thread_fence (std::memory_order_acquire);
                if (seq.load (std::memory_order_relaxed) == s1)
                {
                    std::memcpy (&d, w, sizeof d);
                    return true;
                }
            }
            return false;
        }
        uint32_t takeClipBits() noexcept { return clipBits.exchange (0, std::memory_order_relaxed); }
        // Peak since the last call: 0 In slot 1 (odds), 1 In slot 2 (evens), 2 Out slot 1, 3 Out slot 2 (host left and right outputs).
        float takePeak (int which) noexcept { return peak[which & 3].exchange (0.0f, std::memory_order_relaxed); }

    private:
        static constexpr int kWords = (int) (sizeof (DisplayData) / 4);
        std::atomic<uint32_t> seq { 0 };
        std::atomic<uint32_t> words[kWords] = {};
        std::atomic<uint32_t> clipBits { 0 };
        std::atomic<float> peak[4] = {};
        static_assert (std::atomic<uint32_t>::is_always_lock_free && std::atomic<float>::is_always_lock_free, "audio side must not lock");
    };

    // Copy of the members, taken on the audio thread after a host block.
    inline void fillDisplayData (const smr::Engine& e, DisplayData& d) noexcept
    {
        d.valid = 1;
        d.freqblock = e.freqblock;
        d.envMode = (uint32_t) e.env_track_mode;
        for (int j = 0; j < smr::kNumChannels; ++j)
        {
            d.note[j]    = e.note[j];
            d.fadeto[j]  = e.motion_fadeto_note[j];
            d.lock[j]    = e.lock[j];
            d.qlocked[j] = e.q_locked[j];
            d.morph[j]   = e.motion_morphpos[j];
            d.level[j]   = e.channel_level[j];
            d.envPwm[j]  = e.ENVOUT_PWM[j];
        }
    }

    // filter.c process_audio_block, end of block: f_blended from the first frame of the block, times channel_level, against
    // SLIDER_CLIP_LEVEL. slider_led_mode is SHOW_CLIPPING by default (leds.c) and ui_mode is always PLAY in the plugin.
    // src/core/Filter.cpp leaves this out ("LED only"), so it is repeated here from the public state. The core state is
    // read after the host block, so it is the last 16-frame block of it (docs/m6b-led-notes.md, "Plugin limit").
    inline uint32_t sliderClipBits (const smr::Engine& e) noexcept
    {
        uint32_t bits = 0;
        for (int j = 0; j < smr::kNumChannels; ++j)
        {
            const float f = (e.filter_out[j][0] * (1.0f - e.motion_morphpos[j])) + (e.filter_out[j + smr::kNumChannels][0] * e.motion_morphpos[j]);
            if ((f * e.channel_level[j]) > kSliderClipLevel) bits |= 1u << j;
        }
        return bits;
    }

    // leds.c update_slider_LEDs, play branch: the LED is on while channel_level >= f_slider_pwm, which falls from 4.0 to 0 in
    // 50 steps, so the on-time is channel_level / 4 (25 % at level 1.0). A clipping block switches it fully on.
    inline float sliderLedDuty (float level) noexcept
    {
        if (! (level > 0.0f)) return 0.0f;
        return std::min (level, 4.0f) / 4.0f;
    }

    // led_ring.c display_filter_rotation(), play mode. flash: the firmware's static `flash`, toggled once per redraw
    // (locked channels show their LED only while it is set). Output: 12 bit duty values, clamped to 1023 as in the firmware.
    inline void ringLeds (const DisplayData& d, bool flash, uint16_t ring[kRingLeds][3]) noexcept
    {
        float inv_fade[6], fade[6];

        for (int i = 0; i < kRingLeds; ++i) ring[i][0] = ring[i][1] = ring[i][2] = 0;

        for (int chan = 0; chan < 6; ++chan)
        {
            const float t_f = d.level[chan] < 0.05f ? 0.05f : d.level[chan];
            if (d.lock[chan] == 0)
            {
                inv_fade[chan] = (1.0f - d.morph[chan]) * t_f;
                fade[chan] = d.morph[chan] * t_f;
            }
            else
            {
                fade[chan] = 0.0f;
                inv_fade[chan] = flash ? t_f : 0.0f;
            }
        }

        for (int i = 0; i < kRingLeds; ++i)
        {
            for (int chan = 0; chan < 6; ++chan)
            {
                const int next_i = d.fadeto[chan];

                if ((d.freqblock & (1u << i)) != 0)          // blocked notes: 75, 75, 75
                {
                    ring[i][0] = 75; ring[i][1] = 75; ring[i][2] = 75;
                }
                else if (d.note[chan] == i)
                {
                    if (inv_fade[chan] > 0.0f)
                    {
                        const bool empty = (ring[i][0] + ring[i][1] + ring[i][2]) == 0;
                        for (int c = 0; c < 3; ++c)
                        {
                            const uint16_t v = (uint16_t) (kColours[chan][c] * inv_fade[chan]);
                            ring[i][c] = empty ? v : (uint16_t) (ring[i][c] + v);
                            if (ring[i][c] > 1023) ring[i][c] = 1023;
                        }
                    }
                    // The firmware indexes ring[next_i] unchecked; a value outside 0..19 would be a firmware bug, so it is skipped here.
                    if (fade[chan] > 0.0f && next_i >= 0 && next_i < kRingLeds)
                    {
                        const bool empty = (ring[next_i][0] + ring[next_i][1] + ring[next_i][2]) == 0;
                        for (int c = 0; c < 3; ++c)
                        {
                            const uint16_t v = (uint16_t) (kColours[chan][c] * fade[chan]);
                            ring[next_i][c] = empty ? v : (uint16_t) (ring[next_i][c] + v);
                            if (ring[next_i][c] > 1023) ring[next_i][c] = 1023;
                        }
                    }
                    break;      // chan = 6: only the first channel at this position is drawn
                }
            }
        }
    }

    // led_ring.c calculate_envout_leds(), play mode and 1V/oct mode (the other branches need UI state the plugin does not have).
    inline void envOutLeds (const DisplayData& d, uint16_t env[6][3]) noexcept
    {
        for (int chan = 0; chan < 6; ++chan)
        {
            float amt = (float) d.envPwm[chan] / 4096.0f;
            if (d.envMode == (uint32_t) smr::ENV_VOLTOCT && amt < 0.05f) amt = 0.05f;
            for (int c = 0; c < 3; ++c)
            {
                env[chan][c] = (uint16_t) (kColours[chan][c] * amt);
                if (env[chan][c] > 1023) env[chan][c] = 1023;
            }
        }
    }
}
