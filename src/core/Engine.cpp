// Engine lifecycle, scheduling and the main-loop pass. See Engine.h.
#include "Engine.h"

#include <cstring>

namespace smr
{
    Engine::Engine()
    {
        init_table_pointers();
    }

    // c_hiq / c_loq are NULL until the first bank change in the firmware (filter.c). The port starts
    // them at the bank 0 tables so that no pointer is ever null (docs/port-notes.md item P-4).
    void Engine::init_table_pointers() noexcept
    {
        const Tables& t = tables();
        for (int i = 0; i < kNumChannels; ++i)
        {
            c_hiq[i] = t.maxq[0];
            c_loq[i] = t.bpre_lo[0];
        }
    }

    // main.c main(), from the top to the while(1) loop. Flash loading (load_startup_params, factory_reset)
    // is replaced by factory defaults: factory_reset() in system_mode.c stores the values that
    // set_default_param_values() produces, so a unit with an empty flash boots with exactly these.
    void Engine::reset()
    {
        // Keep the controls the host has set; everything else returns to power-up values.
        const Controls keep = sw;
        uint16_t adcKeep[kNumAdcs], potKeep[kNumAdc3s];
        std::memcpy (adcKeep, adc_buffer, sizeof adcKeep);
        std::memcpy (potKeep, potadc_buffer, sizeof potKeep);

        static_cast<EngineState&> (*this) = EngineState();

        sw = keep;
        std::memcpy (adc_buffer, adcKeep, sizeof adcKeep);
        std::memcpy (potadc_buffer, potKeep, sizeof potKeep);
        init_table_pointers();
        scheduler.reset();

        set_default_param_values();
        set_default_user_scalebank();

        spread = (int8_t) ((adc_buffer[SPREAD_ADC] >> 8) + 1);   // main.c, before I2S_Block_PlayRec()

        init_freq_lpf();

        // M2-d: before block 0, one main-loop pass (its first statement is param_read_switches(), which
        // sets mod_mode_135/246, env_track_mode and CHANNEL_LEVEL_LPF) and then one TIM10 tick.
        runLoopPass();
        runTim10Tick();
    }

    void Engine::setControls (const Controls& c) noexcept
    {
        for (int i = 0; i < kNumAdcs; ++i)  adc_buffer[i]    = c.adc[i] > 4095 ? (uint16_t) 4095 : c.adc[i];
        for (int i = 0; i < kNumAdc3s; ++i) potadc_buffer[i] = c.pot[i] > 4095 ? (uint16_t) 4095 : c.pot[i];
        sw = c;
        for (int i = 0; i < kNumAdcs; ++i)  sw.adc[i] = adc_buffer[i];
        for (int i = 0; i < kNumAdc3s; ++i) sw.pot[i] = potadc_buffer[i];
    }

    void Engine::processAudioOnly (const int32_t* inLeft, const int32_t* inRight,
                                   int32_t* outFiltered, int32_t* outFilteredR)
    {
        process_audio_block (inLeft, inRight);
        std::memcpy (outFiltered,  filtered_buffer,  sizeof filtered_buffer);
        std::memcpy (outFilteredR, filtered_bufferR, sizeof filtered_bufferR);
    }

    void Engine::processBlock (const int32_t* inLeft, const int32_t* inRight,
                               int32_t* outFiltered, int32_t* outFilteredR)
    {
        processAudioOnly (inLeft, inRight, outFiltered, outFilteredR);

        // Ticks run between blocks, never inside one: the audio DMA interrupt has the highest priority
        // (docs/architecture.md section 11.1). Fixed order: TIM9, TIM10, then the main loop.
        const DueTicks d = scheduler.advanceOneBlock();
        for (uint32_t n = 0; n < d.tim9; ++n)  runTim9Tick();
        for (uint32_t n = 0; n < d.tim10; ++n) runTim10Tick();
        for (uint32_t n = 0; n < d.loop; ++n)  runLoopPass();
    }

    // envout_pwm.c ENV_update_IRQHandler
    void Engine::runTim9Tick()
    {
        update_ENVOUT_PWM();
    }

    // params.c TIM1_UP_TIM10_IRQHandler
    void Engine::runTim10Tick()
    {
        param_read_freq();
        // update_slider_LEDs();   // LED only: omitted
        update_motion();
    }

    // main.c, the while(1) body, in the firmware's order. Not ported (UI and hardware only):
    // check_errors, update_LED_ring, update_lock_leds, process_freq_blocks, process_rotary_button,
    // process_rotary_rotation, check_rotary_pressed_repeatedly and the edit modes.
    // process_lock_buttons is ported for ui_mode PLAY only (Milestone 7).
    void Engine::runLoopPass()
    {
        int32_t t_spread;

        param_read_switches();

        t_spread = read_spread();
        if (t_spread != -1) update_spread ((int8_t) t_spread);

        process_lock_jacks();

        process_lock_buttons();

        if (do_ROTUP)
        {
            do_ROTUP = 0;
            rotate_up();
        }

        if (do_ROTDOWN)
        {
            do_ROTDOWN = 0;
            rotate_down();
        }

        process_rotateCV();

        process_scaleCV();
    }

    bool Engine::setFreqBlock (uint32_t mask) noexcept
    {
        if (mask >> kNumFilts) return false;
        int n = 0;
        for (int j = 0; j < kNumFilts; ++j)
            if (mask & (1u << j)) ++n;
        if (n > 14) return false;
        freqblock = mask;
        return true;
    }

