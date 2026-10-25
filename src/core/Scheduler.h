// Tick scheduler (docs/architecture.md section 11.3). All time in the core is counted in firmware
// time (counts of the 168 MHz timer clock), never in host time. Exact integer arithmetic.
//
// Core rate is the rational Fs = num/den. One block is 16 frames, which lasts
//   16 * 168,000,000 / Fs = 2,688,000,000 * den / num   counts.
// Each source with period P counts keeps an accumulator in units of 1/num counts:
//   acc += 2,688,000,000 * den;  while (acc >= P * num) { acc -= P * num; one tick; }
// All values fit in 64-bit integers.
#pragma once

#include <cstdint>

#include "Constants.h"

namespace smr
{
    struct DueTicks
    {
        uint32_t tim9 = 0;    // update_ENVOUT_PWM()
        uint32_t tim10 = 0;   // param_read_freq(), update_slider_LEDs() (no-op), update_motion()
        uint32_t loop = 0;    // one pass of the main() while(1) body
    };

    class Scheduler
    {
    public:
        Scheduler (uint64_t rateNum = kCoreRateNum, uint64_t rateDen = kCoreRateDen,
                   uint64_t tim9Counts = kTim9Counts, uint64_t tim10Counts = kTim10Counts,
                   uint64_t loopCounts = kLoopCounts) noexcept;

        void reset() noexcept;

        // Advance by one 16-frame block and report how many ticks of each kind fell due.
        DueTicks advanceOneBlock() noexcept;

    private:
        uint64_t num_, den_;
        uint64_t p9_, p10_, pLoop_;   // thresholds, in units of 1/num counts
        uint64_t perBlock_;           // 16 * kTimerClockHz * den
        uint64_t acc9_ = 0, acc10_ = 0, accLoop_ = 0;
    };
}
