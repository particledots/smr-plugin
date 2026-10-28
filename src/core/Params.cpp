// Port of the DSP-relevant parts of third_party/SMR/params.c: param_read_freq, param_read_q,
// param_read_switches, read_spread, process_rotateCV, process_scaleCV.
// Milestone 7 step 3b: the display-only state of param_read_freq (ongoing_fine_tuning, ongoing_coarse_tuning,
// fine_tuning_timeout, fine_timer, fine_envled, cur_envled_state, saved_envled_state, coarse_adj_led, old_switch_state and the
// "MORE FINE/COARSE TUNE DISPLAY CASES" else-if chain) is now ported too (docs/port-notes.md P-8). None of it is read by
// anything that reaches the audio or the envelope outputs; V1 compares it with the firmware's own globals.
// Every floating-point literal has an f suffix: the firmware is built with -fsingle-precision-constant.
#include <cmath>

#include "Engine.h"

namespace smr
{
    namespace
    {
        constexpr int kBaseTrackOffset = -11;   // params.h BASE_TRACKOFFSET

        inline uint32_t diff (uint32_t a, uint32_t b) { return (a > b) ? (a - b) : (b - a); }   // params.c diff

        // The coarse-tuning ladder that params.c writes out twice (once for t_fo and once for t_fe).
        // Same comparisons in the same order. Like the C, if t is >= 13*4095/13 (that is, exactly 4095)
        // no branch matches and coarse_adj keeps its old value; the function then returns false.
        inline bool coarseLadder (float t, float& coarse_adj, int& envled)
        {
            // - 6 semitones
            if (t < 1.0f * 4095.0f / 13.0f) { coarse_adj = 1.0f / 1.41421356237f; envled = 0b1111111; }
            else if ((t >= 1.0f * 4095.0f / 13.0f) && (t < 2.0f * 4095.0f / 13.0f)) { coarse_adj = 1.0f / 1.33483985417f; envled = 0b1011111; }
            else if ((t >= 2.0f * 4095.0f / 13.0f) && (t < 3.0f * 4095.0f / 13.0f)) { coarse_adj = 1.0f / 1.25992104989f; envled = 0b1001111; }
            else if ((t >= 3.0f * 4095.0f / 13.0f) && (t < 4.0f * 4095.0f / 13.0f)) { coarse_adj = 1.0f / 1.189207115f; envled = 0b1000111; }
            else if ((t >= 4.0f * 4095.0f / 13.0f) && (t < 5.0f * 4095.0f / 13.0f)) { coarse_adj = 1.0f / 1.12246204831f; envled = 0b1000011; }
            else if ((t >= 5.0f * 4095.0f / 13.0f) && (t < 6.0f * 4095.0f / 13.0f)) { coarse_adj = 1.0f / 1.05946309436f; envled = 0b1000001; }

            //Tuned
            else if ((t >= 6.0f * 4095.0f / 13.0f) && (t < 7.0f * 4095.0f / 13.0f)) { coarse_adj = 1.0f; envled = 0b0001100; }

            // + 6 semitones
            else if ((t >= 7.0f * 4095.0f / 13.0f) && (t < 8.0f * 4095.0f / 13.0f)) { coarse_adj = 1.05946309436f; envled = 0b0100000; }
            else if ((t >= 8.0f * 4095.0f / 13.0f) && (t < 9.0f * 4095.0f / 13.0f)) { coarse_adj = 1.12246204831f; envled = 0b0110000; }
            else if ((t >= 9.0f * 4095.0f / 13.0f) && (t < 10.0f * 4095.0f / 13.0f)) { coarse_adj = 1.189207115f; envled = 0b0111000; }
            else if ((t >= 10.0f * 4095.0f / 13.0f) && (t < 11.0f * 4095.0f / 13.0f)) { coarse_adj = 1.25992104989f; envled = 0b0111100; }
            else if ((t >= 11.0f * 4095.0f / 13.0f) && (t < 12.0f * 4095.0f / 13.0f)) { coarse_adj = 1.33483985417f; envled = 0b0111110; }
            else if ((t >= 12.0f * 4095.0f / 13.0f) && (t < 13.0f * 4095.0f / 13.0f)) { coarse_adj = 1.41421356237f; envled = 0b0111111; }
            else return false;
            return true;
        }
    }

