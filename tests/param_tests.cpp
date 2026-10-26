// Milestone 4 tests (docs/open-questions.md M4-a to M4-g): the parameter table, the mapping to core inputs, the
// actions (locks, bank, triggers, filter selection), the Freq octave mapping, the snapshot and its text encoding.
// Headless: no JUCE. Each check prints one line starting PASS: or FAIL:. Exit code 0 only if every check passed.
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "Engine.h"
#include "ParamMap.h"
#include "StateCodec.h"
#include "Tables.h"

using namespace smr_plugin;

namespace
{
    int failures = 0;
    int checks = 0;

    void check (bool ok, const char* what, const std::string& detail = "")
    {
        ++checks;
        std::printf ("%s: %s%s%s\n", ok ? "PASS" : "FAIL", what, detail.empty() ? "" : " - ", detail.c_str());
        if (! ok) ++failures;
    }

    std::string fmt (const char* f, double a = 0, double b = 0, double c = 0)
    {
        char buf[200];
        std::snprintf (buf, sizeof buf, f, a, b, c);
        return buf;
    }

    // The way the plugin starts the core (docs/open-questions.md M4-b, D3): controls first, then reset, then the
    // initial apply. Block processing: apply, then one core block.
    struct Rig
    {
        std::unique_ptr<smr::Engine> e { new smr::Engine() };
        ParamApplier ap;
        float v[kNumParams];

        Rig() { for (int i = 0; i < kNumParams; ++i) v[i] = paramDefault (i); }
        float& p (const char* id) { return v[indexOfId (id)]; }

        void start()
        {
            e->setControls (ParamApplier::buildControls (v));
            e->reset();
            ap.apply (*e, v, true);
        }

        void run (int blocks, int32_t amp = 0)
        {
            int32_t l[16], r[16], oa[16], ob[16];
            uint32_t s = 777;
            for (int b = 0; b < blocks; ++b)
            {
                for (int k = 0; k < 16; ++k)
                {
                    s = s * 1664525u + 1013904223u;
                    l[k] = amp ? (int32_t) (s >> 8) % amp : 0;
                    r[k] = l[k];
                }
                ap.apply (*e, v);
                e->processBlock (l, r, oa, ob);
            }
        }

        void pulse (const char* id)   // trigger parameter: off, on for one block, off
        {
            p (id) = 0.0f; run (1);
            p (id) = 1.0f; run (1);
            p (id) = 0.0f; run (1);
        }
    };

    uint16_t* field (smr::Controls& c, Role r, int ch)
    {
        switch (r)
        {
            case Role::LevelSlider: return &c.pot[smr::SLIDER_ADC_BASE + ch];
            case Role::LevelCv:     return &c.adc[smr::LEVEL_ADC_BASE + ch];
            case Role::QKnob:       return &c.pot[smr::QPOT_ADC];
            case Role::QCv:         return &c.adc[smr::QVAL_ADC];
            case Role::Morph:       return &c.adc[smr::MORPH_ADC];
            case Role::Spread:      return &c.adc[smr::SPREAD_ADC];
            case Role::RotateCv:    return &c.adc[smr::ROTCV_ADC];
            case Role::ScaleCv:     return &c.adc[smr::SCALE_ADC];
            case Role::NudgeOdds:   return &c.adc[smr::FREQNUDGE1_ADC];
            case Role::NudgeEvens:  return &c.adc[smr::FREQNUDGE6_ADC];
            default: return nullptr;
        }
    }

    void testTable()
    {
        check (kNumParams == 41, "parameter count is 41", fmt ("%.0f", kNumParams));

        bool unique = true, ranges = true, names = true, no4ms = true;
        for (int i = 0; i < kNumParams; ++i)
        {
            const ParamSpec& s = spec (i);
            for (int j = i + 1; j < kNumParams; ++j)
                if (std::strcmp (s.id, spec (j).id) == 0 || std::strcmp (s.name, spec (j).name) == 0) unique = false;
            if (! (paramDefault (i) >= paramMin (i) && paramDefault (i) <= paramMax (i))) ranges = false;
            if (s.kind == Kind::Choice && (s.numChoices < 2 || s.choices == nullptr)) ranges = false;
            if (s.kind == Kind::Choice && (int) paramMax (i) != s.numChoices - 1) ranges = false;
            if (std::strlen (s.id) == 0 || std::strlen (s.name) == 0) names = false;

            std::string all = std::string (s.id) + " " + s.name;
            for (int k = 0; k < s.numChoices; ++k) all += std::string (" ") + s.choices[k];
            for (auto& ch : all) ch = (char) std::tolower ((unsigned char) ch);
            if (all.find ("4ms") != std::string::npos) no4ms = false;
        }
        check (unique, "parameter ids and names are unique");
        check (ranges, "every default lies inside its range; choice lists are consistent");
        check (names, "every parameter has an id and a name");
        check (no4ms, "no parameter id, name or choice contains the text 4ms (README of the firmware repo, Q4)");

        int banks = 0;
        for (int i = 0; i < kNumParams; ++i) if (spec (i).role == Role::Bank) banks = spec (i).numChoices;
        check (banks == smr::kNumScaleBanks, "Bank has 20 entries", fmt ("%.0f", banks));
    }

