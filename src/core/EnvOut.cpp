// Port of third_party/SMR/envout_pwm.c: update_ENVOUT_PWM() and FreqCoef_to_PWMval().
// The hardware timer register writes (TIM3->CCRx, TIM1->CCRx) are replaced by the ENVOUT_PWM[] array itself,
// which the host reads. Every floating-point literal carries an f suffix (-fsingle-precision-constant).
#include "Engine.h"

namespace smr
{
    namespace
    {
        // envout_pwm.c FreqCoef_to_PWMval. The comparison cascade is generated mechanically (tools/gen_freqcoef.py).
        uint32_t FreqCoef_to_PWMval (uint32_t k, float v)
        {
            float t, b, tval, bval;
            uint32_t result;

#include "FreqCoef.inc"

            result = f2u32 (((t - v) / (t - b)) * bval + ((v - b) / (t - b)) * tval);

            // asm("usat %[dst], #12, %[src]"): clamp to 0..4095. The input here is never negative or above 4096.
            if (result > 4095) result = 4095;

            return result;
        }
    }

    // envout_pwm.c update_ENVOUT_PWM
    void Engine::update_ENVOUT_PWM()
    {
        uint8_t j;
        uint32_t k;

        auto& s = env;

        if (env_track_mode == ENV_VOLTOCT)   //takes 5us, every 20us
        {
            for (j = 0; j < kNumChannels; j++)
            {
                k = f2u32 (ENVOUT_preload[j] * 8192.0f);   //multiply the preload float value by 8192 so we can do a series of integer comparisons in FreqCoef_to_PWMval()

                ENVOUT_PWM[j] = f2u32 (voltoct_pwm_tracking * (float) (FreqCoef_to_PWMval (k, ENVOUT_preload[j])));

                if ((float) ENVOUT_PWM[j] < 127.96875f)
                    ENVOUT_PWM[j] = 0;
                else
                    ENVOUT_PWM[j] = f2u32 ((float) ENVOUT_PWM[j] - 127.96875f);   // Adjusts PWM out so C2 = 1V, instead of A1
            }
        }
        else if (env_track_mode == ENV_SLOW || env_track_mode == ENV_FAST)
        {
            for (j = 0; j < kNumChannels; j++)
            {
                //Apply LPF
                if (s.envelope[j] < ENVOUT_preload[j])
                {
                    s.envelope[j] *= envspeed_attack;
                    s.envelope[j] += (1.0f - envspeed_attack) * ENVOUT_preload[j];
                }
                else
                {
                    s.envelope[j] *= envspeed_decay;
                    s.envelope[j] += (1.0f - envspeed_decay) * ENVOUT_preload[j];
                }

                //Pre-CV (input) or Post faders (quieter)
                if (env_prepost_mode == PRE)
                    ENVOUT_PWM[j] = f2u32 (s.envelope[j]) >> 18;
                else
                    ENVOUT_PWM[j] = f2u32 (s.envelope[j] * channel_level[j]) >> 16;
            }
        }
        else
        {   //trigger mode
            for (j = 0; j < kNumChannels; j++)
            {
                //Pre-CV (input) or Post-CV (output)
                if (env_prepost_mode == PRE)
                {
                    if (stored_trigger_level[j] < 0.002f)
                        ENVOUT_preload[j] *= 0.5f;
                    else
                        ENVOUT_preload[j] *= stored_trigger_level[j];
                }
                else
                {
                    ENVOUT_preload[j] *= channel_level[j];
                    stored_trigger_level[j] = channel_level[j];
                }


                if (s.env_trigout[j])   //keep the trigger high for about 100ms, ignoring the input signal
                {
                    s.env_trigout[j]--;
                }
                else
                {
                    if (f2u32 (ENVOUT_preload[j]) > 0x02000000u)   //about 12.5% of max, or 1V envelope output
                    {
                        s.env_low_ctr[j] = 0;
                        s.env_trigout[j] = 285;   //about 100ms
                        ENVOUT_PWM[j] = 4090;
                    }
                    else
                    {
                        if (++s.env_low_ctr[j] >= 285)   //only set the output low if the input signal has stayed low for about 100ms
                            ENVOUT_PWM[j] = 0;
                    }
                }
            }
        }

        // TIM3->CCR / TIM1->CCR writes: not ported. The host reads ENVOUT_PWM[0..5] (docs/port-notes.md).
    }
}
