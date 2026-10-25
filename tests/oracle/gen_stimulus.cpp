// Writes the stimulus files for the differential test. Deterministic (fixed seeds); no input files.
//   gen_stimulus <scenario 0..7> <steps> <out>
// Scenarios (docs/architecture.md section 14, V1): every scenario uses a different mix of filter mode and
// type, bank changes, Q, morph, rotation, spread, scale CV, levels, locks and tick schedules.
#include <cmath>

#include "trace_common.h"

using namespace smrtest;

namespace
{
    struct Rng
    {
        uint32_t s;
        explicit Rng (uint32_t seed) : s (seed ? seed : 1) {}
        uint32_t next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
        uint32_t below (uint32_t n) { return next() % n; }
        bool chance (uint32_t oneIn) { return below (oneIn) == 0; }
        int32_t range (int32_t lo, int32_t hi) { return lo + (int32_t) below ((uint32_t) (hi - lo + 1)); }
    };

    // A control that walks toward a target; the target jumps now and then.
    struct Walker
    {
        int v, target, speed;
        uint32_t jumpOneIn;
        int lo, hi;
        void step (Rng& r)
        {
            if (r.chance (jumpOneIn)) target = r.range (lo, hi);
            if (r.chance (40)) target = v;   // sometimes stop
            if (v < target) v = (v + speed > target) ? target : v + speed;
            else if (v > target) v = (v - speed < target) ? target : v - speed;
        }
    };
}