    void testDefaults()
    {
        // The defaults must equal the controls the renderer and the Milestone 2 tests have used since the start,
        // so that the plugin starts like the verified core. The one deliberate difference is the Fast|Trigger|Slow
        // switch (M4-d): Slow, where the renderer left both env switches off (Trigger).
        float v[kNumParams];
        for (int i = 0; i < kNumParams; ++i) v[i] = paramDefault (i);
        const smr::Controls c = ParamApplier::buildControls (v);

        bool adcOk = true;
        for (int k = 0; k < smr::kNumAdcs; ++k)
        {
            // Freq jacks at 0 octaves read count 11 (BASE_TRACKOFFSET), which looks up the same table entry as count 0.
            const uint16_t want = (k >= 2 && k <= 7) ? 4095 : ((k == smr::FREQCV1_ADC || k == smr::FREQCV6_ADC) ? 11 : 0);
            if (c.adc[k] != want) adcOk = false;
        }
        check (adcOk, "defaults: Level CV 1..6 = 4095, Freq jacks 11 (= 0 octaves), every other ADC count = 0");

        bool potOk = c.pot[0] == 2048;
        for (int k = 1; k <= 6; ++k) if (c.pot[k] != 4095) potOk = false;
        check (potOk, "defaults: Q pot 2048, sliders 4095", fmt ("%.0f", c.pot[0]));

        check (! c.env_mode_pre && ! c.mod246_six && ! c.mod135_one && ! c.range_mode && ! c.cvlag && ! c.rotary_sw,
               "defaults: Post, 135, 246, Scale Rotation off, CV Slew off, ROTATE not pressed");
        check (! c.env_speed_fast && c.env_speed_slow, "defaults: Fast|Trigger|Slow switch on Slow (M4-d)");
    }

    void testUnitMapping()
    {
        // 0 -> 0 counts, 1 -> 4095, 0.5 -> 2048 for every count-type parameter, onto the right ADC/pot slot.
        bool zero = true, full = true, half = true;
        int tested = 0;
        for (int i = 0; i < kNumParams; ++i)
        {
            const ParamSpec& s = spec (i);
            float v[kNumParams];
            for (int k = 0; k < kNumParams; ++k) v[k] = 0.0f;   // everything off, so only this slot can be non-zero

            smr::Controls probe;
            if (field (probe, s.role, s.channel) == nullptr) continue;
            ++tested;

            v[i] = 0.0f;   { smr::Controls c = ParamApplier::buildControls (v); if (*field (c, s.role, s.channel) != 0) zero = false; }
            v[i] = 1.0f;   { smr::Controls c = ParamApplier::buildControls (v); if (*field (c, s.role, s.channel) != 4095) full = false; }
            v[i] = 0.5f;   { smr::Controls c = ParamApplier::buildControls (v); if (*field (c, s.role, s.channel) != 2048) half = false; }
        }
        check (tested == 20, "count-type parameters tested: 6 sliders, 6 Level CV, Q, Q CV, Morph, Spread, Rotate CV, Scale CV, 2 nudge", fmt ("%.0f", tested));
        check (zero, "unit 0 gives 0 counts");
        check (full, "unit 1 gives 4095 counts");
        check (half, "unit 0.5 gives 2048 counts");

        // Out-of-range host values are clamped, NaN gives 0.
        check (unitToCounts (-1.0f) == 0 && unitToCounts (7.0f) == 4095 && unitToCounts (NAN) == 0, "unitToCounts clamps and maps NaN to 0");

        // Display text follows the firmware's own integer maths.
        const int sp = indexOfRole (Role::Spread);
        const int rc = indexOfRole (Role::RotateCv);
        const int sc = indexOfRole (Role::ScaleCv);
        check (formatValue (sp, 0.0f) == "1" && formatValue (sp, 1.0f) == "16" && formatValue (sp, 0.5f) == "9", "Spread text 1..16 (read_spread: (adc>>8)+1)",
               formatValue (sp, 0.0f) + "," + formatValue (sp, 0.5f) + "," + formatValue (sp, 1.0f));
        check (formatValue (rc, 0.0f) == "0" && formatValue (rc, 1.0f) == "19", "Rotate CV text 0..19 (process_rotateCV: adc/205)");
        check (formatValue (sc, 0.0f) == "0" && formatValue (sc, 1.0f) == "10", "Scale CV text 0..10 (process_scaleCV: lpf/409)");
    }

