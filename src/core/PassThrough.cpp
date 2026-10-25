#include "PassThrough.h"

#include <cstring>

namespace smr::core
{
    void PassThrough::process (const float* const* in, float* const* out, int numChannels, int numFrames) const noexcept
    {
        if (numFrames <= 0)
            return;

        for (int ch = 0; ch < numChannels; ++ch)
            if (in[ch] != out[ch])
                std::memmove (out[ch], in[ch], sizeof (float) * static_cast<std::size_t> (numFrames));
    }
}
