#include "ParamMap.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "Tables.h"

namespace smr_plugin
{
    namespace
    {
        const char* const kOddsModes[]   = { "135 (all odd channels)", "1 (channel 1 only)" };
        const char* const kEvensModes[]  = { "246 (all even channels)", "6 (channel 6 only)" };
        const char* const kPrePost[]     = { "Post", "Pre" };
        const char* const kEnvSpeed[]    = { "Fast", "Trigger", "Slow" };
        const char* const kFilterMode[]  = { "Two-Pass", "One-Pass" };
        const char* const kFilterType[]  = { "MAXQ", "BpRe" };
        // Colour groups from the manual ("Bank colours", v5): blue 6, pink 3, orange 4, green 6, white 1 = 20 banks,
        // in the order of filter.c's scale_bank numbering (docs/architecture.md 3.2). Names are group and number only.
        const char* const kBanks[] = {
            "Blue 1", "Blue 2", "Blue 3", "Blue 4", "Blue 5", "Blue 6",
            "Pink 1", "Pink 2", "Pink 3",
            "Orange 1", "Orange 2", "Orange 3", "Orange 4",
            "Green 1", "Green 2", "Green 3", "Green 4", "Green 5", "Green 6",
            "White (custom)" };

        constexpr float kQDefault = 2048.0f / 4095.0f;   // the Q knob default used since Milestone 2 (smr_render, tests)

#define F(id, name, role, ch, def) { id, name, Kind::Float, Role::role, ch, 0.0f, 1.0f, def, 0, nullptr }
#define B(id, name, role, ch, def) { id, name, Kind::Bool, Role::role, ch, 0.0f, 1.0f, def, 0, nullptr }
#define C(id, name, role, list, def) { id, name, Kind::Choice, Role::role, 0, 0.0f, (float) (sizeof (list) / sizeof (list[0]) - 1), def, \
                                       (int) (sizeof (list) / sizeof (list[0])), list }

        const ParamSpec kSpecs[kNumParams] = {
            F ("level1", "Level 1", LevelSlider, 0, 1.0f), F ("level2", "Level 2", LevelSlider, 1, 1.0f),
            F ("level3", "Level 3", LevelSlider, 2, 1.0f), F ("level4", "Level 4", LevelSlider, 3, 1.0f),
            F ("level5", "Level 5", LevelSlider, 4, 1.0f), F ("level6", "Level 6", LevelSlider, 5, 1.0f),
            F ("levelcv1", "Level CV 1", LevelCv, 0, 1.0f), F ("levelcv2", "Level CV 2", LevelCv, 1, 1.0f),
            F ("levelcv3", "Level CV 3", LevelCv, 2, 1.0f), F ("levelcv4", "Level CV 4", LevelCv, 3, 1.0f),
            F ("levelcv5", "Level CV 5", LevelCv, 4, 1.0f), F ("levelcv6", "Level CV 6", LevelCv, 5, 1.0f),
            F ("q", "Res (Q) Knob", QKnob, 0, kQDefault),
            F ("qcv", "Res (Q) CV", QCv, 0, 0.0f),
            F ("morph", "Morph", Morph, 0, 0.0f),
            F ("spread", "Spread", Spread, 0, 0.0f),
            F ("rotatecv", "Rotate CV", RotateCv, 0, 0.0f),
            F ("scalecv", "Scale CV", ScaleCv, 0, 0.0f),
            F ("nudgeodds", "Freq Nudge (odds)", NudgeOdds, 0, 0.0f),
            F ("nudgeevens", "Freq Nudge (evens)", NudgeEvens, 0, 0.0f),
            { "freqodds", "Freq (odds, octaves)", Kind::Float, Role::FreqOdds, 0, 0.0f, 0.0f, 0.0f, 0, nullptr },
            { "freqevens", "Freq (evens, octaves)", Kind::Float, Role::FreqEvens, 0, 0.0f, 0.0f, 0.0f, 0, nullptr },
            C ("oddsmode", "135 | 1 Switch", OddsMode, kOddsModes, 0.0f),
            C ("evensmode", "246 | 6 Switch", EvensMode, kEvensModes, 0.0f),
            C ("prepost", "Pre | Post Switch", PrePost, kPrePost, 0.0f),
            C ("envspeed", "Fast | Trigger | Slow Switch", EnvSpeed, kEnvSpeed, 2.0f),
            B ("slew", "CV Slew", Slew, 0, 0.0f),
            B ("scalerotation", "Scale Rotation", ScaleRotation, 0, 0.0f),
            B ("rotatecw", "Rotate Clockwise (trigger)", RotateCw, 0, 0.0f),
            B ("rotateccw", "Rotate Counter-clockwise (trigger)", RotateCcw, 0, 0.0f),
            B ("scaleup", "Scale Up (trigger)", ScaleUp, 0, 0.0f),
            B ("scaledown", "Scale Down (trigger)", ScaleDown, 0, 0.0f),
            C ("bank", "Bank", Bank, kBanks, 0.0f),
            B ("lock1", "Lock 1", Lock, 0, 0.0f), B ("lock2", "Lock 2", Lock, 1, 0.0f),
            B ("lock3", "Lock 3", Lock, 2, 0.0f), B ("lock4", "Lock 4", Lock, 3, 0.0f),
            B ("lock5", "Lock 5", Lock, 4, 0.0f), B ("lock6", "Lock 6", Lock, 5, 0.0f),
            C ("filtermode", "Filter Mode", FilterMode, kFilterMode, 0.0f),
            C ("filtertype", "Filter Type", FilterType, kFilterType, 0.0f),
        };
#undef F
#undef B
#undef C

