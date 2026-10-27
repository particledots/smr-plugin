#include "BlockAdapter.h"

#include <cmath>
#include <cstring>

namespace smr_host
{
    namespace
    {
        constexpr float kFullScale = 2147483520.0f;   // largest float below 2^31
        constexpr int kGuardHostFrames = 2;

        inline int32_t toCore (float x)
        {
            if (! (x == x)) return 0;
            if (x > 1.0f) x = 1.0f;
            if (x < -1.0f) x = -1.0f;
            return (int32_t) (x * kFullScale);
        }

        inline float fromCore (int32_t v) { return (float) v / kFullScale; }
    }

    bool BlockAdapter::prepare (int hostRate, int maxHostBlock)
    {
        prepared_ = false;
        int up = 1, down = 1;
        if (! ratioToCore (hostRate, up, down) || maxHostBlock < 1) return false;
        return prepareWith (hostRate, maxHostBlock, up, down, (double) smr::kCoreRateNum / (double) smr::kCoreRateDen);
    }

    bool BlockAdapter::prepareOneToOne (int rate, int maxHostBlock)
    {
        prepared_ = false;
        if (rate <= 0 || maxHostBlock < 1) return false;
        return prepareWith (rate, maxHostBlock, 1, 1, (double) rate);
    }

    bool BlockAdapter::prepareWith (int hostRate, int maxHostBlock, int up, int down, double coreRate)
    {
        up_ = up;
        down_ = down;
        hostRate_ = hostRate;
        maxHostBlock_ = maxHostBlock;
        coreRate_ = coreRate;

        if (! engine_) engine_.reset (new smr::Engine());

        // worst-case queue sizes
        const int coreFramesPerHostBlock = (int) (((int64_t) maxHostBlock * up + down - 1) / down) + 2;
        const int cap = coreFramesPerHostBlock + 2 * smr::kBlockFrames + 8;
        const int hostCap = maxHostBlock + (int) (((int64_t) 2 * smr::kBlockFrames * down + up - 1) / up) + 16;

        for (auto& c : ch_)
        {
            c.toCore.prepare (up, down, (double) hostRate, coreRate);
            c.toHost.prepare (down, up, coreRate, (double) hostRate);
            c.coreIn.assign ((size_t) cap, 0.0f);
            c.coreOut.assign ((size_t) cap + 64, 0.0f);
            c.hostOut.assign ((size_t) hostCap + 256, 0.0f);
        }

        prepared_ = true;
        reset();
        return true;
    }

    void BlockAdapter::reset()
    {
        if (! prepared_) return;
        engine_->reset();
        for (auto& c : ch_)
        {
            c.toCore.reset();
            c.toHost.reset();
            c.nCoreIn = 0;
            c.nCoreOut = smr::kBlockFrames;                 // pre-roll: one block of silence (docs/architecture.md 12.2)
            std::fill (c.coreOut.begin(), c.coreOut.end(), 0.0f);
            // Guard frames (not bypass): the resampler's output count can lag its input by one sample, so two zero host
            // frames are queued in addition to the one-block pre-roll. In the one-to-one path there is no guard.
            c.nHostOut = (up_ == 1 && down_ == 1) ? 0 : kGuardHostFrames;
            std::fill (c.hostOut.begin(), c.hostOut.end(), 0.0f);
        }
        underruns_ = 0;
        blocks_ = 0;
    }

    double BlockAdapter::latencyHostFrames() const
    {
        if (! prepared_) return 0.0;
        const double coreRate = coreRate_;
        const double toCoreCoreFrames = ch_[0].toCore.latencyOutputSamples();            // in core frames
        const double toHostHostFrames = ch_[0].toHost.latencyOutputSamples();            // in host frames
        const double preRollHostFrames = (double) smr::kBlockFrames * (double) hostRate_ / coreRate;
        const double guard = (up_ == 1 && down_ == 1) ? 0.0 : (double) kGuardHostFrames;
        return toCoreCoreFrames * (double) hostRate_ / coreRate + preRollHostFrames + toHostHostFrames + guard;
    }

    void BlockAdapter::process (const float* inL, const float* inR, float* outL, float* outR, int n)
    {
        if (! prepared_ || n <= 0) return;
        if (! inR) inR = inL;

        // Process in pieces no longer than maxHostBlock_ so the queues cannot overflow.
        int done = 0;
        while (done < n)
        {
            const int m = (n - done) < maxHostBlock_ ? (n - done) : maxHostBlock_;
            const float* in[2] = { inL + done, inR + done };

            // Copy the inputs first so that out may alias in.
            // 1. host -> core
            for (int c = 0; c < 2; ++c)
            {
                Chan& ch = ch_[c];
                const int produced = ch.toCore.process (in[c], m, &ch.coreIn[(size_t) ch.nCoreIn], (int) ch.coreIn.size() - ch.nCoreIn);
                ch.nCoreIn += produced;
            }

            // 2. core blocks. Both channels always have the same count.
            while (ch_[0].nCoreIn >= smr::kBlockFrames && ch_[1].nCoreIn >= smr::kBlockFrames)
            {
                int32_t l[smr::kBlockFrames], r[smr::kBlockFrames], oa[smr::kBlockFrames], ob[smr::kBlockFrames];
                for (int k = 0; k < smr::kBlockFrames; ++k)
                {
                    l[k] = toCore (ch_[0].coreIn[(size_t) k]);
                    r[k] = toCore (ch_[1].coreIn[(size_t) k]);
                }
                for (auto& ch : ch_)
                {
                    ch.nCoreIn -= smr::kBlockFrames;
                    std::memmove (ch.coreIn.data(), ch.coreIn.data() + smr::kBlockFrames, sizeof (float) * (size_t) ch.nCoreIn);
                }

                engine_->processBlock (l, r, oa, ob);
                ++blocks_;

                for (int k = 0; k < smr::kBlockFrames; ++k)
                {
                    ch_[0].coreOut[(size_t) ch_[0].nCoreOut + (size_t) k] = fromCore (oa[k]);
                    ch_[1].coreOut[(size_t) ch_[1].nCoreOut + (size_t) k] = fromCore (ob[k]);
                }
                ch_[0].nCoreOut += smr::kBlockFrames;
                ch_[1].nCoreOut += smr::kBlockFrames;
            }

            // 3. core -> host
            for (auto& ch : ch_)
            {
                const int produced = ch.toHost.process (ch.coreOut.data(), ch.nCoreOut, &ch.hostOut[(size_t) ch.nHostOut], (int) ch.hostOut.size() - ch.nHostOut);
                // The resampler consumes all nCoreOut inputs unless the output buffer is full (cannot happen with these sizes).
                ch.nCoreOut = 0;
                ch.nHostOut += produced;
            }

            // 4. deliver m host frames (zero-fill and count if the queue is short)
            for (int c = 0; c < 2; ++c)
            {
                Chan& ch = ch_[c];
                float* out = c == 0 ? outL + done : outR + done;
                const int have = ch.nHostOut < m ? ch.nHostOut : m;
                std::memcpy (out, ch.hostOut.data(), sizeof (float) * (size_t) have);
                for (int k = have; k < m; ++k) out[k] = 0.0f;
                if (have < m && c == 0) ++underruns_;
                ch.nHostOut -= have;
                std::memmove (ch.hostOut.data(), ch.hostOut.data() + have, sizeof (float) * (size_t) ch.nHostOut);
            }
            done += m;
        }
    }
}
