#include "Scheduler.h"

namespace smr
{
    Scheduler::Scheduler (uint64_t rateNum, uint64_t rateDen, uint64_t tim9Counts, uint64_t tim10Counts,
                          uint64_t loopCounts) noexcept
        : num_ (rateNum),
          den_ (rateDen),
          p9_ (tim9Counts * rateNum),
          p10_ (tim10Counts * rateNum),
          pLoop_ (loopCounts * rateNum),
          perBlock_ ((uint64_t) kBlockFrames * kTimerClockHz * rateDen)
    {
    }

    void Scheduler::reset() noexcept
    {
        acc9_ = acc10_ = accLoop_ = 0;
    }

    DueTicks Scheduler::advanceOneBlock() noexcept
    {
        DueTicks d;

        acc9_ += perBlock_;
        while (acc9_ >= p9_)    { acc9_ -= p9_;       ++d.tim9; }

        acc10_ += perBlock_;
        while (acc10_ >= p10_)  { acc10_ -= p10_;     ++d.tim10; }

        accLoop_ += perBlock_;
        while (accLoop_ >= pLoop_) { accLoop_ -= pLoop_; ++d.loop; }

        return d;
    }
}