    void testSwitches()
    {
        float v[kNumParams];
        for (int i = 0; i < kNumParams; ++i) v[i] = paramDefault (i);

        v[indexOfId ("oddsmode")] = 1; v[indexOfId ("evensmode")] = 1; v[indexOfId ("prepost")] = 1;
        v[indexOfId ("slew")] = 1; v[indexOfId ("scalerotation")] = 1;
        v[indexOfId ("envspeed")] = 0;
        smr::Controls c = ParamApplier::buildControls (v);
        check (c.mod135_one && c.mod246_six && c.env_mode_pre && c.cvlag && c.range_mode, "switch values reach Controls (1, 6, Pre, Slew, Scale Rotation)");
        check (c.env_speed_fast && ! c.env_speed_slow, "Fast|Trigger|Slow: index 0 is Fast");
        v[indexOfId ("envspeed")] = 1;
        c = ParamApplier::buildControls (v);
        check (! c.env_speed_fast && ! c.env_speed_slow, "Fast|Trigger|Slow: index 1 is Trigger (neither pin)");
        v[indexOfId ("envspeed")] = 2;
        c = ParamApplier::buildControls (v);
        check (! c.env_speed_fast && c.env_speed_slow, "Fast|Trigger|Slow: index 2 is Slow");

        // The core reads them (params.c param_read_switches).
        Rig r;
        r.p ("oddsmode") = 1; r.p ("evensmode") = 1; r.p ("prepost") = 1; r.p ("scalerotation") = 1;
        r.start();
        r.run (4);
        check (r.e->mod_mode_135 == 1 && r.e->mod_mode_246 == 6, "core: mod_mode_135 = 1 and mod_mode_246 = 6");
        check (r.e->env_prepost_mode == smr::PRE && r.e->rotate_to_next_scale == 1, "core: env_prepost_mode PRE, rotate_to_next_scale 1");

        Rig f;
        f.p ("envspeed") = 0; f.start(); f.run (4);
        check (f.e->env_track_mode == smr::ENV_FAST, "core: Fast gives ENV_FAST (rotary_sw is never pressed, so never ENV_VOLTOCT)");
        Rig t;
        t.p ("envspeed") = 1; t.start(); t.run (4);
        check (t.e->env_track_mode == smr::ENV_TRIG, "core: Trigger gives ENV_TRIG");
        Rig s;
        s.start(); s.run (4);
        check (s.e->env_track_mode == smr::ENV_SLOW, "core: default gives ENV_SLOW");

        Rig lag;
        lag.start(); lag.run (4);
        const float lpfOff = lag.e->CHANNEL_LEVEL_LPF;
        lag.p ("morph") = 0.5f; lag.p ("slew") = 1.0f; lag.run (4);
        check (lag.e->CHANNEL_LEVEL_LPF != lpfOff && lag.e->CHANNEL_LEVEL_LPF > 0.0f && lag.e->CHANNEL_LEVEL_LPF < 1.0f,
               "core: CV Slew latches a Morph-dependent CHANNEL_LEVEL_LPF", fmt ("%f vs %f", lag.e->CHANNEL_LEVEL_LPF, lpfOff));
    }

