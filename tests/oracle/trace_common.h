// Shared by the two differential-test drivers (oracle_driver.cpp, port_driver.cpp) and by trace_diff.cpp.
// Test code only. Defines the stimulus file format, the per-block trace format, and the one function that
// turns "what the module looks like after a block" into trace words, so both drivers write the same fields.
#pragma once

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace smrtest
{
    // One stimulus step = one 16-frame block of audio, then the ticks that fall due after it.
    // The step also carries the direct sets that the firmware's UI code would make. Both drivers apply every
    // field in the same order: direct sets, switches and ADC values, trigger events, audio block, ticks, trace.
#pragma pack(push, 1)
    struct Step
    {
        uint16_t adc[15];
        uint16_t pot[7];
        uint8_t  sw;                // bit0 env_mode_pre, 1 mod246_six, 2 mod135_one, 3 range_mode, 4 env_speed_fast, 5 env_speed_slow, 6 rotary_sw, 7 cvlag
        uint8_t  events;            // bit0 do_ROTUP, 1 do_ROTDOWN, 2 do_LOCK135, 3 do_LOCK246
        uint8_t  set_filter_mode;   // 0 keep, 2 TWOPASS, 3 ONEPASS (set directly, as system_mode.c does)
        uint8_t  set_filter_type;   // 0 keep, 1 change_filter_type(BPRE), 2 change_filter_type(MAXQ)
        uint8_t  set_bank_mask;     // bit i: scale_bank[i] = bank[i]
        uint8_t  bank[6];
        uint8_t  set_lock;          // 1: apply lock_mask and q_locked_mask this step (lock[] is a latch: the main loop toggles it)
        uint8_t  lock_mask;         // lock[i] = bit i
        uint8_t  lock_pressed_mask; // lock_pressed[i] = bit i (applied every step: it is the button state)
        uint8_t  q_locked_mask;     // q_locked[i] = bit i
        uint8_t  lock_down_big;     // 1: lock_down[0..5] = 40000, else 0 (applied every step)
        uint32_t freqblock;
        uint8_t  tim9, tim10, loop; // ticks to run after the block, in the order TIM9, TIM10, loop
        int32_t  inL[16];
        int32_t  inR[16];
    };
#pragma pack(pop)

    static_assert (sizeof (Step) == 15 * 2 + 7 * 2 + 4 + 1 + 6 + 5 + 4 + 3 + 128, "Step layout");

    inline bool readStimulus (const char* path, std::vector<Step>& out)
    {
        FILE* f = std::fopen (path, "rb");
        if (! f) return false;
        char magic[8];
        uint32_t n = 0;
        bool ok = std::fread (magic, 1, 8, f) == 8 && std::memcmp (magic, "SMRSTIM1", 8) == 0 && std::fread (&n, 4, 1, f) == 1;
        if (ok)
        {
            out.resize (n);
            ok = n == 0 || std::fread (out.data(), sizeof (Step), n, f) == n;
        }
        std::fclose (f);
        return ok;
    }

    inline bool writeStimulus (const char* path, const std::vector<Step>& in)
    {
        FILE* f = std::fopen (path, "wb");
        if (! f) return false;
        const uint32_t n = (uint32_t) in.size();
        bool ok = std::fwrite ("SMRSTIM1", 1, 8, f) == 8 && std::fwrite (&n, 4, 1, f) == 1
                  && (n == 0 || std::fwrite (in.data(), sizeof (Step), n, f) == n);
        std::fclose (f);
        return ok;
    }

    // Trace: per step, a fixed list of named 32-bit words. Names are stored once, in the file header.
    struct Trace
    {
        std::vector<std::string> names;
        std::vector<uint32_t> data;
        uint32_t steps = 0;
        uint32_t wordsPerStep = 0;
        uint32_t cursor = 0;       // index within the current step

        void beginStep() { cursor = 0; }
        void endStep()
        {
            if (steps == 0) wordsPerStep = cursor;
            ++steps;
        }
        void w (const char* name, uint32_t v)
        {
            if (steps == 0) names.push_back (name);
            data.push_back (v);
            ++cursor;
        }
        void i (const char* name, int32_t v) { w (name, (uint32_t) v); }
        void f (const char* name, float v) { uint32_t u; std::memcpy (&u, &v, 4); w (name, u); }

        bool write (const char* path) const
        {
            FILE* fp = std::fopen (path, "wb");
            if (! fp) return false;
            bool ok = std::fwrite ("SMRTRC01", 1, 8, fp) == 8 && std::fwrite (&steps, 4, 1, fp) == 1
                      && std::fwrite (&wordsPerStep, 4, 1, fp) == 1;
            for (const auto& n : names)
                ok = ok && std::fwrite (n.c_str(), 1, n.size() + 1, fp) == n.size() + 1;
            ok = ok && (data.empty() || std::fwrite (data.data(), 4, data.size(), fp) == data.size());
            std::fclose (fp);
            return ok;
        }
    };

    // Everything compared after each step. Both drivers fill this from their own variables.
    struct View
    {
        const int32_t* outL;          // filtered_buffer  (16)
        const int32_t* outR;          // filtered_bufferR (16)
        const uint32_t* envPwm;       // ENVOUT_PWM[6]
        const float* envPreload;      // ENVOUT_preload[6]
        const uint32_t* qval;         // qval[6]
        const uint8_t* note;
        const uint8_t* scale;
        const uint8_t* scaleBank;
        const float* morphpos;
        const int8_t* fadetoNote;
        const int8_t* fadetoScale;
        const int8_t* spreadDest;
        const int8_t* spreadDir;
        const int8_t* scaleDest;
        const int8_t* scalecvOverage;
        const float* nudge;
        const float* shift;
        const float* level;
        const uint8_t* lock;
        int motionRotate, motionNotejump, spread;
        uint32_t filterType, filterMode, envTrackMode;
        float levelLpf, fMorph;
    };

    inline void emit (Trace& t, const View& v)
    {
        char nm[48];
        auto arr32 = [&] (const char* base, const int32_t* p, int n) { for (int k = 0; k < n; ++k) { std::snprintf (nm, sizeof nm, "%s[%d]", base, k); t.i (nm, p[k]); } };
        auto arru  = [&] (const char* base, const uint32_t* p, int n) { for (int k = 0; k < n; ++k) { std::snprintf (nm, sizeof nm, "%s[%d]", base, k); t.w (nm, p[k]); } };
        auto arrf  = [&] (const char* base, const float* p, int n) { for (int k = 0; k < n; ++k) { std::snprintf (nm, sizeof nm, "%s[%d]", base, k); t.f (nm, p[k]); } };
        auto arru8 = [&] (const char* base, const uint8_t* p, int n) { for (int k = 0; k < n; ++k) { std::snprintf (nm, sizeof nm, "%s[%d]", base, k); t.w (nm, p[k]); } };
        auto arri8 = [&] (const char* base, const int8_t* p, int n) { for (int k = 0; k < n; ++k) { std::snprintf (nm, sizeof nm, "%s[%d]", base, k); t.i (nm, p[k]); } };

        arr32 ("filtered_buffer", v.outL, 16);
        arr32 ("filtered_bufferR", v.outR, 16);
        arru ("ENVOUT_PWM", v.envPwm, 6);
        arrf ("ENVOUT_preload", v.envPreload, 6);
        arru ("qval", v.qval, 6);
        arru8 ("note", v.note, 6);
        arru8 ("scale", v.scale, 6);
        arru8 ("scale_bank", v.scaleBank, 6);
        arrf ("motion_morphpos", v.morphpos, 6);
        arri8 ("motion_fadeto_note", v.fadetoNote, 6);
        arri8 ("motion_fadeto_scale", v.fadetoScale, 6);
        arri8 ("motion_spread_dest", v.spreadDest, 6);
        arri8 ("motion_spread_dir", v.spreadDir, 6);
        arri8 ("motion_scale_dest", v.scaleDest, 6);
        arri8 ("motion_scalecv_overage", v.scalecvOverage, 6);
        arrf ("freq_nudge", v.nudge, 6);
        arrf ("freq_shift", v.shift, 6);
        arrf ("channel_level", v.level, 6);
        arru8 ("lock", v.lock, 6);
        t.i ("motion_rotate", v.motionRotate);
        t.i ("motion_notejump", v.motionNotejump);
        t.i ("spread", v.spread);
        t.w ("filter_type", v.filterType);
        t.w ("filter_mode", v.filterMode);
        t.w ("env_track_mode", v.envTrackMode);
        t.f ("CHANNEL_LEVEL_LPF", v.levelLpf);
        t.f ("f_morph", v.fMorph);
    }

    // Table dump used by V1a: a flat list of 32-bit words, written as a Trace with one "step".
    inline void dumpWords (Trace& t, const char* base, const void* p, size_t n)
    {
        const uint32_t* w = static_cast<const uint32_t*> (p);
        char nm[64];
        for (size_t k = 0; k < n; ++k)
        {
            std::snprintf (nm, sizeof nm, "%s[%zu]", base, k);
            t.w (nm, w[k]);
        }
    }
}
