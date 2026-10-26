// Parameter bank save and load, lock and bank helpers (Milestone 4).
// Source: third_party/SMR system_mode.c store_params_into_sram() / load_params_from_sram(), params.c
// process_lock_buttons() (release branch) and process_rotary_button() (release branch). See docs/open-questions.md M4-b.
#include <cmath>

#include "Engine.h"

namespace smr
{
    void Engine::setLockLatched (int ch, bool on) noexcept
    {
        if ((unsigned) ch >= (unsigned) kNumChannels) return;

        if (on)
        {
            if (lock[ch] == 1) return;
            lock[ch] = 1;
            motion_spread_dir[ch] = 0;
        }
        else
        {
            if (lock[ch] == 0) return;
            lock[ch] = 0;
            q_locked[ch] = 0;
        }
    }

    void Engine::setBank (int bank) noexcept
    {
        if (bank < 0 || bank >= kNumScaleBanks) return;

        hover_scale_bank = (uint8_t) bank;
        for (int i = 0; i < kNumChannels; ++i)
            if (lock[i] != 1)
                scale_bank[i] = (uint8_t) bank;     // "Set all unlocked scale_banks to the same value"
    }

    // system_mode.c store_params_into_sram()
    Snapshot Engine::saveSnapshot() const noexcept
    {
        Snapshot s;
        s.filter_type = (uint8_t) filter_type;
        s.filter_mode = (uint8_t) filter_mode;
        s.freqblock = freqblock;
        for (int i = 0; i < kNumChannels; ++i)
        {
            s.note[i] = note[i];
            s.scale[i] = scale[i];
            s.scale_bank[i] = scale_bank[i];
            s.lock[i] = lock[i];
            s.q_locked[i] = q_locked[i];
            s.qval[i] = qval[i];
            s.freq_nudge[i] = freq_nudge[i];
        }
        s.hover_scale_bank = hover_scale_bank;
        return s;
    }

    // system_mode.c load_params_from_sram()
    bool Engine::loadSnapshot (const Snapshot& s) noexcept
    {
        // Validation is ours, not the firmware's: flash is trusted there, a project file is not.
        if (s.filter_type > 1) return false;
        if (s.filter_mode != TWOPASS && s.filter_mode != ONEPASS) return false;
        if (s.freqblock >> kNumFilts) return false;
        for (int i = 0; i < kNumChannels; ++i)
        {
            if (s.note[i] >= kNumFilts) return false;
            if (s.scale[i] >= kNumScales) return false;
            if (s.scale_bank[i] >= kNumScaleBanks) return false;
            if (s.lock[i] > 1 || s.q_locked[i] > 1) return false;
            if (s.qval[i] > 8191) return false;
            if (! std::isfinite (s.freq_nudge[i]) || s.freq_nudge[i] < 0.0f || s.freq_nudge[i] > 8.0f) return false;
        }

        change_filter_type ((FilterType) s.filter_type);
        filter_mode = (FilterMode) s.filter_mode;
        freqblock = s.freqblock;

        int numFreqBlocked = 0;
        for (int j = 0; j < kNumScaleBanks; ++j)
            if (freqblock & (1u << j)) ++numFreqBlocked;
        if (numFreqBlocked > 14) freqblock = 0;   // "invalid"

        bool alreadySetHover = false;
        for (int i = 0; i < kNumChannels; ++i)
        {
            note[i] = s.note[i];
            scale[i] = s.scale[i];
            scale_bank[i] = s.scale_bank[i];

            lock[i] = s.lock[i];
            q_locked[i] = s.q_locked[i];
            qval[i] = s.qval[i];
            qval_goal[i] = (float) qval[i];
            freq_nudge[i] = s.freq_nudge[i];

            motion_fadeto_scale[i] = (int8_t) scale[i];
            motion_scale_dest[i] = (int8_t) scale[i];
            motion_spread_dir[i] = 0;
            motion_spread_dest[i] = (int8_t) note[i];
            motion_fadeto_note[i] = (int8_t) note[i];

            if (! lock[i] && ! alreadySetHover)
            {
                hover_scale_bank = scale_bank[i];
                alreadySetHover = true;
            }
        }
        if (! alreadySetHover) hover_scale_bank = scale_bank[0];

        return true;
    }
}
