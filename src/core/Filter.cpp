// Port of third_party/SMR/filter.c: process_audio_block() and change_filter_type().
// Statement order, operand order and float/int conversions follow the C source. Differences are
// listed in docs/port-notes.md. Every floating-point literal carries an f suffix (the firmware is
// built with -fsingle-precision-constant, so each bare literal there is a float).
#include "Engine.h"

namespace smr
{
    namespace
    {
        // Clamp a table index. Not in the firmware: it only guards memory in states the firmware would
        // reach through undefined behaviour (docs/port-notes.md item P-5).
        inline uint32_t clampIdx (uint32_t i, uint32_t maxIdx) noexcept { return i > maxIdx ? maxIdx : i; }

        // twopass_calibration is declared [3380]; indices past it read as 0 (docs/port-notes.md item P-5).
        inline float twopassCal (const Tables& t, uint32_t idx) noexcept
        {
            return idx < (uint32_t) kTwopassCalSize ? (float) t.twopass_calibration[idx] : 0.0f;
        }
    }

    // filter.c change_filter_type
    void Engine::change_filter_type (FilterType newtype) noexcept
    {
        filter_type_changed = 1;
        new_filter_type = newtype;
    }

    // filter.c, the body of the bank-change block at the top of process_audio_block (lines 204 to 340).
    void Engine::select_tables_for_channel (int i)
    {
        const Tables& tb = tables();
        float* ff;
        int16_t j;

        //Also clear the buf[] history if we changed scales or banks, so we don't get artifacts.
        // This reproduces filter.c exactly, including M1-p: it zeroes floats 0..221 of buf[i] (660 floats)
        // and never touches buf_a.
        ff = &buf[bufIdx (i, 0, 0, 0)];
        for (j = 0; j < (kNumScales * kNumFilts); j++)
        {
            *(ff + j) = 0;
            *(ff + j + 1) = 0;
            *(ff + j + 2) = 0;
        }

        if (filter_type == MAXQ)
        {
            // Banks 0..18 are the stock tables in filter.c's order; the last bank is the user bank.
            // Any other value (0xFF) leaves c_hiq unchanged, as in the C chain of else-ifs.
            if (scale_bank[i] < kNumStockBanks)
                c_hiq[i] = tb.maxq[scale_bank[i]];
            else if (scale_bank[i] == kNumScaleBanks - 1)
                c_hiq[i] = user_scalebank;
        }
        else if (filter_mode != TWOPASS && filter_type == BPRE)
        {
            // No branch for bank 19 (user scales): c_hiq/c_loq keep their old values (M1-o).
            if (scale_bank[i] < kNumStockBanks)
            {
                c_hiq[i] = tb.bpre_hi[scale_bank[i]];
                c_loq[i] = tb.bpre_lo[scale_bank[i]];
            }
        }
    }