        static_assert (sizeof (kSpecs) / sizeof (kSpecs[0]) == (size_t) kNumParams, "parameter table size");

        bool isOn (const float* v, int i) { return v[i] >= 0.5f; }

        int choiceIndex (const float* v, int i)
        {
            const int n = kSpecs[i].numChoices;
            int k = (int) std::lround (v[i]);
            return k < 0 ? 0 : (k >= n ? n - 1 : k);
        }
    }

    const ParamSpec& spec (int index) { return kSpecs[index]; }

    int indexOfId (const char* id)
    {
        for (int i = 0; i < kNumParams; ++i)
            if (std::strcmp (kSpecs[i].id, id) == 0) return i;
        return -1;
    }

    int indexOfRole (Role role, int channel)
    {
        for (int i = 0; i < kNumParams; ++i)
            if (kSpecs[i].role == role && kSpecs[i].channel == channel) return i;
        return -1;
    }

    float paramMin (int index) { return kSpecs[index].minValue; }

    float paramMax (int index)
    {
        const Role r = kSpecs[index].role;
        if (r == Role::FreqOdds || r == Role::FreqEvens) return freqMaxOctaves();
        return kSpecs[index].maxValue;
    }

    float paramDefault (int index) { return kSpecs[index].defaultValue; }

    uint16_t unitToCounts (float unit)
    {
        if (! (unit == unit)) return 0;
        if (unit < 0.0f) unit = 0.0f;
        if (unit > 1.0f) unit = 1.0f;
        return (uint16_t) std::lround (unit * 4095.0f);
    }

    // params.c: freq_jack_cv = (adc + trackoffset + BASE_TRACKOFFSET) * trackcomp, clamped to 0..4095.
    constexpr int kBaseTrackOffset = 11;   // params.h: BASE_TRACKOFFSET is (-11)

    float freqMaxOctaves()
    {
        // The highest ADC count is 4095, which looks up exp_1voct[4095 - 11]. Rounded down to 0.01.
        const float top = smr::tables().exp_1voct[4095 - kBaseTrackOffset];
        return std::floor (std::log2 (top) * 100.0f) / 100.0f;
    }

    uint16_t octavesToCounts (float octaves)
    {
        if (! (octaves == octaves)) octaves = 0.0f;
        octaves = std::min (std::max (octaves, 0.0f), freqMaxOctaves());

        const float* t = smr::tables().exp_1voct;
        const float target = std::exp2 (octaves);
        const int last = 4095 - kBaseTrackOffset;

        int lo = 0, hi = last;                       // first index with t[idx] >= target
        while (lo < hi)
        {
            const int mid = (lo + hi) / 2;
            if (t[mid] >= target) hi = mid; else lo = mid + 1;
        }
        int idx = lo;
        if (idx > 0 && std::fabs (std::log2 (t[idx - 1]) - octaves) < std::fabs (std::log2 (t[idx]) - octaves))
            --idx;
        return (uint16_t) (idx + kBaseTrackOffset);
    }

    std::string formatValue (int index, float value)
    {
        char buf[48];
        const ParamSpec& s = kSpecs[index];
        switch (s.role)
        {
            case Role::Spread:   std::snprintf (buf, sizeof buf, "%d", (unitToCounts (value) >> 8) + 1); break;   // params.c read_spread: 1..16
            case Role::RotateCv: std::snprintf (buf, sizeof buf, "%d", unitToCounts (value) / 205); break;       // process_rotateCV: 0..19
            case Role::ScaleCv:  std::snprintf (buf, sizeof buf, "%d", unitToCounts (value) / 409); break;       // process_scaleCV: 0..10
            case Role::FreqOdds:
            case Role::FreqEvens: std::snprintf (buf, sizeof buf, "%.2f oct", (double) value); break;
            default:
                if (s.kind == Kind::Float) std::snprintf (buf, sizeof buf, "%d %%", (int) std::lround (value * 100.0f));
                else if (s.kind == Kind::Bool) std::snprintf (buf, sizeof buf, "%s", value >= 0.5f ? "On" : "Off");
                else
                {
                    const int k = std::min (std::max ((int) std::lround (value), 0), s.numChoices - 1);
                    std::snprintf (buf, sizeof buf, "%s", s.choices[k]);
                }
        }
        return buf;
    }

    smr::Controls ParamApplier::buildControls (const float* v)
    {
        smr::Controls c;
        for (int i = 0; i < kNumParams; ++i)
        {
            const ParamSpec& s = kSpecs[i];
            switch (s.role)
            {
                case Role::LevelSlider: c.pot[smr::SLIDER_ADC_BASE + s.channel] = unitToCounts (v[i]); break;
                case Role::LevelCv:     c.adc[smr::LEVEL_ADC_BASE + s.channel] = unitToCounts (v[i]); break;
                case Role::QKnob:       c.pot[smr::QPOT_ADC] = unitToCounts (v[i]); break;
                case Role::QCv:         c.adc[smr::QVAL_ADC] = unitToCounts (v[i]); break;
                case Role::Morph:       c.adc[smr::MORPH_ADC] = unitToCounts (v[i]); break;
                case Role::Spread:      c.adc[smr::SPREAD_ADC] = unitToCounts (v[i]); break;
                case Role::RotateCv:    c.adc[smr::ROTCV_ADC] = unitToCounts (v[i]); break;
                case Role::ScaleCv:     c.adc[smr::SCALE_ADC] = unitToCounts (v[i]); break;
                case Role::NudgeOdds:   c.adc[smr::FREQNUDGE1_ADC] = unitToCounts (v[i]); break;
                case Role::NudgeEvens:  c.adc[smr::FREQNUDGE6_ADC] = unitToCounts (v[i]); break;
                case Role::FreqOdds:    c.adc[smr::FREQCV1_ADC] = octavesToCounts (v[i]); break;
                case Role::FreqEvens:   c.adc[smr::FREQCV6_ADC] = octavesToCounts (v[i]); break;
                case Role::OddsMode:    c.mod135_one = choiceIndex (v, i) == 1; break;
                case Role::EvensMode:   c.mod246_six = choiceIndex (v, i) == 1; break;
                case Role::PrePost:     c.env_mode_pre = choiceIndex (v, i) == 1; break;
                case Role::EnvSpeed:
                    c.env_speed_fast = choiceIndex (v, i) == 0;
                    c.env_speed_slow = choiceIndex (v, i) == 2;
                    break;
                case Role::Slew:          c.cvlag = isOn (v, i); break;
                case Role::ScaleRotation: c.range_mode = isOn (v, i); break;
                default: break;
            }
        }
        c.rotary_sw = false;
        return c;
    }

    void ParamApplier::apply (smr::Engine& e, const float* v, bool initial)
    {
        e.setControls (buildControls (v));

        for (int i = 0; i < kNumParams; ++i)
        {
            const ParamSpec& s = kSpecs[i];
            const bool changed = initial || (v[i] != prev_[i]);

            switch (s.role)
            {
                case Role::Lock:
                    if (changed) e.setLockLatched (s.channel, isOn (v, i));
                    break;

                case Role::FilterMode:
                    if (changed) e.setFilterMode (choiceIndex (v, i) == 1 ? smr::ONEPASS : smr::TWOPASS);
                    break;

                case Role::FilterType:
                    if (changed)
                    {
                        const smr::FilterType want = choiceIndex (v, i) == 1 ? smr::BPRE : smr::MAXQ;
                        const smr::FilterType have = e.filter_type_changed ? e.new_filter_type : e.filter_type;
                        if (want != have) e.changeFilterType (want);
                    }
                    break;

                case Role::Bank:
                    if (! initial && changed) e.setBank (choiceIndex (v, i));
                    break;

                case Role::RotateCw:  if (! initial && ! (prev_[i] >= 0.5f) && isOn (v, i)) e.triggerRotateUp(); break;
                case Role::RotateCcw: if (! initial && ! (prev_[i] >= 0.5f) && isOn (v, i)) e.triggerRotateDown(); break;
                case Role::ScaleUp:   if (! initial && ! (prev_[i] >= 0.5f) && isOn (v, i)) e.change_scale_up(); break;
                case Role::ScaleDown: if (! initial && ! (prev_[i] >= 0.5f) && isOn (v, i)) e.change_scale_down(); break;

                default: break;
            }
        }

        std::memcpy (prev_, v, sizeof prev_);
    }
}
