// Linear-phase polyphase FIR resampler by a rational ratio up/down (plain C++, no JUCE, no allocation after prepare()).
// Quality targets are my design choices, not firmware facts (docs/architecture.md section 12.2). They are set and
// measured in Milestone 5; in Milestone 2 this is for listening only.
#pragma once

#include <cstdint>
#include <vector>

namespace smr_host
{
    // Reduced ratio for converting hostRate to the core rate (kCoreRateNum / kCoreRateDen), as in docs/architecture.md 12.2.
    // Returns false if hostRate is not an integer whose reduced ratio has up <= maxUp (then the arbitrary-ratio path of
    // Milestone 5 is needed). up == down == 1 means "no conversion".
    bool ratioToCore (int hostRate, int& up, int& down, int maxUp = 1000);

    class Resampler
    {
    public:
        // inRate/outRate are only used to design the filter cutoff; the ratio itself is up/down.
        void prepare (int up, int down, double inRate, double outRate, int tapsPerPhase = 32);
        void reset();

        // Pushes nIn samples; writes the produced samples to out (capacity outCapacity); returns the number produced.
        // out must be large enough: at most ceil(nIn * up / down) + 1.
        int process (const float* in, int nIn, float* out, int outCapacity);

        // Group delay in output samples (the filter's pure delay; not counting any block pre-roll).
        double latencyOutputSamples() const;
        bool isBypass() const { return up_ == 1 && down_ == 1; }
        int up() const { return up_; }
        int down() const { return down_; }

    private:
        int up_ = 1, down_ = 1, taps_ = 32;
        std::vector<float> h_;      // taps_ * up_ coefficients, h_[phase * taps_ + k]
        std::vector<float> hist_;   // 2 * taps_ (doubled ring so a contiguous window is always available)
        int histPos_ = 0;
        int phase_ = 0;             // p in [0, up_)
        int64_t i0_ = 0;            // input index of the next output, relative to the newest input
        int64_t newest_ = -1;       // index of the newest input pushed
    };
}