    // filter.c process_audio_block
    void Engine::process_audio_block (const int32_t* inLeft, const int32_t* inRight)
    {
        const Tables& tb = tables();

        //~78us static, ~104us morphing, every 166us = 6kHz ( = 96k / 16 samples)
        int16_t i, j;
        uint32_t t;
        float f_blended;
        float tmp, fir, iir;
        float c0, c0_a, c1, c2, c2_a;
        float a0, a1, a2;
        float var_q, inv_var_q, var_f, inv_var_f;
        float pos_in_cf;         // % of Qknob position within crossfade region
        float ratio_a, ratio_b;  // two-pass filter crossfade ratios
        float level_lpf;
        uint8_t filter_num, channel_num;
        uint8_t scale_num;
        uint8_t nudge_filter_num;
        const int32_t* ptmp_i32;

        float filter_out_a[kNumFilts][kBlockFrames] = {};   // first filter out for two-pass
        float filter_out_b[kNumFilts][kBlockFrames] = {};   // second filter out for two-pass


        if (filter_mode != TWOPASS)    // not mandatory but save CPU in most cases
        {
            if (filter_type == BPRE && (
                    scale_bank[0] >= kNumScaleBanks || scale_bank[1] >= kNumScaleBanks
                    || scale_bank[2] >= kNumScaleBanks || scale_bank[3] >= kNumScaleBanks
                    || scale_bank[4] >= kNumScaleBanks || scale_bank[5] >= kNumScaleBanks))
                change_filter_type (MAXQ);
        }

        // audio_convert_2x16_to_stereo24() is not ported: the core takes unpacked int32 samples (M2-f).
        // Copying here (not reading in place) is what makes the output buffers allowed to alias the input.
        for (i = 0; i < kBlockFrames; i++)
        {
            left_buffer[i]  = inLeft[i];
            right_buffer[i] = inRight[i];
        }

        if (filter_type_changed) filter_type = new_filter_type;


        //Determine the coef tables we're using for the active filters (Lo-Q and Hi-Q) for each channel
        for (i = 0; i < kNumChannels; i++)
        {
            //Range check scale_bank and scale (the "<0" tests in the C are always false for uint8_t)
            if (scale_bank[i] >= kNumScaleBanks && scale_bank[i] != 0xFF) scale_bank[i] = kNumScaleBanks - 1;
            if (scale[i] >= kNumScales) scale[i] = kNumScales - 1;

            if (scale_bank[i] != pab.old_scale_bank[i] || filter_type_changed)
            {
                pab.old_scale_bank[i] = scale_bank[i];
                select_tables_for_channel (i);
            }   // new scale bank or filter type changed
        }       // channels


        //############################### 2-PASS  ###################################

        if (filter_mode == TWOPASS)
        {
            // UPDATE QVAL
            param_read_q();

            // CALCULATE FILTER OUTPUTS
            //filter_out[0-5] are the note[]/scale[]/scale_bank[] filters.
            //filter_out[6-11] are the morph destination values
            //filter_out[channel1-6][buffer_sample]

            for (channel_num = 0; channel_num < kNumChannels; channel_num++)
            {
                filter_num = note[channel_num];
                scale_num = scale[channel_num];

                pab.qc[channel_num] = (float) qval[channel_num];

                // QVAL ADJUSTMENTS

                // first filter max Q at noon on Q knob
                pab.qval_a[channel_num] = pab.qc[channel_num] * 2.0f;
                if (pab.qval_a[channel_num] > 4095.0f) { pab.qval_a[channel_num] = 4095.0f; }

                // limit q knob range on second filter
                if (pab.qc[channel_num] < 3900.0f) { pab.qval_b[channel_num] = 1000.0f; }
                else if (pab.qc[channel_num] >= 3900.0f) { pab.qval_b[channel_num] = 1000.0f + (pab.qc[channel_num] - 3900.0f) * 15.0f; }   //1000 to 3925

                // Q/RESONANCE: c0 = 1 - 2/(decay * samplerate), where decay is around 0.01 to 4.0
                c0_a = 1.0f - tb.exp_4096[clampIdx (f2u32 (pab.qval_a[channel_num] / 1.4f) + 200u, 4095u)] / 10.0f;   //exp[200...3125]
                c0   = 1.0f - tb.exp_4096[clampIdx (f2u32 (pab.qval_b[channel_num] / 1.4f) + 200u, 4095u)] / 10.0f;   //exp[200...3125]

                // FREQ: c1 = 2 * pi * freq / samplerate
                c1 = *(c_hiq[channel_num] + (scale_num * 21) + filter_num);
                c1 *= freq_nudge[channel_num] * freq_shift[channel_num];
                if (c1 > 1.30899581f) c1 = 1.30899581f;   //hard limit at 20k

                // CROSSFADE between the two filters
                if (pab.qc[channel_num] < (float) kCfMin) { ratio_a = 1.0f; }
                else if (pab.qc[channel_num] > (float) kCfMax) { ratio_a = 0.0f; }
                else
                {
                    pos_in_cf = (pab.qc[channel_num] - (float) kCfMin) / (float) kCrossfadeWidth;
                    ratio_a = 1.0f - pos_in_cf;
                }
                ratio_b = (1.0f - ratio_a);
                ratio_b *= 43801543.68f / twopassCal (tb, f2u32 (pab.qval_b[channel_num] - 900.0f));

                // AMPLITUDE: Boost high freqs and boost low resonance
                c2_a = (0.003f * c1) - (0.1f * c0_a) + 0.102f;
                c2   = (0.003f * c1) - (0.1f * c0)   + 0.102f;
                c2 *= ratio_b;

                if (channel_num & 1) ptmp_i32 = right_buffer;
                else ptmp_i32 = left_buffer;

                j = channel_num;
                {
                    const int b0 = bufIdx (channel_num, scale_num, filter_num, 0);
                    for (i = 0; i < kBlockFrames; i++)
                    {
                        // FIRST PASS (_a)
                        buf_a[b0 + 2] = (c0_a * buf_a[b0 + 1] + c1 * buf_a[b0 + 0]) - c2_a * (float) (*ptmp_i32++);
                        buf_a[b0 + 0] = buf_a[b0 + 0] - (c1 * buf_a[b0 + 2]);

                        buf_a[b0 + 1] = buf_a[b0 + 2];
                        filter_out_a[j][i] = buf_a[b0 + 1];

                        // SECOND PASS (_b)
                        buf[b0 + 2] = (c0 * buf[b0 + 1] + c1 * buf[b0 + 0]) - c2 * (filter_out_a[j][i]);
                        buf[b0 + 0] = buf[b0 + 0] - (c1 * buf[b0 + 2]);

                        buf[b0 + 1] = buf[b0 + 2];
                        filter_out_b[j][i] = buf[b0 + 1];

                        filter_out[j][i] = (ratio_a * filter_out_a[j][i]) - filter_out_b[j][i];   // output of filter two needs to be inverted to avoid phase cancellation
                    }
                }

                // VOCT output
                if (env_track_mode == ENV_VOLTOCT) ENVOUT_preload[channel_num] = c1;


                // Calculate the morph destination filter:
                // Calcuate c1 and c2, which must be updated since the freq changed, and then calculate an entire filter for each channel that's morphing
                if (motion_morphpos[channel_num] > 0.0f)
                {
                    filter_num = (uint8_t) motion_fadeto_note[channel_num];
                    scale_num = (uint8_t) motion_fadeto_scale[channel_num];

                    //FREQ: c1 = 2 * pi * freq / samplerate
                    c1 = *(c_hiq[channel_num] + (scale_num * 21) + filter_num);
                    c1 *= freq_nudge[channel_num];
                    c1 *= freq_shift[channel_num];
                    if (c1 > 1.30899581f) c1 = 1.30899581f;   //hard limit at 20k

                    //AMPLITUDE: Boost high freqs and boost low resonance
                    c2_a = (0.003f * c1) - (0.1f * c0_a) + 0.102f;
                    c2   = (0.003f * c1) - (0.1f * c0) + 0.102f;
                    c2 *= ratio_b;

                    //Point to the left or right input buffer
                    if (channel_num & 1) ptmp_i32 = right_buffer;
                    else ptmp_i32 = left_buffer;

                    j = channel_num + 6;
                    {
                        const int b0 = bufIdx (channel_num, scale_num, filter_num, 0);
                        for (i = 0; i < kBlockFrames; i++)
                        {
                            // FIRST PASS (_a)
                            buf_a[b0 + 2] = (c0_a * buf_a[b0 + 1] + c1 * buf_a[b0 + 0]) - c2_a * (float) (*ptmp_i32++);
                            buf_a[b0 + 0] = buf_a[b0 + 0] - (c1 * buf_a[b0 + 2]);

                            buf_a[b0 + 1] = buf_a[b0 + 2];
                            filter_out_a[j][i] = buf_a[b0 + 1];

                            // SECOND PASS (_b)
                            buf[b0 + 2] = (c0 * buf[b0 + 1] + c1 * buf[b0 + 0]) - c2 * (filter_out_a[j][i]);
                            buf[b0 + 0] = buf[b0 + 0] - (c1 * buf[b0 + 2]);

                            buf[b0 + 1] = buf[b0 + 2];
                            filter_out_b[j][i] = buf[b0 + 1];

                            filter_out[j][i] = (ratio_a * filter_out_a[j][i]) - filter_out_b[j][i];   // output of filter two needs to be inverted to avoid phase cancellation
                        }
                    }

                    // VOCT output with glissando
                    if (env_track_mode == ENV_VOLTOCT && env_prepost_mode == PRE)
                        ENVOUT_preload[channel_num] = (ENVOUT_preload[channel_num] * (1.0f - motion_morphpos[channel_num])) + (c1 * motion_morphpos[channel_num]);
                }
            }
        }


        //###################### 1-PASS ###################################

        else if (filter_mode != TWOPASS)
        {
            // UPDATE QVAL
            param_read_q();

            //Calculate filter_out[]
            //filter_out[0-5] are the note[]/scale[]/scale_bank[] filters. filter_out[6-11] are the morph destination values
            if (filter_type == MAXQ)
            {
                for (j = 0; j < kNumChannels * 2; j++)
                {
                    if (j < 6)
                        channel_num = (uint8_t) j;
                    else
                        channel_num = (uint8_t) (j - 6);

                    if (j < 6 || motion_morphpos[channel_num] != 0)
                    {
                        // Set filter_num and scale_num to the Morph sources
                        if (j < 6)
                        {
                            filter_num = note[channel_num];
                            scale_num = scale[channel_num];

                        // Set filter_num and scale_num to the Morph dests
                        }
                        else
                        {
                            filter_num = (uint8_t) motion_fadeto_note[channel_num];
                            scale_num = (uint8_t) motion_fadeto_scale[channel_num];
                        }

                        nudge_filter_num = (uint8_t) (filter_num + 1);   // (unused in this branch, as in the C)
                        if (nudge_filter_num > kNumFilts) nudge_filter_num = kNumFilts;
                        (void) nudge_filter_num;

                        //Q/RESONANCE: c0 = 1 - 2/(decay * samplerate), where decay is around 0.01 to 4.0
                        c0 = 1.0f - tb.exp_4096[clampIdx (f2u32 ((float) qval[channel_num] / 1.4f) + 200u, 4095u)] / 10.0f;   //exp[200...3125]

                        // FREQ: c1 = 2 * pi * freq / samplerate
                        c1 = *(c_hiq[channel_num] + (scale_num * 21) + filter_num);
                        c1 *= freq_nudge[channel_num];
                        c1 *= freq_shift[channel_num];
                        if (c1 > 1.30899581f) c1 = 1.30899581f;   //hard limit at 20k


                        //AMPLITUDE: Boost high freqs and boost low resonance
                        c2 = (0.003f * c1) - (0.1f * c0) + 0.102f;
                        c2 *= ((4096.0f - (float) qval[channel_num]) / 1024.0f) + 1.04f;


                        {
                            const int b0 = bufIdx (channel_num, scale_num, filter_num, 0);
                            for (i = 0; i < kBlockFrames; i++)   // MONO_BUFSZ/(96000/SAMPLERATE) with SAMPLERATE 96000
                            {
                                if (channel_num & 1) tmp = (float) right_buffer[i];
                                else tmp = (float) left_buffer[i];

                                buf[b0 + 2] = (c0 * buf[b0 + 1] + c1 * buf[b0 + 0]) - c2 * tmp;
                                iir = buf[b0 + 0] - (c1 * buf[b0 + 2]);
                                buf[b0 + 0] = iir;

                                buf[b0 + 1] = buf[b0 + 2];
                                filter_out[j][i] = buf[b0 + 1];
                            }
                        }
                    }
                }


            }
            else     //filter_type==BPRE
            {
                for (j = 0; j < kNumChannels * 2; j++)
                {
                    if (j < 6)
                        channel_num = (uint8_t) j;
                    else
                        channel_num = (uint8_t) (j - 6);

                    if (j < 6 || motion_morphpos[channel_num] != 0.0f)
                    {
                        // Set filter_num and scale_num to the Morph sources
                        if (j < 6)
                        {
                            filter_num = note[channel_num];
                            scale_num = scale[channel_num];

                        // Set filter_num and scale_num to the Morph dests
                        }
                        else
                        {
                            filter_num = (uint8_t) motion_fadeto_note[channel_num];
                            scale_num = (uint8_t) motion_fadeto_scale[channel_num];
                        }

                        //Q vector
                        var_f = freq_nudge[channel_num];
                        if (var_f < 0.002f) var_f = 0.0f;
                        if (var_f > 0.998f) var_f = 1.0f;
                        inv_var_f = 1.0f - var_f;

                        //Freq nudge vector
                        nudge_filter_num = (uint8_t) (filter_num + 1);
                        if (nudge_filter_num > kNumFilts) nudge_filter_num = kNumFilts;


                        a0 = *(c_loq[channel_num] + (scale_num * 63) + (nudge_filter_num * 3) + 0) * var_f + *(c_loq[channel_num] + (scale_num * 63) + (filter_num * 3) + 0) * inv_var_f;
                        a1 = *(c_loq[channel_num] + (scale_num * 63) + (nudge_filter_num * 3) + 1) * var_f + *(c_loq[channel_num] + (scale_num * 63) + (filter_num * 3) + 1) * inv_var_f;
                        a2 = *(c_loq[channel_num] + (scale_num * 63) + (nudge_filter_num * 3) + 2) * var_f + *(c_loq[channel_num] + (scale_num * 63) + (filter_num * 3) + 2) * inv_var_f;

                        c0 = *(c_hiq[channel_num] + (scale_num * 63) + (nudge_filter_num * 3) + 0) * var_f + *(c_hiq[channel_num] + (scale_num * 63) + (filter_num * 3) + 0) * inv_var_f;
                        c1 = *(c_hiq[channel_num] + (scale_num * 63) + (nudge_filter_num * 3) + 1) * var_f + *(c_hiq[channel_num] + (scale_num * 63) + (filter_num * 3) + 1) * inv_var_f;
                        c2 = *(c_hiq[channel_num] + (scale_num * 63) + (nudge_filter_num * 3) + 2) * var_f + *(c_hiq[channel_num] + (scale_num * 63) + (filter_num * 3) + 2) * inv_var_f;

                        //Q vector
                        if (qval[channel_num] > 4065)
                        {
                            var_q = 1.0f;
                            inv_var_q = 0.0f;
                        }
                        else
                        {
                            var_q = tb.log_4096[qval[channel_num]];
                            inv_var_q = 1.0f - var_q;
                        }

                        c0 = c0 * var_q + a0 * inv_var_q;
                        c1 = c1 * var_q + a1 * inv_var_q;
                        c2 = c2 * var_q + a2 * inv_var_q;


                        {
                            const int b0 = bufIdx (channel_num, scale_num, filter_num, 0);
                            for (i = 0; i < kBlockFrames; i++)   // MONO_BUFSZ/(96000/SAMPLERATE) with SAMPLERATE 96000
                            {
                                tmp = buf[b0 + 0];
                                buf[b0 + 0] = buf[b0 + 1];

                                //Odd input (left) goes to odd filters (1/3/5)
                                //Even input (right) goes to even filters (2/4/6)
                                if (filter_num & 1) iir = (float) left_buffer[i] * c0;
                                else iir = (float) right_buffer[i] * c0;

                                iir -= c1 * tmp;
                                fir = -tmp;
                                iir -= c2 * buf[b0 + 0];
                                fir += iir;
                                buf[b0 + 1] = iir;

                                filter_out[j][i] = fir;
                            }
                        }
                    }
                }
            }
        }   // Filter-mode


        // ##########################################################


        // MORPHING
        for (i = 0; i < kBlockFrames; i++)
        {
            filtered_buffer[i] = 0;
            filtered_bufferR[i] = 0;

            update_morph();

            for (j = 0; j < kNumChannels; j++)
            {
                if (motion_morphpos[j] == 0.0f)
                    f_blended = filter_out[j][i];
                else
                    f_blended = (filter_out[j][i] * (1.0f - motion_morphpos[j])) + (filter_out[j + kNumChannels][i] * motion_morphpos[j]);   // filter blending

                // The C has an ui_mode==EDIT_SCALES branch here; ui_mode is fixed at PLAY in this port (Milestone 7).

                pab.poll_ctr[j] += 1;
                if (pab.poll_ctr[j] > 50)   // UPDATE RATE
                {
                    pab.poll_ctr[j] = 0;

                    t = potadc_buffer[j + SLIDER_ADC_BASE];
                    if (t < 20) t = 0;
                    else t -= 20;

                    level_lpf = ((float) (adc_buffer[j + LEVEL_ADC_BASE]) / 4096.0f) * (float) (t) / 4096.0f;
                    if (level_lpf <= 0.007f) level_lpf = 0.0f;

                    pab.prev_level[j] = pab.level_goal[j];

                    pab.level_goal[j] *= CHANNEL_LEVEL_LPF;
                    pab.level_goal[j] += (1.0f - CHANNEL_LEVEL_LPF) * level_lpf;

                    pab.level_inc[j] = (pab.level_goal[j] - pab.prev_level[j]) / 50.0f;
                    channel_level[j] = pab.prev_level[j];
                }
                else    // SMOOTH OUT DATA BETWEEN ADC READS
                    channel_level[j] += pab.level_inc[j];

                // APPLY LEVEL TO AUDIO OUT
                // The C does   filtered_buffer[i] += (f_blended * channel_level[j]);
                // i.e. int32 converted to float, float add, truncate back. The truncation is undefined in C when
                // out of range; the port saturates and counts it (docs/port-notes.md item P-3).
                {
                    int32_t& acc = (j & 1) ? filtered_buffer[i] : filtered_bufferR[i];
                    const float sum = (float) acc + (f_blended * channel_level[j]);
                    if (! (sum > -2147483648.0f && sum < 2147483648.0f)) ++saturation_count;
                    acc = f2i32 (sum);
                }
            }
        }

        for (j = 0; j < kNumChannels; j++)
        {
            f_blended = (filter_out[j][0] * (1.0f - motion_morphpos[j])) + (filter_out[j + kNumChannels][0] * motion_morphpos[j]);

            // LED_SLIDER_ON(slider_led[j]) clipping indicator: omitted (LED only).

            if (env_track_mode != ENV_VOLTOCT)
            {
                if (f_blended > 0)
                    ENVOUT_preload[j] = f_blended;
                else
                    ENVOUT_preload[j] = -1.0f * f_blended;
            }
        }

        // audio_convert_stereo24_to_2x16() is not ported (M2-f): processAudioOnly() copies the int32 buffers out.

        filter_type_changed = 0;
    }
}