    // params.c param_read_freq
    void Engine::param_read_freq()
    {
        const Tables& tb = tables();

        uint8_t i, j, k;
        int odds[3]  = { 0, 2, 4 };   // ch 1, 3, 5
        int evens[3] = { 1, 3, 5 };   // ch 2, 4, 6
        int32_t freq_jack_cv;

        auto& s = prf;

        // FREQ SHIFT
        //With the Maxq filter, the Freq Nudge pot alone adjusts the "nudge", and the CV jack is 1V/oct shift
        //With the BpRe filter, the Freq Nudge pot plus CV jack adjusts the "nudge", and there is no 1V/oct shift

        if (filter_type == MAXQ)
        {
            // Read buffer knob and normalize input: 0-1
            t_fo = (float) (adc_buffer[FREQNUDGE1_ADC]);
            t_fe = (float) (adc_buffer[FREQNUDGE6_ADC]);

            // after turning on the module
            // keep freq nudge knobs output at 0 as long as knobs are within sleep range
            if (! s.sleep_range_saved)
            {
                s.wakeup_odds[0]  = t_fo - s.sleep_range;
                s.wakeup_odds[1]  = t_fo + s.sleep_range;
                s.wakeup_evens[0] = t_fe - s.sleep_range;
                s.wakeup_evens[1] = t_fe + s.sleep_range;
                s.sleep_range_saved = 1;   // ensures this only gets computed once
            }
            if (s.first_run[0])
            {
                if ((t_fo > s.wakeup_odds[0]) && (t_fo < s.wakeup_odds[1])) { t_fo = 0.0f; }
                else { s.first_run[0] = 0; }
            }
            if (s.first_run[1])
            {
                if ((t_fe > s.wakeup_evens[0]) && (t_fe < s.wakeup_evens[1])) { t_fe = 0.0f; }
                else { s.first_run[1] = 0; }
            }

            // Freq shift odds
            // is odds cv input Low-passed and adjusted for 1V/Oct
            if (trackcomp[0] < 0.5f || trackcomp[0] > 2.0f) trackcomp[0] = 1.0f;   //sanity check

            freq_jack_cv = f2i32 ((float) (adc_buffer[FREQCV1_ADC] + trackoffset[0] + kBaseTrackOffset) * trackcomp[0]);
            if (freq_jack_cv < 0) freq_jack_cv = 0;
            if (freq_jack_cv > 4095) freq_jack_cv = 4095;

            freq_jack_conditioning[0].raw_val = (uint16_t) freq_jack_cv;
            apply_fir_lpf (&(freq_jack_conditioning[0]));
            apply_bracket (&(freq_jack_conditioning[0]));

            s.f_shift_odds = tb.exp_1voct[(uint32_t) (uint16_t) freq_jack_conditioning[0].bracketed_val & 4095u];


            // Freq shift evens
            // is odds cv input Low-passed and adjusted for 1V/Oct
            if (trackcomp[1] < 0.5f || trackcomp[1] > 2.0f) trackcomp[1] = 1.0f;   //sanity check

            freq_jack_cv = f2i32 ((float) (adc_buffer[FREQCV6_ADC] + trackoffset[1] + kBaseTrackOffset) * trackcomp[1]);
            if (freq_jack_cv < 0) freq_jack_cv = 0;
            if (freq_jack_cv > 4095) freq_jack_cv = 4095;

            freq_jack_conditioning[1].raw_val = (uint16_t) freq_jack_cv;
            apply_fir_lpf (&(freq_jack_conditioning[1]));
            apply_bracket (&(freq_jack_conditioning[1]));

            s.f_shift_evens = tb.exp_1voct[(uint32_t) (uint16_t) freq_jack_conditioning[1].bracketed_val & 4095u];


            // FREQ NUDGE
            // SEMITONE FINE TUNE
            f_nudge_odds  = 1.0f + t_fo / 55000.0f;   // goes beyond semitone
            f_nudge_evens = 1.0f + t_fe / 55000.0f;

            // 12-SEMITONE COARSE TUNE
            // [freq nudge knob] + [lock button] -> coarse tune

            // ODDS
            if (std::fabs (f_nudge_odds - s.old_f_nudge_odds) > 0.0029304029304f)   // NUDGEPOT_MIN_CHANGE
            {
                // inform led_ring_c that fine tuning is ongoing
                ongoing_fine_tuning[0] = 1;

                // update buffers
                s.f_nudge_odds_buf = s.old_f_nudge_odds;
                s.old_f_nudge_odds = f_nudge_odds;

                // enable fine tuning timer for slow display release
                fine_timer[0] = 1;

                // for each odd channel
                for (i = 0; i < 3; i++)
                {
                    j = (uint8_t) odds[i];

                    //update fine tuning env led
                    fine_envled = fine_envled | (1 << (5 - j));

                    // inform led_ring_c that fine tuning is ongoing on odds chan(s)
                    ongoing_fine_tuning[0] = 1;
                    fine_tuning_timeout[0] = 1;   // stage timeout counter

                    // If lock button pressed, disable fine tuning and process coarse tuning
                    if (lock_pressed[j])
                    {
                        // inform led_ring_c that coarse tuning is ongoing
                        ongoing_coarse_tuning[0] = 1;

                        // disable fine tuning timer for instantaneous display release
                        fine_timer[0] = 0;

                        // coarse tuning superseds fine tuning
                        ongoing_fine_tuning[0] = 0;

                        // disable fine adjustments when setting coarse tuning
                        if (s.fknob_lock[0] == 0)
                        {
                            s.f_nudge_buf[0] = s.f_nudge_odds_buf;
                            s.fknob_lock[0] = 1;
                        }
                        // Compute and apply coarse tuning
                        coarseLadder (t_fo, coarse_adj[j], cur_envled_state);

                        // apply/clear coarse lock as needed
                        if (std::fabs (coarse_adj[j] - s.saved_coarse_adj[j]) < 0.001f) { s.coarse_lock[j] = 0; }
                        if (s.coarse_lock[j]) { coarse_adj[j] = s.saved_coarse_adj[j]; cur_envled_state = saved_envled_state[j]; }
                        else { s.saved_coarse_adj[j] = coarse_adj[j]; saved_envled_state[j] = cur_envled_state; }

                        // apply coarse adjustment (in case channel is locked)
                        freq_nudge[j] = s.f_nudge_buf[0] * coarse_adj[j];

                        already_handled_lock_release[j] = 1;   //set this flag so that we don't do anything when the button is released
                    }
                    else { s.coarse_lock[j] = 1; }
                }
            }

            // EVENS
            if (std::fabs (f_nudge_evens - s.old_f_nudge_evens) > 0.0029304029304f)   // NUDGEPOT_MIN_CHANGE
            {
                // inform led_ring_c that fine tuning is ongoing
                ongoing_fine_tuning[1] = 1;

                // update buffers
                s.f_nudge_evens_buf = s.old_f_nudge_evens;
                s.old_f_nudge_evens = f_nudge_evens;

                // enable fine tuning timer for slow display release
                fine_timer[1] = 1;

                // for each even channel
                for (i = 0; i < 3; i++)
                {
                    j = (uint8_t) evens[i];

                    //update fine tuning env led
                    fine_envled = fine_envled | (1 << (5 - j));

                    // inform led_ring_c that fine tuning is ongoing on evens chan(s)
                    ongoing_fine_tuning[1] = 1;
                    fine_tuning_timeout[1] = 1;   // stage timeout counter

                    // If lock button pressed, disable fine tuning and process coarse tuning
                    if (lock_pressed[j])
                    {
                        // inform led_ring_c that coarse tuning is ongoing
                        ongoing_coarse_tuning[1] = 1;

                        // disable fine tuning timer for instantaneous display release
                        fine_timer[1] = 0;

                        // coarse tuning superseds fine tuning
                        ongoing_fine_tuning[1] = 0;

                        // disable fine adjustments when setting coarse tuning
                        if (s.fknob_lock[1] == 0)
                        {
                            s.f_nudge_buf[1] = s.f_nudge_evens_buf;
                            s.fknob_lock[1] = 1;
                        }
                        // Compute and apply coarse tuning
                        coarseLadder (t_fe, coarse_adj[j], cur_envled_state);

                        // apply/clear coarse lock as needed
                        if (std::fabs (coarse_adj[j] - s.saved_coarse_adj[j]) < 0.001f) { s.coarse_lock[j] = 0; }
                        if (s.coarse_lock[j]) { coarse_adj[j] = s.saved_coarse_adj[j]; cur_envled_state = saved_envled_state[j]; }
                        else { s.saved_coarse_adj[j] = coarse_adj[j]; saved_envled_state[j] = cur_envled_state; }

                        // apply coarse adjustment (in case channel is locked)
                        freq_nudge[j] = s.f_nudge_buf[1] * coarse_adj[j];

                        already_handled_lock_release[j] = 1;
                    }
                    else { s.coarse_lock[j] = 1; }
                }
            }

            // MORE FINE/COARSE TUNE DISPLAY CASES
            // stop displaying fine tune if fine tune knob is not being adjusted but leave display on for timed duration.
            // NOTE: in params.c this `else if` chain belongs to the EVENS `if` above, so the timeouts only count on passes
            // where the Evens knob did not move (docs/open-questions.md M7-g).
            // special case: both fine tunes are ajusted at once. Leave more time to facilitate tuning odds/even together
            else if ((ongoing_fine_tuning[0] == 1) && (ongoing_fine_tuning[1] == 1))
            {
                fine_tuning_timeout[0] += 1;
                fine_tuning_timeout[1] += 1;
                if ((fine_tuning_timeout[0] > (kDisplayTimeNudgeBothSides * fine_timer[0])) && (fine_tuning_timeout[1] > (kDisplayTimeNudgeBothSides * fine_timer[1])))
                {
                    ongoing_fine_tuning[0] = 0;
                    ongoing_fine_tuning[1] = 0;
                    fine_envled = fine_envled & 0b000000;   // turn off all env led
                    fine_tuning_timeout[0] = 0;
                    fine_tuning_timeout[1] = 0;
                }
            // turn off odds fine tune display after short time
            }
            else if ((ongoing_fine_tuning[0] == 1) && (ongoing_fine_tuning[1] == 0))
            {
                fine_tuning_timeout[0] += 1;
                if (fine_tuning_timeout[0] > (kDisplayTimeNudgeOneSide * fine_timer[0]))
                {
                    ongoing_fine_tuning[0] = 0;
                    fine_envled = fine_envled & 0b010101;   // turn off odds env led
                    fine_tuning_timeout[0] = 0;
                }
            // turn off evens fine tune display after short time
            }
            else if ((ongoing_fine_tuning[1] == 1) && (ongoing_fine_tuning[0] == 0))
            {
                fine_tuning_timeout[1] += 1;
                if (fine_tuning_timeout[1] > (kDisplayTimeNudgeOneSide * fine_timer[1]))
                {
                    ongoing_fine_tuning[1] = 0;
                    fine_envled = fine_envled & 0b101010;   // turn off even env led
                    fine_tuning_timeout[1] = 0;
                }
            }

            // display fine tune when locked button is held down
            if (lock_pressed[0] || lock_pressed[2] || lock_pressed[4])
            {
                fine_envled = fine_envled | 0b101010;
                ongoing_fine_tuning[0] = 1;
                fine_tuning_timeout[0] = 1;
                if (! ongoing_coarse_tuning[0]) fine_timer[0] = 1;   //enable soft release
            }
            if (lock_pressed[1] || lock_pressed[3] || lock_pressed[5])
            {
                fine_envled = fine_envled | 0b010101;
                ongoing_fine_tuning[1] = 1;
                fine_tuning_timeout[1] = 1;
                if (! ongoing_coarse_tuning[1]) fine_timer[1] = 1;   //enable soft release
            }

            // display fine tune when lock switch state is changed
            // switch 135
            if (mod_mode_135 != s.old_switch_state[0])
            {
                s.old_switch_state[0] = (uint8_t) mod_mode_135;
                fine_envled = fine_envled | 0b101010;
                ongoing_fine_tuning[0] = 1;
                fine_tuning_timeout[0] = 1;   //reset the timer so that we get the full soft-release period
                fine_timer[0] = 1;            //enable soft release
            }
            // switch 246
            if (mod_mode_246 != s.old_switch_state[1])
            {
                s.old_switch_state[1] = (uint8_t) mod_mode_246;
                fine_envled = fine_envled | 0b010101;
                ongoing_fine_tuning[1] = 1;
                fine_tuning_timeout[1] = 1;   //reset the timer so that we get the full soft-release period
                fine_timer[1] = 1;            //enable soft release
            }

            // exit coarse tuning display as needed
            if (ongoing_coarse_tuning[0])
            {
                if (! lock_pressed[0] && ! lock_pressed[2] && ! lock_pressed[4])
                {
                    ongoing_coarse_tuning[0] = 0;
                }
            }
            if (ongoing_coarse_tuning[1])
            {
                if (! lock_pressed[1] && ! lock_pressed[3] && ! lock_pressed[5])
                {
                    ongoing_coarse_tuning[1] = 0;
                }
            }

            // LOCK SWITCHES
            // nudge and shift always enabled on 1 and 6
            // ... and enabled on 3,5,2 and 4 based on the lock toggles
            // ODDS
            // enable freq nudge and shift for "135 mode"
            if (! lock[0])
            {
                // Prevent fknob from adjusting freq_nudge when it's fknob_lock(ed)
                if (s.fknob_lock[0] == 1)
                {
                    // during coarse adj
                    if ((std::fabs (f_nudge_odds - s.f_nudge_buf[0]) < 0.001f) && ! lock_pressed[0]) { s.fknob_lock[0] = 0; }
                    else { freq_nudge[0] = s.f_nudge_buf[0] * coarse_adj[0]; }
                }
                if (s.fknob_lock[0] == 0) { freq_nudge[0] = f_nudge_odds * coarse_adj[0]; }
            }
            freq_shift[0] = s.f_shift_odds;   // apply freq CV in

            if (mod_mode_135 == 135)
            {
                if (! lock[2])
                {
                    if (s.fknob_lock[0] == 1)
                    {
                        if ((std::fabs (f_nudge_odds - s.f_nudge_buf[0]) < 0.001f) && ! lock_pressed[2]) { s.fknob_lock[0] = 0; }
                        else { freq_nudge[2] = s.f_nudge_buf[0] * coarse_adj[2]; }
                    }
                    if (s.fknob_lock[0] == 0) { freq_nudge[2] = f_nudge_odds * coarse_adj[2]; }
                }
                freq_shift[2] = s.f_shift_odds;

                if (! lock[4])
                {
                    if (s.fknob_lock[0] == 1)
                    {
                        if ((std::fabs (f_nudge_odds - s.f_nudge_buf[0]) < 0.001f) && ! lock_pressed[4]) { s.fknob_lock[0] = 0; }
                        else { freq_nudge[4] = s.f_nudge_buf[0] * coarse_adj[4]; }
                    }
                    if (s.fknob_lock[0] == 0) { freq_nudge[4] = f_nudge_odds * coarse_adj[4]; }
                }
                freq_shift[4] = s.f_shift_odds;
            }
            // disable freq nudge and shift on channel 3 and 5 when in "1 mode"
            else
            {
                if (! lock[2])
                {
                    freq_nudge[2] = coarse_adj[2];
                }
                freq_shift[2] = 1.0f;

                if (! lock[4])
                {
                    freq_nudge[4] = coarse_adj[4];
                }
                freq_shift[4] = 1.0f;
            }

            //EVENS
            if (! lock[5])
            {
                if (s.fknob_lock[1] == 1)
                {
                    if ((std::fabs (f_nudge_evens - s.f_nudge_buf[1]) < 0.001f) && ! lock_pressed[5]) { s.fknob_lock[1] = 0; }
                    else { freq_nudge[5] = s.f_nudge_buf[1] * coarse_adj[5]; }
                }
                if (s.fknob_lock[1] == 0) { freq_nudge[5] = f_nudge_evens * coarse_adj[5]; }
            }
            freq_shift[5] = s.f_shift_evens;

            if (mod_mode_246 == 246)
            {
                if (! lock[1])
                {
                    if (s.fknob_lock[1] == 1)
                    {
                        if ((std::fabs (f_nudge_evens - s.f_nudge_buf[1]) < 0.001f) && ! lock_pressed[1]) { s.fknob_lock[1] = 0; }
                        else { freq_nudge[1] = s.f_nudge_buf[1] * coarse_adj[1]; }
                    }
                    if (s.fknob_lock[1] == 0) { freq_nudge[1] = f_nudge_evens * coarse_adj[1]; }
                }
                freq_shift[1] = s.f_shift_evens;

                if (! lock[3])
                {
                    if (s.fknob_lock[1] == 1)
                    {
                        if ((std::fabs (f_nudge_evens - s.f_nudge_buf[1]) < 0.001f) && ! lock_pressed[3]) { s.fknob_lock[1] = 0; }
                        else { freq_nudge[3] = s.f_nudge_buf[1] * coarse_adj[3]; }
                    }
                    if (s.fknob_lock[1] == 0) { freq_nudge[3] = f_nudge_evens * coarse_adj[3]; }
                }
                freq_shift[3] = s.f_shift_evens;
            }
            // disable freq nudge and shift on channel 2 and 4 when in "6 mode"
            else
            {
                if (! lock[3])
                {
                    freq_nudge[3] = coarse_adj[3];
                }
                freq_shift[3] = 1.0f;

                if (! lock[1])
                {
                    freq_nudge[1] = coarse_adj[1];
                }
                freq_shift[1] = 1.0f;
            }

            // CLEAR COARSE TUNING AT BUTTON RELEASE AFTER 6 BUTTON PRESS (~3s)
            // if locked buttons have all been pressed for +3s
            if (lock_down[0] > 36000 &&
                lock_down[1] > 36000 &&
                lock_down[2] > 36000 &&
                lock_down[3] > 36000 &&
                lock_down[4] > 36000 &&
                lock_down[5] > 36000)
            {
                s.clear_coarse_staged[0] = 1;
                s.clear_coarse_staged[1] = 1;
                s.clear_coarse_staged[2] = 1;
                s.clear_coarse_staged[3] = 1;
                s.clear_coarse_staged[4] = 1;
                s.clear_coarse_staged[5] = 1;
                num_clear_coarse_staged = 6;
            }
            else
            {
                coarse_adj_led[0] = coarse_adj[0];
                coarse_adj_led[1] = coarse_adj[1];
                coarse_adj_led[2] = coarse_adj[2];
                coarse_adj_led[3] = coarse_adj[3];
                coarse_adj_led[4] = coarse_adj[4];
                coarse_adj_led[5] = coarse_adj[5];
            }

            if (num_clear_coarse_staged != 0)
            {
                for (k = 0; k < 6; k++)
                {
                    if (s.clear_coarse_staged[k])
                    {
                        // do not change lock states upon button release
                        already_handled_lock_release[k] = 1;
                        if (! lock_pressed[k])
                        {
                            // clear coarse adjustment
                            coarse_adj[k] = 1.0f;

                            //update saved coarse adjustement & lock to it
                            s.saved_coarse_adj[k] = coarse_adj[k];
                            saved_envled_state[k] = 0b0001100;
                            s.coarse_lock[k] = 1;

                            // inform led.c that this coarse tuning was cleared
                            coarse_adj_led[k] = coarse_adj[k];

                            // unstage coarse adjustment
                            s.clear_coarse_staged[k] = 0;
                            // remove channel from list of adjustments to be cleared
                            num_clear_coarse_staged -= 1;
                            //
                            already_handled_lock_release[k] = 0;
                        }
                    }
                }
            }
        }
        else
        {
            t_fo = (float) (adc_buffer[FREQNUDGE1_ADC] + adc_buffer[FREQCV1_ADC]) / 4096.0f;
            if (t_fo > 1.0f) t_fo = 1.0f;

            t_fe = (float) (adc_buffer[FREQNUDGE6_ADC] + adc_buffer[FREQCV6_ADC]) / 4096.0f;
            if (t_fe > 1.0f) t_fe = 1.0f;

            s.f_shift_odds = 1.0f;
            s.f_shift_evens = 1.0f;

            f_nudge_odds *= 0.995f;                    // FREQNUDGE_LPF
            f_nudge_odds += (1.0f - 0.995f) * t_fo;

            f_nudge_evens *= 0.995f;
            f_nudge_evens += (1.0f - 0.995f) * t_fe;

            if (! lock[0])
            {
                freq_nudge[0] = f_nudge_odds;
            }
            freq_shift[0] = s.f_shift_odds;

            if (mod_mode_135 == 135)
            {
                if (! lock[2])
                {
                    freq_nudge[2] = f_nudge_odds;
                }
                freq_shift[2] = s.f_shift_odds;

                if (! lock[4])
                {
                    freq_nudge[4] = f_nudge_odds;
                }
                freq_shift[4] = s.f_shift_odds;
            }
            else
            {
                if (! lock[2])
                {
                    freq_nudge[2] = 0.0f;
                }
                freq_shift[2] = 1.0f;

                if (! lock[4])
                {
                    freq_nudge[4] = 0.0f;
                }
                freq_shift[4] = 1.0f;
            }

            if (! lock[5])
            {
                freq_nudge[5] = f_nudge_evens;
            }
            freq_shift[5] = s.f_shift_evens;

            if (mod_mode_246 == 246)
            {
                if (! lock[1])
                {
                    freq_nudge[1] = f_nudge_evens;
                }
                freq_shift[1] = s.f_shift_evens;

                if (! lock[3])
                {
                    freq_nudge[3] = f_nudge_evens;
                }
                freq_shift[3] = s.f_shift_evens;
            }
            else
            {
                if (! lock[1])
                {
                    freq_nudge[1] = 0.0f;
                }
                freq_shift[1] = 1.0f;

                if (! lock[3])
                {
                    freq_nudge[3] = 0.0f;
                }
                freq_shift[3] = 1.0f;
            }
        }
    }

