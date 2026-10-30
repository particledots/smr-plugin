// Derived from the Spectral Multiband Resonator firmware (third_party/SMR, v5.1.1, commit 79bc953).
// Original authors: Dan Green, Hugo Paris. Copyright (c) 2015 Dan Green. MIT licence: see THIRD-PARTY-LICENSES.txt and licenses/SMR-LICENSE.txt.
// src/core: DSP only. No JUCE, no plugin-host code, no hardware code.
// Constants copied from the firmware (third_party/SMR at 79bc953). Each line names its source.
#pragma once

#include <cstdint>

namespace smr
{
    // globals.h
    constexpr int kNumFilts      = 20;   // NUM_FILTS
    constexpr int kNumChannels   = 6;    // NUM_CHANNELS
    constexpr int kNumScales     = 11;   // NUMSCALES
    constexpr int kNumScaleBanks = 20;   // NUMSCALEBANKS (bank 19 is the user bank)

    // filter.c: MONO_BUFSZ = ((codec_BUFF_LEN/2)/2)/2 = 16 frames per block
    constexpr int kBlockFrames   = 16;

    // dig_inouts.h
    constexpr int kNumAdcs       = 15;   // NUM_ADCS
    constexpr int kNumAdc3s      = 7;    // NUM_ADC3S
    constexpr int MORPH_ADC      = 0;
    constexpr int QVAL_ADC       = 1;
    constexpr int LEVEL_ADC_BASE = 2;    // 2..7 are the Level CV jacks
    constexpr int FREQNUDGE1_ADC = 8;
    constexpr int FREQNUDGE6_ADC = 9;
    constexpr int SCALE_ADC      = 10;
    constexpr int SPREAD_ADC     = 11;
    constexpr int ROTCV_ADC      = 12;
    constexpr int FREQCV1_ADC    = 13;
    constexpr int FREQCV6_ADC    = 14;
    constexpr int QPOT_ADC       = 0;    // potadc_buffer
    constexpr int SLIDER_ADC_BASE = 1;   // 1..6 are the sliders

    // Table geometry (see docs/architecture.md section 13.2)
    constexpr int kScaleStride   = 21;   // 20 notes plus one pad, per scale
    constexpr int kMaxqTableSize = 231;  // 21 x 11
    constexpr int kBpreTableSize = 693;  // 21 x 3 x 11
    constexpr int kBpreScaleStride = 63; // 21 x 3
    constexpr int kNumStockBanks = 19;   // banks 0..18 have tables; bank 19 is user_scalebank
    constexpr int kExpTableSize  = 4096;
    constexpr int kTwopassCalSize = 3380;

    // Core sample rate, held in one place. Bryn decided on 2026-10-26 to use the hardware's I2S rate
    // 671875/7 = 95,982.142857 Hz (docs/open-questions.md M2-b: computed from PLLI2S_N 344 and PLLI2S_R 2 in
    // system_stm32f4xx.c and I2S_Init in stm32f4xx_spi.c; matched by capture-1 within 0.1 cent on the evens jack, CI run 40457983850).
    // The coefficient tables and decay constants stay those of the firmware (written for 96000 Hz): only the clock changes.
    // Before this decision the value was the nominal 96000 / 1.
    // The rate is the rational kCoreRateNum / kCoreRateDen so it has no floating-point drift in the tick scheduler.
    constexpr uint64_t kCoreRateNum = 671875;
    constexpr uint64_t kCoreRateDen = 7;

    // Timer clock for TIM9 and TIM10 (docs/architecture.md section 11.2, derived).
    constexpr uint64_t kTimerClockHz = 168000000ULL;

    // Timer periods: counts per update = register value + 1 (params.c "168MHz / (Period+1)").
    constexpr uint64_t kTim10Counts = 8195 + 1;    // params.c init_freq_update_timer
    constexpr uint64_t kTim9Counts  = 30000 + 1;   // envout_pwm.c init_ENV_update_timer
    // Main loop: placeholder 12,000 iterations per second (decision M2-c, docs/open-questions.md).
    // params.h DISPLAYTIME_*: counted in param_read_freq calls, which run on TIM10 (params.c TIM1_UP_TIM10_IRQHandler), so
    // in units of kTim10Counts / 168 MHz (about 20.5 kHz): 12000 is about 0.585 s, 25000 about 1.22 s. Not main-loop passes.
    constexpr uint32_t kDisplayTimeNudgeBothSides = 25000;
    constexpr uint32_t kDisplayTimeNudgeOneSide   = 12000;

    constexpr uint64_t kLoopRateHz  = 12000;
    constexpr uint64_t kLoopCounts  = kTimerClockHz / kLoopRateHz;   // 14000 counts at 168 MHz

    // filter.h CROSSFADE_POINT is written "4095 * 2 / 3" with no brackets: integer 2730.
    constexpr int kCrossfadePoint = 4095 * 2 / 3;
    constexpr int kCrossfadeWidth = 1800;
    constexpr int kCfMin = kCrossfadePoint - kCrossfadeWidth / 2;   // 1830
    constexpr int kCfMax = kCrossfadePoint + kCrossfadeWidth / 2;   // 3630

    // globals.h
    enum FilterMode : int { TWOPASS = 2, ONEPASS = 3 };
    enum FilterType : int { BPRE = 0, MAXQ = 1 };
    constexpr uint32_t PRE  = 0;
    constexpr uint32_t POST = 1;

    // params.h LOCK_BUTTON_QUNLOCK_HOLD_TIME: main-loop passes (not seconds) a Lock button must be held, with the channel locked and
    // Q-locked and the Q pot not turned, before the frequency lock is cleared and the Q-lock kept (params.c process_lock_buttons).
    // The manual says 2 seconds; the loop has no timer, so the real time depends on the main-loop rate (M2-c placeholder): docs/open-questions.md M7-a.
    constexpr uint32_t kLockButtonQunlockHoldTime = 50000;

    // params.h
    enum EnvOutMode : int { ENV_SLOW = 0, ENV_FAST = 1, ENV_TRIG = 2, ENV_VOLTOCT = 3 };
}