    // user_scales.c set_default_user_scalebank (data DEFAULT_user_scalebank[21], repeated for 11 scales)
    void Engine::set_default_user_scalebank()
    {
        static const float DEFAULT_user_scalebank[21] = {
            0.02094395102393198f,
            0.0221893431599245f,
            0.02350879016601388f,
            0.02490669557392844f,
            0.02638772476301919f,
            0.02795682053052971f,
            0.02961921958772248f,
            0.03138047003691591f,
            0.03324644988776009f,
            0.03522338667454755f,
            0.03731787824003011f,
            0.03953691475510571f,
            0.04188790204786397f,
            0.04437868631984903f,
            0.04701758033202778f,
            0.0498133911478569f,
            0.0527754495260384f,
            0.05591364106105944f,
            0.05923843917544499f,
            0.06276094007383184f,
            0.06649289977552018f
        };

        uint8_t i, j;

        for (j = 0; j < kNumScales; j++)
        {
            for (i = 0; i < kNumFilts + 1; i++)
                user_scalebank[i + j * 21] = DEFAULT_user_scalebank[i];
        }
    }

    // params.c set_default_param_values. rot_dir[] is written there but read nowhere (grep), so it is omitted.
    void Engine::set_default_param_values()
    {
        uint8_t i;

        //Set default parameter values
        for (i = 0; i < kNumChannels; i++)
        {
            note[i] = (uint8_t) (i + 5);
            scale[i] = 6;
            motion_fadeto_scale[i] = (int8_t) scale[i];
            motion_scale_dest[i] = (int8_t) scale[i];
            scale_bank[i] = 0;
            motion_spread_dir[i] = 0;
            motion_spread_dest[i] = (int8_t) note[i];
            motion_fadeto_note[i] = (int8_t) note[i];

            motion_morphpos[i] = 0;
            freq_shift[i] = 0;
            motion_scalecv_overage[i] = 0;
        }
        motion_notejump = 0;
        motion_rotate = 0;
        filter_type = MAXQ;
        filter_mode = TWOPASS;

        trackcomp[0] = 1.0f;
        trackcomp[1] = 1.0f;
        trackoffset[0] = 0;
        trackoffset[1] = 0;
    }

    // params.c init_freq_lpf
    void Engine::init_freq_lpf()
    {
        freq_jack_conditioning[0].polarity      = AP_UNIPOLAR;
        freq_jack_conditioning[0].fir_lpf_size  = 40;
        freq_jack_conditioning[0].iir_lpf_size  = 0;
        freq_jack_conditioning[0].bracket_size  = 2;

        freq_jack_conditioning[1].polarity      = AP_UNIPOLAR;
        freq_jack_conditioning[1].fir_lpf_size  = 40;
        freq_jack_conditioning[1].iir_lpf_size  = 0;
        freq_jack_conditioning[1].bracket_size  = 2;

        setup_fir_filter (&freq_jack_conditioning[0]);
        setup_fir_filter (&freq_jack_conditioning[1]);
    }

    // params.c process_lock_jacks (LED calls omitted)
    void Engine::process_lock_jacks()
    {
        if (do_LOCK135)
        {
            do_LOCK135 = 0;

            lock[0] = (uint8_t) (1 - lock[0]);

            if (mod_mode_135 == 135)
            {
                lock[2] = (uint8_t) (1 - lock[2]);

                lock[4] = (uint8_t) (1 - lock[4]);
            }
        }

        if (do_LOCK246)
        {
            do_LOCK246 = 0;

            lock[5] = (uint8_t) (1 - lock[5]);

            if (mod_mode_246 == 246)
            {
                lock[1] = (uint8_t) (1 - lock[1]);

                lock[3] = (uint8_t) (1 - lock[3]);
            }
        }
    }

    // params.c process_lock_buttons, ui_mode PLAY only (ui_mode is always PLAY in this port; the SELECT_PARAMS, EDIT_COLORS and
    // EDIT_SCALES branches belong to System Mode, which is not ported). LOCKLED_ON/OFF are omitted (the window reads lock[] and q_locked[]).
    // One call is one main-loop pass: the counts below are passes, not time.
    void Engine::process_lock_buttons()
    {
        uint8_t i;
        for (i = 0; i < 6; i++)
        {
            if (lock_button[i])
            {
                lock_up[i] = 1;
                if (lock_down[i] != 0
                    && lock_down[i] != 0xFFFFFFFF)   //don't wrap our counter!
                    lock_down[i]++;

                if (lock_down[i] == 5)   //first time we notice lock button is solidly down...
                {
                    lock_pressed[i] = 1;
                    user_turned_Q_pot = 0;
                    already_handled_lock_release[i] = 0;
                }

                //check to see if it's been held down for a while, and the user hasn't turned the Q pot
                //if so, then we should unlock immediately, but not unlock the q_lock
                if (lock_down[i] > kLockButtonQunlockHoldTime && lock[i] && ! user_turned_Q_pot && q_locked[i])
                {
                    lock[i] = 0;
                    already_handled_lock_release[i] = 1;   //set this flag so that we don't do anything when the button is released
                    lock_down[i] = 0;                      //stop checking this button until it's released
                }
            }
            else
            {
                if (lock_up[i] != 0) lock_up[i]++;
                if (lock_up[i] > 5)
                {
                    lock_up[i] = 0;
                    //Handle button release

                    lock_pressed[i] = 0;

                    if (! user_turned_Q_pot && ! already_handled_lock_release[i])   //only change lock state if user did not do a q_lock
                    {
                        if (lock[i] == 0)
                        {
                            lock[i] = 1;
                            motion_spread_dir[i] = 0;
                        }
                        else
                        {
                            lock[i] = 0;
                            q_locked[i] = 0;
                        }
                    }
                }

                lock_down[i] = 1;
            }
        }
    }
}