    void testControlsReachCore()
    {
        // Sliders and Level CV: channel_level = Level CV/4096 * (slider - 20)/4096, smoothed (filter.c).
        {
            Rig r;
            r.p ("level3") = 0.0f;
            r.start(); r.run (600);
            check (r.e->channel_level[2] <= 0.0f + 1e-6f && r.e->channel_level[0] > 0.9f,
                   "core: Level 3 slider at 0 silences channel 3, others stay up",
                   fmt ("ch3 %g, ch1 %g", r.e->channel_level[2], r.e->channel_level[0]));
        }
        {
            Rig r;
            r.p ("levelcv5") = 0.0f;
            r.start(); r.run (600);
            check (r.e->channel_level[4] <= 1e-6f && r.e->channel_level[1] > 0.9f, "core: Level CV 5 at 0 silences channel 5",
                   fmt ("ch5 %g", r.e->channel_level[4]));
        }
        {
            Rig lo, hi;
            lo.p ("q") = 0.1f; hi.p ("q") = 0.9f;
            lo.start(); hi.start(); lo.run (6000); hi.run (6000);
            check (hi.e->qval[0] > lo.e->qval[0] + 1000, "core: Q knob raises qval", fmt ("lo %g hi %g", (double) lo.e->qval[0], (double) hi.e->qval[0]));
        }
        {
            // M4-h: params.c param_read_q only runs the Q CV low-pass when the reading has moved by more than 15
            // counts, so a steady CV is taken in ONE step of (1 - QCV_LPF) = 5 % of its value. A faithful port keeps
            // that, so a steady Q CV parameter has about a twentieth of the effect it looks like it should.
            Rig lo, hi;
            lo.p ("qcv") = 0.0f; hi.p ("qcv") = 0.5f;
            lo.start(); hi.start(); lo.run (6000); hi.run (6000);
            const double gain = (double) hi.e->qval[0] - (double) lo.e->qval[0];
            check (gain > 50.0, "core: Q CV raises qval", fmt ("lo %.0f hi %.0f", (double) lo.e->qval[0], (double) hi.e->qval[0]));
            check (gain < 0.08 * 2048.0, "core: a steady Q CV of 2048 counts adds only about 5 percent of it (M1-k, M4-h)", fmt ("added %.0f counts", gain));
        }
        {
            Rig fast, slow;
            fast.p ("morph") = 0.0f; slow.p ("morph") = 1.0f;
            fast.start(); slow.start(); fast.run (3000); slow.run (3000);
            check (slow.e->f_morph < fast.e->f_morph, "core: higher Morph gives a slower f_morph", fmt ("fast %g slow %g", fast.e->f_morph, slow.e->f_morph));
        }
        {
            Rig lo, mid, hi;
            lo.p ("spread") = 0.0f; mid.p ("spread") = 0.5f; hi.p ("spread") = 1.0f;
            lo.start(); mid.start(); hi.start();
            lo.run (50); mid.run (50); hi.run (50);
            check (lo.e->spread == 1 && mid.e->spread == 9 && hi.e->spread == 16, "core: Spread 0, 0.5, 1 gives spread 1, 9, 16",
                   fmt ("%.0f %.0f %.0f", lo.e->spread, mid.e->spread, hi.e->spread));
        }
        {
            Rig r;
            r.start(); r.run (50);
            r.p ("rotatecv") = 1.0f; r.run (50);
            check (r.e->rot_offset == 19, "core: Rotate CV at 1.0 gives rot_offset 19", fmt ("%.0f", r.e->rot_offset));
        }
        {
            Rig r;
            r.start(); r.run (50);
            r.p ("scalecv") = 1.0f; r.run (3000);
            check (r.e->t_scalecv == 10, "core: Scale CV at 1.0 settles at 10", fmt ("%.0f", (int) r.e->t_scalecv));
        }
    }