    // params.c param_read_q
    void Engine::param_read_q()
    {
        int32_t t, i, num_locked;
        auto& s = prq;

        if (s.poll_ctr++ > 15)   // UPDATE RATE
        {
            s.poll_ctr = 0;

            //Check jack + LPF
            t = adc_buffer[QVAL_ADC];
            if (diff ((uint32_t) t, old_adc_buffer[QVAL_ADC]) > 15)
            {
                old_adc_buffer[QVAL_ADC] = adc_buffer[QVAL_ADC];
                t = adc_buffer[QVAL_ADC];

                qvalcv = f2u32 ((float) qvalcv * 0.95f);                       // QCV_LPF
                qvalcv = f2u32 ((float) qvalcv + (1.0f - 0.95f) * (float) t);
            }


            //Check pot + LPF
            t = potadc_buffer[QPOT_ADC];
            s.qpot_lpf *= 0.95f;                                              // QPOT_LPF
            s.qpot_lpf += (1.0f - 0.95f) * (float) t;

            num_locked = 0;

            if (std::fabs (s.qpot_lpf - s.old_qpot_lpf) > 100.0f)             // QPOT_MIN_CHANGE
            {
                s.old_qpot_lpf = s.qpot_lpf;

                // ui_mode==PLAY is always true in this port (Milestone 7 adds the other UI modes)
                for (i = 0; i < 6; i++)
                {
                    if (lock_pressed[i])   //if lock button is being held down, then q_lock the channel and assign its qval
                    {
                        q_locked[i] = 1;
                        user_turned_Q_pot = 1;

                        s.prev_qval[i] = qval_goal[i];
                        qval_goal[i] = s.qpot_lpf;

                        num_locked++;
                    }
                }
                //otherwise, if no lock buttons were held down, then change the qvalpot (which effects all non-q_locked channels)
            }

            if (! num_locked) qvalpot = f2u32 (s.qpot_lpf);

            for (i = 0; i < kNumChannels; i++)
            {
                if (! q_locked[i])
                {
                    s.prev_qval[i] = qval_goal[i];
                    qval_goal[i] = (float) (qvalcv + qvalpot);
                    if (qval_goal[i] > 4095.0f) qval_goal[i] = 4095.0f;
                }
            }
        }

        // SMOOTH OUT DATA BETWEEN ADC READS
        for (i = 0; i < kNumChannels; i++)
        {
            // smooth output of channels that are not q-locked
            if (! q_locked[i])
            {
                qval[i] = f2u32 (s.prev_qval[i] + ((float) s.poll_ctr * (qval_goal[i] - s.prev_qval[i]) / 15.0f));

            // use locked qval for q-locked channels
            }
            else
            {
                qval[i] = f2u32 (qval_goal[i]);
            }
        }
    }

