// Host-side wrapper around the core: any host sample rate (from the exact-ratio list), any host buffer size,
// float stereo in and out. Plain C++, no JUCE. Real-time safe after prepare(): no allocation, no locks.
//
// Signal path: host float -> resampler to the core rate -> 16-frame blocks of int32 (24 bit left-justified,
// docs/architecture.md section 12.3) -> smr::Engine::processBlock -> float -> resampler to the host rate.
// Host rate == core rate: no resampler, only the block adapter (bit-exact path used for the hardware comparisons).
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
    };
}
