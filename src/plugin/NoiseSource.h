// src/plugin: the Noise switch's generator (Milestone 7, step 6, docs/open-questions.md M7-e). Plain C++, no JUCE.
//
// STAND-IN, NOT FIRMWARE. The hardware noise is the STM32 DAC channel 1 noise generator (dac.c DAC_Ch1_NoiseConfig, TIM6_Config),
// normalled to the odds IN jack by analogue circuitry. Its sample rate, spectrum, sequence and level are not known (M7-e, M1-n).
// This generator is white noise, uniform in -kNoisePeak..+kNoisePeak, from a fixed-seed xorshift32, so a render is repeatable.
// kNoiseRms is the project's own test level (-20 dBFS rms, the level of the capture sheet's noise sections, tools/analysis/selftest.py),
// not a measurement of the hardware. It stays far under the input clip level (DisplayModel.h kInputClipFloat).
#pragma once

#include <cmath>
#include <cstdint>

namespace smr_plugin
{
    constexpr float kNoiseRms  = 0.1f;                   // -20 dBFS rms
    constexpr float kNoisePeak = 0.17320508f;            // uniform noise has rms = peak / sqrt (3)

    class NoiseSource
    {
    public:
        void reset() noexcept { state_ = kSeed; }

        // Fills n samples with the next part of the sequence. No allocation, no locks.
        void fill (float* out, int n) noexcept
        {
            for (int i = 0; i < n; ++i)
                out[i] = next();
        }

        float next() noexcept
        {
            uint32_t x = state_;
            x ^= x << 13;
            x ^= x >> 17;
            x ^= x << 5;
            state_ = x;
            // 24 high-quality bits to -1..+1 (never exactly 1), then to -kNoisePeak..+kNoisePeak
            const float u = (float) (x >> 8) * (2.0f / 16777216.0f) - 1.0f;
            return u * kNoisePeak;
        }

    private:
        static constexpr uint32_t kSeed = 0x2545F491u;   // any non-zero value
        uint32_t state_ = kSeed;
    };
}