    // params.c param_read_switches
    void Engine::param_read_switches()
    {
        uint32_t lag_val;
        const Tables& tb = tables();

        /*** Read Switches ***/

        //PRE|POST switch
        if (sw.env_mode_pre)
        {
            env_prepost_mode = PRE;
        }
        else
        {
            env_prepost_mode = POST;
        }


        if (sw.mod246_six)
        {
            mod_mode_246 = 6;
        }
        else
        {
            mod_mode_246 = 246;
        }

        if (sw.mod135_one)
        {
            mod_mode_135 = 1;
        }
        else
        {
            mod_mode_135 = 135;
        }

        if (sw.range_mode)
        {
            rotate_to_next_scale = 1;
        }
        else
        {
            rotate_to_next_scale = 0;
        }

        if (sw.env_speed_fast)
        {
            if (env_track_mode != ENV_VOLTOCT && env_track_mode != ENV_FAST)   //changed state
            {
                if (sw.rotary_sw)
                    env_track_mode = ENV_VOLTOCT;
                else
                    env_track_mode = ENV_FAST;

                envspeed_attack = 0.9990f;
                envspeed_decay = 0.9991f;
            }
        }
        else
        {
            if (sw.env_speed_slow)
            {
                env_track_mode = ENV_SLOW;
                envspeed_attack = 0.9995f;
                envspeed_decay = 0.9999f;
            }
            else   //trigger
            {
                env_track_mode = ENV_TRIG;
                envspeed_attack = 0.0f;
                envspeed_decay = 0.0f;
            }
        }

        lag_val = sw.cvlag ? 1u : 0u;
        if (old_cvlag != lag_val)
        {
            old_cvlag = lag_val;

            if (lag_val)   //CVLAG switch is flipped on, latch the current Morph adc value and use that to calculate LPF coefficients
            {
                //Read from morph pot, and scale to 137..4095
                lag_val = (adc_buffer[MORPH_ADC] / 2) + 137;
                if (lag_val > 4095) lag_val = 4095;

                CHANNEL_LEVEL_LPF = 1.0f - tb.exp_4096[lag_val];
            }
            else
            {
                CHANNEL_LEVEL_LPF = 0.75f;   // CHANNEL_LEVEL_MIN_LPF
            }
        }
    }

