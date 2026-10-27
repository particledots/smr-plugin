// Host-side wrapper around the core: any host sample rate (from the exact-ratio list), any host buffer size,
// float stereo in and out. Plain C++, no JUCE. Real-time safe after prepare(): no allocation, no locks.
//
// Signal path: host float -> resampler to the core rate -> 16-frame blocks of int32 (24 bit left-justified,
// docs/architecture.md section 12.3) -> smr::Engine::processBlock -> float -> resampler to the host rate.
// Host rate == core rate: no resampler, only the block adapter. Since the core rate became 671875/7 Hz (open question M2-b) no
// integer host rate is equal to it, so this path exists only through prepareOneToOne() (smr_render --core-rate and the tests).
//
// The float scale is a placeholder until M1-n is decided: 1.0 maps to the largest int32 below 2^31.
#pragma once

#include <memory>
#include <vector>

#include "Engine.h"
#include "Resampler.h"

namespace smr_host
{
    class BlockAdapter
    {
    public:
        // Returns false (and leaves the adapter unprepared) if hostRate needs the arbitrary-ratio path.
        bool prepare (int hostRate, int maxHostBlock);
        // The host stream is taken as already being at the core rate: no resampling, the 16-frame block adapter only. A 96000 Hz file
        // is then read as if its rate were the core rate (every frequency in the output is core/96000 times the table's).
        // Used by smr_render --core-rate and by the tests; the plugin uses prepare().
        bool prepareOneToOne (int rate, int maxHostBlock);
        void reset();   // also calls Engine::reset(): keeps the controls last set

        smr::Engine& engine() { return *engine_; }

        // Processes n host frames. inR may be null (mono: the left input is copied to both input slots).
        // outputs: left = first output slot (filtered_buffer), right = second (filtered_bufferR), the source mapping (M1-m, provisional).
        // Aliasing: out may equal in.
        void process (const float* inL, const float* inR, float* outL, float* outR, int n);

        // Total delay of silence at the start, in host frames: resampler group delays plus the one-block pre-roll.
        double latencyHostFrames() const;

        int hostRate() const { return hostRate_; }
        uint64_t underruns() const { return underruns_; }
        uint64_t blocksProcessed() const { return blocks_; }

    private:
        bool prepareWith (int hostRate, int maxHostBlock, int up, int down, double coreRate);

        struct Chan
        {
            Resampler toCore, toHost;
            std::vector<float> coreIn;     // core-rate input queue
            std::vector<float> coreOut;    // core-rate output queue
            std::vector<float> hostOut;    // host-rate output queue
            int nCoreIn = 0, nCoreOut = 0, nHostOut = 0;
        };

        std::unique_ptr<smr::Engine> engine_;
        Chan ch_[2];
        int hostRate_ = 0;
        int maxHostBlock_ = 0;
        uint64_t underruns_ = 0, blocks_ = 0;
        bool prepared_ = false;
        int up_ = 1, down_ = 1;
        double coreRate_ = 0.0;   // the rate the resamplers convert to; equal to hostRate_ in the one-to-one path
    };
}