    void testFreq()
    {
        const float maxOct = freqMaxOctaves();
        check (maxOct > 9.7f && maxOct < 9.95f, "Freq range is about 9.8 octaves (exp_1voct.h ends near 954x)", fmt ("%.2f", maxOct));
        check (octavesToCounts (0.0f) == 11, "0 octaves is count 11 (BASE_TRACKOFFSET), ratio 1.0", fmt ("%.0f", octavesToCounts (0.0f)));
        check (octavesToCounts (maxOct) <= 4095, "top of the range is within 12 bits", fmt ("%.0f", octavesToCounts (maxOct)));

        const float* t = smr::tables().exp_1voct;
        double worst = 0;
        bool mono = true;
        uint16_t prev = 0;
        for (int i = 0; i <= 980; ++i)
        {
            const float oct = maxOct * (float) i / 980.0f;
            const uint16_t c = octavesToCounts (oct);
            if (c < prev) mono = false;
            prev = c;
            const double got = std::log2 ((double) t[c - 11]);
            worst = std::fmax (worst, std::fabs (got - (double) oct) * 1200.0);
        }
        check (mono, "octavesToCounts never goes down as octaves go up");
        check (worst < 2.0, "table value at the chosen count is within 2 cents of 2^octaves", fmt ("worst %.2f cents", worst));

        // End to end: set the Freq parameters, let the 40-tap filter and bracket settle, read freq_shift.
        // Odds go to channels 1,3,5 (freq_shift[0,2,4]), evens to 2,4,6 (freq_shift[1,3,5]) in 135/246 mode.
        double worstE2E = 0;
        bool oddsOk = true, evensOk = true, unity = true;
        const float octs[] = { 0.0f, 0.5f, 1.0f, 2.0f, 3.3f, 5.0f, 7.0f, 9.0f };
        for (float oct : octs)
        {
            Rig r;
            r.p ("freqodds") = oct;
            r.p ("freqevens") = oct * 0.5f;
            r.start(); r.run (3000);
            const double wantO = std::exp2 ((double) oct), wantE = std::exp2 ((double) (oct * 0.5f));
            const double eo = std::fabs (1200.0 * std::log2 ((double) r.e->freq_shift[0] / wantO));
            const double ee = std::fabs (1200.0 * std::log2 ((double) r.e->freq_shift[1] / wantE));
            worstE2E = std::fmax (worstE2E, std::fmax (eo, ee));
            if (r.e->freq_shift[0] != r.e->freq_shift[2] || r.e->freq_shift[0] != r.e->freq_shift[4]) oddsOk = false;
            if (r.e->freq_shift[1] != r.e->freq_shift[3] || r.e->freq_shift[1] != r.e->freq_shift[5]) evensOk = false;
            if (oct == 0.0f && (r.e->freq_shift[0] != 1.0f || r.e->freq_shift[1] != 1.0f)) unity = false;
        }
        check (oddsOk, "core: Freq (odds) drives channels 1, 3 and 5 equally in 135 mode");
        check (evensOk, "core: Freq (evens) drives channels 2, 4 and 6 equally in 246 mode");
        check (unity, "core: Freq at 0 octaves leaves every channel at ratio exactly 1.0");
        check (worstE2E < 10.0, "core: settled freq_shift is within 10 cents of 2^octaves (bracket of 2 counts included)", fmt ("worst %.2f cents", worstE2E));
        std::printf ("INFO: Freq end-to-end worst error %.2f cents; resolution of one ADC count is %.2f cents\n", worstE2E, 1200.0 / 413.7);

        // 1 switch: only channel 1 follows the odds Freq (params.c: mod_mode_135 == 1).
        Rig one;
        one.p ("oddsmode") = 1; one.p ("freqodds") = 2.0f;
        one.start(); one.run (3000);
        check (one.e->freq_shift[0] > 3.5f && one.e->freq_shift[2] == 1.0f && one.e->freq_shift[4] == 1.0f,
               "core: with the 1 switch only channel 1 follows the odds Freq",
               fmt ("%g %g %g", one.e->freq_shift[0], one.e->freq_shift[2], one.e->freq_shift[4]));
    }

    void testNudgeSleep()
    {
        // Decision D3 (follow the source): params.c param_read_freq keeps the nudge at 0 until the knob has moved
        // more than sleep_range (800 counts) from where it was when the module started. The plugin therefore sets
        // the controls before Engine::reset(), so the window centres on the loaded knob position.
        Rig sleepy;
        sleepy.p ("nudgeodds") = 0.6f;       // 2457 counts at load
        sleepy.start();                      // controls first, then reset
        sleepy.run (400);
        check (sleepy.e->t_fo == 0.0f && sleepy.e->f_nudge_odds == 1.0f, "nudge: a knob loaded at 60% is asleep (no nudge)",
               fmt ("t_fo %g, f_nudge_odds %g", sleepy.e->t_fo, sleepy.e->f_nudge_odds));
        sleepy.p ("nudgeodds") = 0.6f + 0.15f;   // +614 counts: still inside the window
        sleepy.run (400);
        check (sleepy.e->t_fo == 0.0f, "nudge: a move of 614 counts is still inside the sleep window");
        sleepy.p ("nudgeodds") = 1.0f;       // +1638 counts: wakes up
        sleepy.run (400);
        check (sleepy.e->t_fo == 4095.0f && sleepy.e->f_nudge_odds > 1.07f, "nudge: a move of more than 800 counts wakes it",
               fmt ("t_fo %g, f_nudge_odds %g", sleepy.e->t_fo, sleepy.e->f_nudge_odds));

        // The wrong order (reset first, knob set afterwards) would wake it at once. This documents why the order matters.
        Rig late;
        late.e->reset();
        late.p ("nudgeodds") = 0.6f;
        late.ap.apply (*late.e, late.v, true);
        late.run (400);
        check (late.e->t_fo == 2457.0f, "nudge: if the knob is set after reset() the nudge is awake at once (why the plugin sets controls first)",
               fmt ("t_fo %g", late.e->t_fo));

        // A knob that starts at 0 and is automated up behaves as on the hardware: 0 until it passes 800 counts, then it jumps.
        Rig a;
        a.start(); a.run (200);
        a.p ("nudgeodds") = 0.15f; a.run (200);   // 614
        const bool still = a.e->t_fo == 0.0f;
        a.p ("nudgeodds") = 0.25f; a.run (200);   // 1024
        check (still && a.e->t_fo == 1024.0f, "nudge: default start, automated past 800 counts, then follows the knob", fmt ("t_fo %g", a.e->t_fo));
    }