    // params.c read_spread. Returns the new spread (1..16) or -1.
    int8_t Engine::read_spread()
    {
        uint8_t test_spread = 0, hys_spread = 0;
        uint16_t temp_u16;

        // Hysteresis is used to ignore noisy ADC values.
        // We are checking if Spread ADC has changed values enough to warrant a change our "spread" variable.
        // Like a Schmitt trigger, we set a different threshold (temp_u16) depending on if the ADC value is rising or falling.

        test_spread = (uint8_t) ((adc_buffer[SPREAD_ADC] >> 8) + 1);   //0-4095 to 1-16

        if (test_spread < spread)
        {
            if (adc_buffer[SPREAD_ADC] <= (4095 - 75))   // SPREAD_ADC_HYSTERESIS 75
                temp_u16 = (uint16_t) (adc_buffer[SPREAD_ADC] + 75);
            else
                temp_u16 = 4095;

            hys_spread = (uint8_t) ((temp_u16 >> 8) + 1);
        }
        else if (test_spread > spread)
        {
            if (adc_buffer[SPREAD_ADC] > 75)
                temp_u16 = (uint16_t) (adc_buffer[SPREAD_ADC] - 75);
            else
                temp_u16 = 0;

            hys_spread = (uint8_t) ((temp_u16 >> 8) + 1);
        }
        else
        {
            hys_spread = 0xFF;   //adc has not changed, do nothing
        }

        if (hys_spread == test_spread)
        {
            return (int8_t) hys_spread;   //spread has changed
        }
        else return -1;   //spread has not changed
    }

    // params.c process_rotateCV
    void Engine::process_rotateCV()
    {
        int32_t t;

        t = (int16_t) adc_buffer[ROTCV_ADC] - (int16_t) old_rotcv_adc;
        if (t < (-100) || t > 100)
        {
            old_rotcv_adc = adc_buffer[ROTCV_ADC];

            rot_offset = (int8_t) (adc_buffer[ROTCV_ADC] / 205);   //0..19

            jump_rotate_with_cv ((int8_t) (rot_offset - old_rot_offset));

            old_rot_offset = rot_offset;
        }
    }

    // params.c process_scaleCV
    void Engine::process_scaleCV()
    {
        // apply LPF to scale CV ADC readout
        scalecv_lpf_buf *= 0.99f;                                       // SCALECV_LPF
        scalecv_lpf_buf += (1.0f - 0.99f) * (float) adc_buffer[SCALE_ADC];

        // switch scales according to CV in
        t_scalecv = f2i32 (scalecv_lpf_buf / 409.0f);   //0..10
        jump_scale_with_cv ((int8_t) (t_scalecv - t_old_scalecv));
        t_old_scalecv = t_scalecv;
    }
}