int main (int argc, char** argv)
{
    if (argc < 4) { std::printf ("usage: gen_stimulus <scenario 0..7> <steps> <out>\n"); return 2; }
    const int scenario = std::atoi (argv[1]);
    const int n = std::atoi (argv[2]);
    Rng r (0x1234567u + (uint32_t) scenario * 7919u);

    // scenario table: filter mode (2 TWOPASS, 3 ONEPASS), type (1 MAXQ, 0 BPRE), walker speed, bank interval, event rate (one in N steps)
    struct Sc { int mode, type, speed, bankEvery, eventOneIn, amp; bool randomTicks, extremes, switching; };
    static const Sc table[8] = {
        /* 0 */ { 2, 1,  6, 400,  900, 1 << 26, false, false, false },   // two-pass MAXQ, slow controls
        /* 1 */ { 2, 1, 60, 300,  120, 1 << 26, false, false, false },   // two-pass MAXQ, fast controls, many events
        /* 2 */ { 3, 1, 12, 350,  300, 1 << 26, false, false, false },   // one-pass MAXQ
        /* 3 */ { 3, 0, 12, 350,  300, 1 << 26, false, false, false },   // one-pass BpRe
        /* 4 */ { 2, 1, 25, 200,  200, 1 << 25, false, false, true  },   // mode and type switching
        /* 5 */ { 2, 1, 30, 500,  250, 1 << 26, true,  false, false },  // random tick counts 0..6
        /* 6 */ { 3, 0, 300, 250, 100, 1 << 25, true,  true,  false },  // BpRe, extreme control jumps
        /* 7 */ { 2, 1, 300, 250, 100, 1 << 25, false, true,  true  },  // two-pass, extreme control jumps, switching
    };
    if (scenario < 0 || scenario > 7) { std::printf ("FAIL: scenario out of range\n"); return 2; }
    const Sc sc = table[scenario];

    Walker adc[15], pot[7];
    for (auto& w : adc) w = { r.range (0, 4095), 0, sc.speed, 150, 0, 4095 };
    for (auto& w : pot) w = { r.range (0, 4095), 0, sc.speed, 150, 0, 4095 };
    for (auto& w : adc) w.target = w.v;
    for (auto& w : pot) w.target = w.v;
    // Level CVs and sliders are mostly open so that there is sound
    for (int k = 2; k <= 7; ++k) { adc[k].v = adc[k].target = 3000 + (int) r.below (1095); adc[k].lo = 1500; }
    for (int k = 1; k <= 6; ++k) { pot[k].v = pot[k].target = 3000 + (int) r.below (1095); pot[k].lo = 1500; }
    // Freq CV jacks: keep near the 1 V/oct sweep so that the shifts stay plausible unless extremes are asked for
    if (! sc.extremes) { adc[13].lo = adc[14].lo = 0; adc[13].hi = adc[14].hi = 2500; }

    std::vector<Step> steps ((size_t) n);
    uint8_t sw = (uint8_t) r.below (256);
    uint32_t freqblock = 0, lockMask = 0, lockPressed = 0;
    int lockDownCountdown = 0;
    int tim10Acc = 0, tim9Acc = 0;   // realistic schedule: per block 3.416 TIM10 ticks, 0.933 TIM9 ticks, 2 loop passes
    int mode = sc.mode, type = sc.type;

    for (int i = 0; i < n; ++i)
    {
        Step& s = steps[(size_t) i];
        std::memset (&s, 0, sizeof s);

        for (int k = 0; k < 15; ++k) { adc[k].step (r); s.adc[k] = (uint16_t) adc[k].v; }
        for (int k = 0; k < 7; ++k)  { pot[k].step (r);  s.pot[k] = (uint16_t) pot[k].v; }

        if (r.chance (500)) sw = (uint8_t) (sw ^ (1u << r.below (8)));
        s.sw = sw;

        if (r.chance ((uint32_t) sc.eventOneIn)) s.events |= 1;
        if (r.chance ((uint32_t) sc.eventOneIn)) s.events |= 2;
        if (r.chance ((uint32_t) sc.eventOneIn * 3)) s.events |= 4;
        if (r.chance ((uint32_t) sc.eventOneIn * 3)) s.events |= 8;

        // mode and type: set on step 0 and, for the switching scenarios, now and then
        if (i == 0) { s.set_filter_mode = (uint8_t) mode; s.set_filter_type = (uint8_t) (type == 0 ? 1 : 2); }
        else if (sc.switching && r.chance (180))
        {
            mode = r.chance (2) ? 2 : 3;
            type = r.chance (2) ? 0 : 1;
            s.set_filter_mode = (uint8_t) mode;
            s.set_filter_type = (uint8_t) (type == 0 ? 1 : 2);
        }

        // banks: all six channels to one bank at intervals; sometimes one channel on its own; sometimes 0xFF
        if (i == 0 || (i % sc.bankEvery) == 0)
        {
            const uint8_t bank = (uint8_t) ((i / sc.bankEvery + (uint32_t) scenario) % 20);
            s.set_bank_mask = 0x3F;
            for (int k = 0; k < 6; ++k) s.bank[k] = bank;
        }
        else if (r.chance (700))
        {
            const int ch = (int) r.below (6);
            s.set_bank_mask = (uint8_t) (1u << ch);
            s.bank[ch] = r.chance (6) ? (uint8_t) 0xFF : (uint8_t) r.below (20);
        }

        // locks (latched by the main loop's lock jacks, so only set now and then) and lock buttons (momentary)
        if (r.chance (600))
        {
            lockMask = 0;
            for (int k = 0; k < 6; ++k) if (r.chance (4)) lockMask |= 1u << k;
            s.set_lock = 1;
        }
        s.lock_mask = (uint8_t) lockMask;
        if (r.chance (250)) { lockPressed = 0; if (! r.chance (3)) lockPressed = 1u << r.below (6); }
        s.lock_pressed_mask = (uint8_t) lockPressed;
        if (s.set_lock && r.chance (4)) s.q_locked_mask = (uint8_t) (1u << r.below (6));
        if (lockDownCountdown > 0) { s.lock_down_big = 1; --lockDownCountdown; }
        else if (r.chance (2500)) lockDownCountdown = 40;

        // freqblock: at most 8 of the 20 positions
        if (r.chance (1500))
        {
            freqblock = 0;
            const int nb = (int) r.below (9);
            for (int k = 0; k < nb; ++k) freqblock |= 1u << r.below (20);
        }
        s.freqblock = freqblock;

        // ticks
        if (sc.randomTicks)
        {
            s.tim9 = (uint8_t) r.below (7);
            s.tim10 = (uint8_t) r.below (7);
            s.loop = (uint8_t) r.below (7);
        }
        else
        {
            // 28000 counts per block; TIM10 every 8196, TIM9 every 30001 (both counts per block in units of 1/1000)
            tim10Acc += 28000;
            while (tim10Acc >= 8196) { tim10Acc -= 8196; ++s.tim10; }
            tim9Acc += 28000;
            while (tim9Acc >= 30001) { tim9Acc -= 30001; ++s.tim9; }
            s.loop = 2;
        }

        // audio: noise with silent stretches, impulses and a sine
        const int phase = (i / 500) % 5;
        for (int k = 0; k < 16; ++k)
        {
            int32_t l = 0, rr = 0;
            switch (phase)
            {
                case 0: l = r.range (-sc.amp, sc.amp); rr = r.range (-sc.amp, sc.amp); break;
                case 1: break;   // silence
                case 2: l = rr = (k == 0 && (i % 25) == 0) ? sc.amp : 0; break;   // impulses
                case 3: l = (int32_t) (std::sin ((double) (i * 16 + k) * 0.05) * (double) sc.amp); rr = -l; break;
                default: l = r.range (-sc.amp, sc.amp); rr = (k & 1) ? l : 0; break;
            }
            s.inL[k] = l;
            s.inR[k] = rr;
        }
    }
    return writeStimulus (argv[3], steps) ? 0 : 2;
}