    void testActions()
    {
        // Locks (params.c process_lock_buttons release branch).
        {
            Rig r;
            r.start(); r.run (10);
            r.p ("lock2") = 1.0f; r.p ("lock5") = 1.0f; r.run (2);
            check (r.e->lock[1] == 1 && r.e->lock[4] == 1 && r.e->lock[0] == 0 && r.e->lock[2] == 0, "Lock 2 and Lock 5 latch only those channels");
            r.e->q_locked[1] = 1;
            r.p ("lock2") = 0.0f; r.run (2);
            check (r.e->lock[1] == 0 && r.e->q_locked[1] == 0 && r.e->lock[4] == 1, "unlocking clears lock[] and q_locked[] (firmware release branch)");
        }

        // Bank applies to unlocked channels only (process_rotary_button release branch).
        {
            Rig r;
            r.p ("lock3") = 1.0f;
            r.start(); r.run (10);
            r.p ("bank") = 7.0f; r.run (2);
            bool ok = r.e->scale_bank[2] == 0;
            for (int i : { 0, 1, 3, 4, 5 }) if (r.e->scale_bank[i] != 7) ok = false;
            check (ok, "Bank change reaches every unlocked channel and skips the locked one", fmt ("%.0f %.0f", r.e->scale_bank[0], r.e->scale_bank[2]));
            check (r.e->hover_scale_bank == 7, "hover_scale_bank follows the Bank parameter");
            r.p ("bank") = 19.0f; r.run (2);
            check (r.e->scale_bank[0] == 19, "Bank 19 (custom) can be selected");
        }

        // Filter mode and type.
        {
            Rig r;
            r.p ("filtermode") = 1.0f; r.p ("filtertype") = 1.0f;
            r.start(); r.run (8);
            check (r.e->filter_mode == smr::ONEPASS && r.e->filter_type == smr::BPRE, "Filter Mode One-Pass and Filter Type BpRe reach the core at start");
            r.p ("filtertype") = 0.0f; r.p ("filtermode") = 0.0f; r.run (8);
            check (r.e->filter_mode == smr::TWOPASS && r.e->filter_type == smr::MAXQ, "and change back while running");
        }

        // Triggers: rising edge only.
        {
            Rig r;
            r.start(); r.run (50);
            const uint8_t before = r.e->note[0];
            r.pulse ("rotatecw");
            r.run (800);
            const uint8_t afterCw = r.e->note[0];
            r.pulse ("rotateccw");
            r.run (800);
            const uint8_t afterCcw = r.e->note[0];
            check (afterCw == (uint8_t) ((before + 1) % 20), "Rotate Clockwise moves every unlocked channel up by one position",
                   fmt ("%.0f -> %.0f", before, afterCw));
            check (afterCcw == before, "Rotate Counter-clockwise moves it back", fmt ("%.0f -> %.0f", afterCw, afterCcw));
        }
        {
            Rig r;
            r.start(); r.run (50);
            r.p ("rotatecw") = 1.0f; r.run (2000);           // held on: one trigger, not a stream
            check (r.e->note[0] == 6, "a Rotate trigger held on fires once (rising edge)", fmt ("%.0f", r.e->note[0]));
            r.p ("rotatecw") = 0.0f; r.run (10);
            r.p ("rotatecw") = 1.0f; r.run (800);
            check (r.e->note[0] == 7, "turning it off and on again fires again", fmt ("%.0f", r.e->note[0]));
        }
        {
            Rig r;
            r.p ("lock4") = 1.0f;
            r.start(); r.run (50);
            const uint8_t locked = r.e->note[3];
            r.pulse ("rotatecw"); r.run (800);
            check (r.e->note[3] == locked && r.e->note[0] == 6, "a locked channel does not rotate");
        }
        {
            Rig r;
            r.start(); r.run (50);
            const uint8_t s0 = r.e->scale[0];
            r.pulse ("scaleup"); r.run (800);
            const uint8_t up = r.e->scale[0];
            r.pulse ("scaledown"); r.run (800);
            check (up == s0 + 1 && r.e->scale[0] == s0, "Scale Up then Scale Down changes the scale by one and back",
                   fmt ("%.0f -> %.0f -> %.0f", s0, up, r.e->scale[0]));
        }
        {
            // Nothing fires on the initial apply, even if a trigger parameter was saved as on.
            Rig r;
            r.p ("rotatecw") = 1.0f; r.p ("scaleup") = 1.0f;
            r.start(); r.run (800);
            check (r.e->note[0] == 5 && r.e->scale[0] == 6, "no trigger fires when the plugin starts with a trigger parameter on",
                   fmt ("note %.0f scale %.0f", r.e->note[0], r.e->scale[0]));
        }
    }

