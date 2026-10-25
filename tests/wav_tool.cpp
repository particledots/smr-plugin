// V6 helper. Generates synthetic WAV inputs and checks the renderer's output files.
//   wav_tool gen   OUT.wav RATE CHANNELS SECONDS      16-bit PCM: noise bursts and tones
//   wav_tool check OUT.wav RATE MIN_SECONDS           stereo, expected rate, long enough, all samples finite, not silent, peak sane
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "../tools/smr_render/wav.h"

int main (int argc, char** argv)
{
    if (argc >= 6 && std::strcmp (argv[1], "gen") == 0)
    {
        wav::Audio a;
        a.rate = std::atoi (argv[3]);
        a.channels = std::atoi (argv[4]);
        const double seconds = std::atof (argv[5]);
        const size_t frames = (size_t) (seconds * a.rate);
        a.data.resize (frames * (size_t) a.channels);
        uint32_t s = 99991u;
        for (size_t i = 0; i < frames; ++i)
        {
            s = s * 1664525u + 1013904223u;
            const double t = (double) i / a.rate;
            const bool burst = std::fmod (t, 1.0) < 0.15;
            double v = 0.3 * std::sin (2.0 * 3.14159265358979 * 220.0 * t) + 0.1 * std::sin (2.0 * 3.14159265358979 * 3100.0 * t);
            if (burst) v += 0.4 * ((double) (int32_t) s / 2147483648.0);
            for (int c = 0; c < a.channels; ++c)
                a.data[i * (size_t) a.channels + (size_t) c] = (float) (c == 0 ? v : 0.8 * v);
        }
        // write as 16-bit PCM through a small local writer (the shared writer is float only)
        std::vector<uint8_t> b;
        const uint32_t dataBytes = (uint32_t) (a.data.size() * 2);
        b.insert (b.end(), { 'R', 'I', 'F', 'F' });
        wav::put32 (b, 36 + dataBytes);
        b.insert (b.end(), { 'W', 'A', 'V', 'E', 'f', 'm', 't', ' ' });
        wav::put32 (b, 16);
        wav::put16 (b, 1);
        wav::put16 (b, (uint16_t) a.channels);
        wav::put32 (b, (uint32_t) a.rate);
        wav::put32 (b, (uint32_t) (a.rate * a.channels * 2));
        wav::put16 (b, (uint16_t) (a.channels * 2));
        wav::put16 (b, 16);
        b.insert (b.end(), { 'd', 'a', 't', 'a' });
        wav::put32 (b, dataBytes);
        for (float x : a.data) wav::put16 (b, (uint16_t) (int16_t) std::lrint (std::fmax (-1.0, std::fmin (1.0, (double) x)) * 32767.0));
        FILE* f = std::fopen (argv[2], "wb");
        if (! f || std::fwrite (b.data(), 1, b.size(), f) != b.size()) { std::printf ("FAIL: cannot write %s\n", argv[2]); return 1; }
        std::fclose (f);
        return 0;
    }

    if (argc >= 5 && std::strcmp (argv[1], "check") == 0)
    {
        wav::Audio a;
        std::string err;
        if (! wav::read (argv[2], a, err)) { std::printf ("FAIL: %s: %s\n", argv[2], err.c_str()); return 1; }
        const int rate = std::atoi (argv[3]);
        const double minSeconds = std::atof (argv[4]);
        float peak = 0.0f;
        bool finite = true;
        for (float x : a.data)
        {
            if (! std::isfinite (x)) finite = false;
            else peak = std::fmax (peak, std::fabs (x));
        }
        const double secs = (double) a.frames() / (double) a.rate;
        const bool ok = a.channels == 2 && a.rate == rate && secs >= minSeconds && finite && peak > 1e-6f && peak < 8.0f;
        std::printf ("%s: %s: %d ch, %d Hz, %.3f s, peak %.5f, finite %d\n", ok ? "PASS" : "FAIL", argv[2], a.channels, a.rate, secs, (double) peak, finite ? 1 : 0);
        return ok ? 0 : 1;
    }

    std::printf ("usage: wav_tool gen OUT RATE CHANNELS SECONDS | check OUT RATE MIN_SECONDS\n");
    return 2;
}
