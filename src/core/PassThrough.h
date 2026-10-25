// src/core: DSP only. No JUCE, no plugin-host code, no hardware code.
// Milestone 0: an empty pass-through. The SMR filter pipeline replaces this in Milestone 2.
#pragma once

namespace smr::core
{
    class PassThrough
    {
    public:
        // Copies `numChannels` channels of `numFrames` samples from `in` to `out`, bit for bit.
        // `in` and `out` may be the same buffers (in place). Pointers may be equal per channel.
        void process (const float* const* in, float* const* out, int numChannels, int numFrames) const noexcept;
    };
}