    void testSnapshot()
    {
        Rig r;
        r.p ("lock2") = 1.0f;
        r.start(); r.run (50);
        r.pulse ("rotatecw"); r.pulse ("scaleup"); r.run (800);
        r.p ("bank") = 11.0f; r.run (2);
        r.e->q_locked[1] = 1;
        r.e->setFreqBlock (0b101);
        r.run (100, 1 << 20);

        const smr::Snapshot s = r.e->saveSnapshot();
        check (s.lock[1] == 1 && s.scale_bank[0] == 11 && s.scale_bank[1] == 0 && s.note[0] == 7 && s.note[2] == 8 && s.note[1] == 6 && s.scale[0] == 7 && s.freqblock == 5,
               "saveSnapshot captures lock, per-channel bank, rotation (channel 1 skips the locked channel's note), scale and forbidden notes");

        const std::string text = encodeSnapshot (s);
        smr::Snapshot back;
        check (text.size() == 172 && decodeSnapshot (text, back), "snapshot encodes to 172 hex digits and decodes", fmt ("%.0f", (int) text.size()));
        check (encodeSnapshot (back) == text && std::memcmp (&s.note, &back.note, 6) == 0 && back.freqblock == 5 && back.hover_scale_bank == s.hover_scale_bank,
               "encode, decode, encode is stable");

        // Load into a fresh core that starts the way the plugin starts it. Lock 2 is on in the parameters too, as it
        // was when the snapshot was taken.
        Rig n;
        n.p ("lock2") = 1.0f;
        n.start();
        check (n.e->loadSnapshot (back), "loadSnapshot accepts a saved snapshot");
        n.ap.apply (*n.e, n.v, true);
        check (n.e->scale_bank[0] == 11 && n.e->scale_bank[1] == 0 && n.e->note[0] == 7 && n.e->note[2] == 8 && n.e->scale[0] == 7 && n.e->freqblock == 5,
               "loaded core has the saved bank, rotation, scale and forbidden notes");
        check (n.e->lock[1] == 1 && n.e->q_locked[1] == 1 && n.e->qval_goal[1] == (float) n.e->qval[1], "lock, q_locked and qval_goal = qval are restored");
        check (n.e->motion_fadeto_note[0] == 7 && n.e->motion_spread_dest[0] == 7 && n.e->motion_scale_dest[0] == 7, "motion state is reset onto the loaded position");
        check (n.e->hover_scale_bank == s.hover_scale_bank, "hover bank is derived as load_params_from_sram does", fmt ("%d vs %d", n.e->hover_scale_bank, s.hover_scale_bank));
        n.run (200, 1 << 20);
        bool finite = true;
        for (int i = 0; i < 6; ++i) if (! std::isfinite (n.e->channel_level[i]) || ! std::isfinite (n.e->freq_shift[i])) finite = false;
        check (finite, "the loaded core keeps running with finite state");

        // If the Lock parameters disagree with the snapshot, the parameters win (the user can see them): Lock 2 off
        // unlocks channel 2 and clears its q_locked, exactly as unlocking does.
        Rig w;
        w.start();
        w.e->loadSnapshot (back);
        w.ap.apply (*w.e, w.v, true);
        check (w.e->lock[1] == 0 && w.e->q_locked[1] == 0, "after a load the Lock parameters win over the snapshot's lock[]", fmt ("%d", w.e->lock[1]));

        // The hover bank is the first unlocked channel's bank.
        smr::Snapshot h = s;
        h.lock[0] = 1; h.scale_bank[0] = 3; h.scale_bank[1] = 9;
        h.lock[1] = 0;
        Rig hr; hr.start();
        hr.e->loadSnapshot (h);
        check (hr.e->hover_scale_bank == 9, "hover bank skips locked channels", fmt ("%.0f", hr.e->hover_scale_bank));
        smr::Snapshot allLocked = s;
        for (int i = 0; i < 6; ++i) { allLocked.lock[i] = 1; allLocked.scale_bank[i] = (uint8_t) (i + 1); }
        hr.e->loadSnapshot (allLocked);
        check (hr.e->hover_scale_bank == 1, "with every channel locked the hover bank is channel 1's");

        // Too many forbidden notes are discarded, as the firmware does.
        smr::Snapshot many = s;
        many.freqblock = 0x7FFF;   // 15 bits
        hr.e->loadSnapshot (many);
        check (hr.e->freqblock == 0, "15 forbidden notes are discarded (load_params_from_sram: more than 14 is invalid)");

        // Invalid snapshots change nothing.
        smr::Snapshot bad = s;
        bad.note[2] = 20;
        const smr::Snapshot beforeBad = hr.e->saveSnapshot();
        check (! hr.e->loadSnapshot (bad) && std::memcmp (&beforeBad.note, &hr.e->note, 6) == 0, "a note of 20 is rejected and nothing changes");
        bad = s; bad.scale[0] = 11;      check (! hr.e->loadSnapshot (bad), "scale 11 is rejected");
        bad = s; bad.scale_bank[0] = 20; check (! hr.e->loadSnapshot (bad), "bank 20 is rejected");
        bad = s; bad.filter_mode = 4;    check (! hr.e->loadSnapshot (bad), "filter mode 4 is rejected");
        bad = s; bad.lock[0] = 2;        check (! hr.e->loadSnapshot (bad), "lock value 2 is rejected");
        bad = s; bad.freq_nudge[0] = NAN; check (! hr.e->loadSnapshot (bad), "NaN nudge is rejected");
        bad = s; bad.qval[0] = 70000;    check (! hr.e->loadSnapshot (bad), "qval 70000 is rejected");
        bad = s; bad.freqblock = 1u << 25; check (! hr.e->loadSnapshot (bad), "forbidden-note bits above 19 are rejected");

        // Text decoding rejects damage.
        smr::Snapshot tmp;
        check (! decodeSnapshot (text.substr (0, 170), tmp), "decode rejects a short string");
        check (! decodeSnapshot (text + "00", tmp), "decode rejects a long string");
        std::string z = text; z[10] = 'g';
        check (! decodeSnapshot (z, tmp), "decode rejects a non-hex character");
        std::string ver = text; ver[0] = '0'; ver[1] = '2';
        check (! decodeSnapshot (ver, tmp), "decode rejects an unknown version");
        check (decodeSnapshot ("", tmp) == false, "decode rejects an empty string");
    }

    void testSnapshotContinuity()
    {
        // A core restored from a snapshot plays like the one it was taken from at the saved moment: same notes and
        // scales, so the same resonant frequencies. (Filter memory is not part of the firmware's bank.)
        Rig a;
        a.start(); a.run (50);
        a.pulse ("rotatecw"); a.pulse ("scaleup"); a.run (800);
        const smr::Snapshot s = a.e->saveSnapshot();

        Rig b;
        b.start();
        b.e->loadSnapshot (s);
        b.ap.apply (*b.e, b.v, true);
        b.run (800);
        bool same = true;
        for (int i = 0; i < 6; ++i) if (a.e->note[i] != b.e->note[i] || a.e->scale[i] != b.e->scale[i] || a.e->scale_bank[i] != b.e->scale_bank[i]) same = false;
        check (same, "restored core holds the same note, scale and bank on every channel");
    }
}

int main()
{
    testTable();
    testDefaults();
    testUnitMapping();
    testSwitches();
    testControlsReachCore();
    testFreq();
    testNudgeSleep();
    testActions();
    testSnapshot();
    testSnapshotContinuity();

    std::printf ("%s: %d checks, %d failed\n", failures ? "FAIL" : "PASS", checks, failures);
    return failures ? 1 : 0;
}
