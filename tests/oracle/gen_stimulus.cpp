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
    // Milestone 7: the Lock button pins come from a second generator, so the stimulus of Milestone 2 (everything above) is unchanged.
    Rng rb (0x4C0CB077u + (uint32_t) scenario * 104729u);
    uint32_t pinMask = 0;
    int pinLeft[6] = {};          // steps left until each pressed button is released
    int nearLeft = 0;             // steps left of the "lock_down just under the Q-unlock time" shortcut

    // scenario table: filter mode (2 TWOPASS, 3 ONEPASS), type (1 MAXQ, 0 BPRE), walker speed, bank interval, event rate (one in N steps)
    struct Sc { int mode, type, speed, bankEvery, eventOneIn, amp; bool randomTicks, extremes, switching; };
    static const Sc table[8] = {
        /* 0 */ { 2, 1,  6, 400,  900, 1 << 18, false, false, false },   // two-pass MAXQ, slow controls
        /* 1 */ { 2, 1, 60, 300,  120, 1 << 18, false, false, false },   // two-pass MAXQ, fast controls, many events
        /* 2 */ { 3, 1, 12, 350,  300, 1 << 26, false, false, false },   // one-pass MAXQ
        /* 3 */ { 3, 0, 12, 350,  300, 1 << 26, false, false, false },   // one-pass BpRe
        /* 4 */ { 2, 1, 25, 200,  200, 1 << 18, false, false, true  },   // mode and type switching
        /* 5 */ { 2, 1, 30, 500,  250, 1 << 18, true,  false, false },  // random tick counts 0..6
        /* 6 */ { 3, 0, 300, 250, 100, 1 << 25, true,  true,  false },  // BpRe, extreme control jumps
        /* 7 */ { 2, 1, 300, 250, 100, 1 << 18, false, true,  true  },  // two-pass, extreme control jumps, switching
    };
    if (scenario < 0 || scenario > 7) { std::printf ("FAIL: scenario out of range\n"); return 2; }
    const Sc sc = table[scenario];

    Walker adc[15], pot[7];
    for (auto& w : adc) w = { r.range (0, 4095), 0, sc.speed, 150, 0, 4095 };
    for (auto& w : pot) w = { r.range (0, 4095), 0, sc.speed, 150, 0, 4095 };
    for (auto& w : adc) w.target = w.v;
    for (auto& w : pot) w.target = w.v;
    // Q pot and Q CV move slowly. param_read_q smooths the goal with poll_ctr/15 and poll_ctr reaches 16, so a fast jump makes
    // qval overshoot 4095 by (jump / 15); in two-pass that index reaches past twopass_calibration[3380] (an out-of-range
    // read in the firmware, docs/port-notes.md P-5), which the oracle cannot give a defined value for.
    adc[1].speed = pot[0].speed = 2;
    // Scenarios that run two-pass: keep Q CV + Q pot at 3700 or less, so even a sudden unlock overshoot (up to 1/15 of the jump)
    // stays under qval 3948 and the second-pass index qval_b - 900 stays inside twopass_calibration[3380].
    if (sc.mode == 2 || sc.switching) { adc[1].hi = pot[0].hi = 1850; adc[1].v = adc[1].target = pot[0].v = pot[0].target = r.range (0, 1850); }
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
    uint8_t curBank[6] = {};   // the banks the firmware has after this step's sets (Milestone 7: see the BpRe one-pass rule below)

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

        // banks: all six channels to one bank at intervals; sometimes one channel on its own (the firmware never sets a bank above 19)
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
            s.bank[ch] = (uint8_t) r.below (20);
        }

        for (int k = 0; k < 6; ++k) if ((s.set_bank_mask >> k) & 1) curBank[k] = s.bank[k];
        // Milestone 7: one-pass BpRe has no branch for bank 19 (the user bank) in filter.c, so the firmware keeps whatever table pointer the channel
        // had. When the switch into one-pass BpRe happens with a MAXQ pointer there, the firmware reads a 231-float table with the 693-float BpRe
        // layout, past its end (docs/port-notes.md P-10). The oracle and the port read different memory there, so the stimulus never makes that switch
        // with a channel on bank 19. (The scenarios that stay on one-pass BpRe from step 0 are unaffected: their stale pointers are BpRe tables.)
        if (sc.switching && s.set_filter_type == 1 && s.set_filter_mode == 3)
            for (int k = 0; k < 6; ++k)
                if (curBank[k] == 19) { s.set_bank_mask |= (uint8_t) (1u << k); s.bank[k] = 18; curBank[k] = 18; }

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

        // Lock button pins (Milestone 7). A press lasts 1 to 60 steps (the firmware debounces 5 loop passes down and 5 up, about 3 steps);
        // one press in six is a long hold, with the shortcut that sets lock_down just under LOCK_BUTTON_QUNLOCK_HOLD_TIME for a stretch,
        // so that the "hold to unlock, keep Q-lock" branch is reached in 30000 steps (50000 passes would take 25000 steps).
        for (int k = 0; k < 6; ++k)
        {
            if (pinLeft[k] > 0 && --pinLeft[k] == 0) pinMask &= ~(1u << k);
            else if (pinLeft[k] == 0 && rb.chance (500))
            {
                const bool longHold = rb.chance (6);
                pinLeft[k] = longHold ? (int) rb.range (60, 120) : (int) rb.range (1, 60);
                pinMask |= 1u << k;
                if (longHold && nearLeft == 0) nearLeft = 40;
            }
        }
        s.lock_button_mask = (uint8_t) pinMask;
        if (nearLeft > 0) { s.lock_down_near = 1; --nearLeft; }

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
