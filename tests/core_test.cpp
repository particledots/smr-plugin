// Headless test of src/core. Exit code 0 = all passed. Failures print a line starting "FAIL".
#include "PassThrough.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <vector>

static int failures = 0;

static void check (bool ok, const char* what)
{
    std::printf ("%s: %s\n", ok ? "PASS" : "FAIL", what);
    if (! ok)
        ++failures;
}

int main()
{
    smr::core::PassThrough core;
    const int frames = 4096;

    // Test signal: ramp, plus awkward values (negative zero, denormal, infinities, NaN, extremes).
    std::vector<float> src (frames);
    for (int i = 0; i < frames; ++i)
        src[(size_t) i] = std::sin (0.01f * (float) i) * 1.5f;
    src[10] = -0.0f;
    src[11] = std::numeric_limits<float>::denorm_min();
    src[12] = std::numeric_limits<float>::infinity();
    src[13] = -std::numeric_limits<float>::infinity();
    src[14] = std::numeric_limits<float>::quiet_NaN();
    src[15] = std::numeric_limits<float>::max();
    src[16] = std::numeric_limits<float>::lowest();

    // 1. Separate in/out buffers, stereo, bit-exact.
    {
        std::vector<float> inL = src, inR = src, outL (frames, 123.0f), outR (frames, 456.0f);
        const float* in[2]  = { inL.data(), inR.data() };
        float*       out[2] = { outL.data(), outR.data() };
        core.process (in, out, 2, frames);
        check (std::memcmp (outL.data(), src.data(), sizeof (float) * frames) == 0
               && std::memcmp (outR.data(), src.data(), sizeof (float) * frames) == 0,
               "separate buffers, stereo: output is bit-identical to input");
    }

    // 2. In place (same pointers), bit-exact, input untouched.
    {
        std::vector<float> bufL = src, bufR = src;
        const float* in[2]  = { bufL.data(), bufR.data() };
        float*       out[2] = { bufL.data(), bufR.data() };
        core.process (in, out, 2, frames);
        check (std::memcmp (bufL.data(), src.data(), sizeof (float) * frames) == 0
               && std::memcmp (bufR.data(), src.data(), sizeof (float) * frames) == 0,
               "in place, stereo: buffers unchanged");
    }

    // 3. Mono, odd block size, other block sizes.
    for (int n : { 1, 2, 31, 64, 127, 1000 })
    {
        std::vector<float> a (src.begin(), src.begin() + n), b ((size_t) n, 9.0f);
        const float* in[1]  = { a.data() };
        float*       out[1] = { b.data() };
        core.process (in, out, 1, n);
        char msg[96];
        std::snprintf (msg, sizeof msg, "mono, %d frames: bit-identical", n);
        check (std::memcmp (a.data(), b.data(), sizeof (float) * (size_t) n) == 0, msg);
    }

    // 4. Zero frames and zero channels do nothing and do not crash.
    {
        float sentinel = 7.0f;
        const float* in[1]  = { &sentinel };
        float*       out[1] = { &sentinel };
        core.process (in, out, 1, 0);
        core.process (in, out, 0, 16);
        check (sentinel == 7.0f, "zero frames / zero channels: no effect");
    }

    std::printf ("%s\n", failures == 0 ? "ALL PASSED" : "SOME FAILED");
    return failures == 0 ? 0 : 1;
}
