#include "Resampler.h"

#include <algorithm>
#include <cmath>

#include "Constants.h"

namespace smr_host
{
    namespace
    {
        int gcdInt (int a, int b) { while (b) { const int t = a % b; a = b; b = t; } return a; }

        // Modified Bessel function of the first kind, order 0 (series)
        double bessel0 (double x)
        {
            double sum = 1.0, term = 1.0;
            const double q = x * x * 0.25;
            for (int k = 1; k < 60; ++k)
            {
                term *= q / ((double) k * (double) k);
                sum += term;
                if (term < 1e-18 * sum) break;
            }
            return sum;
        }
    }

    bool ratioToCore (int hostRate, int& up, int& down, int maxUp)
    {
        if (hostRate <= 0) return false;
        // core rate = kCoreRateNum / kCoreRateDen; ratio = core / host = kCoreRateNum / (kCoreRateDen * host)
        const int64_t num = (int64_t) smr::kCoreRateNum;
        const int64_t den = (int64_t) smr::kCoreRateDen * (int64_t) hostRate;
        int64_t a = num, b = den;
        while (b) { const int64_t t = a % b; a = b; b = t; }
        const int64_t u = num / a, d = den / a;
        if (u > maxUp || d > 1000000) return false;
        up = (int) u;
        down = (int) d;
        return true;
    }

    void Resampler::prepare (int up, int down, double inRate, double outRate, int tapsPerPhase)
    {
        const int g = gcdInt (up, down);
        up_ = up / g;
        down_ = down / g;
        taps_ = tapsPerPhase;

        if (isBypass())
        {
            h_.clear();
            hist_.clear();
            reset();
            return;
        }

        // Prototype low-pass at the up-sampled rate, Kaiser window (beta 9, roughly 90 dB stop band), length taps_ * up_.
        // Cut-off: 0.46 of the lower of the two rates, leaving a transition band below the lower Nyquist frequency.
        const int N = taps_ * up_;
        const double upRate = inRate * (double) up_;
        const double lowerRate = inRate < outRate ? inRate : outRate;
        const double fc = 0.46 * lowerRate / upRate;   // cycles per sample at the up-sampled rate
        const double beta = 9.0;
        const double centre = (double) (N - 1) * 0.5;
        const double i0b = bessel0 (beta);
        const double pi = 3.14159265358979323846;

        std::vector<double> proto ((size_t) N);
        double sum = 0.0;
        for (int m = 0; m < N; ++m)
        {
            const double x = (double) m - centre;
            const double arg = 2.0 * fc * x;
            const double s = std::fabs (x) < 1e-12 ? 2.0 * fc : std::sin (pi * arg) / (pi * x);
            const double r = 2.0 * (double) m / (double) (N - 1) - 1.0;
            const double w = bessel0 (beta * std::sqrt (std::fmax (0.0, 1.0 - r * r))) / i0b;
            proto[(size_t) m] = s * w;
            sum += proto[(size_t) m];
        }
        // Gain: the sum over all taps must be up_ (each phase then has gain close to 1).
        const double scale = (double) up_ / sum;

        h_.assign ((size_t) N, 0.0f);
        for (int p = 0; p < up_; ++p)
            for (int k = 0; k < taps_; ++k)
                h_[(size_t) p * (size_t) taps_ + (size_t) k] = (float) (proto[(size_t) (p + k * up_)] * scale);

        hist_.assign ((size_t) taps_ * 2, 0.0f);
        reset();
    }

    void Resampler::reset()
    {
        std::fill (hist_.begin(), hist_.end(), 0.0f);
        histPos_ = 0;
        phase_ = 0;
        i0_ = 0;
        newest_ = -1;
    }

    int Resampler::process (const float* in, int nIn, float* out, int outCapacity)
    {
        if (isBypass())
        {
            const int n = nIn < outCapacity ? nIn : outCapacity;
            for (int i = 0; i < n; ++i) out[i] = in[i];
            return n;
        }

        int produced = 0;
        for (int i = 0; i < nIn; ++i)
        {
            // push one input sample into the doubled ring (newest at index histPos_ + taps_ - 1 of the window)
            histPos_ = (histPos_ + 1) % taps_;
            hist_[(size_t) histPos_] = in[i];
            hist_[(size_t) histPos_ + (size_t) taps_] = in[i];
            ++newest_;

            while (i0_ <= newest_)
            {
                if (produced >= outCapacity) return produced;

                // y = sum_k h[p + k*up] * x[i0 - k]; with i0 == newest this is the newest sample at k = 0
                const int64_t back = newest_ - i0_;   // how many samples older than the newest the base sample is (>= 0)
                float acc = 0.0f;
                if (back < taps_)
                {
                    const float* coef = &h_[(size_t) phase_ * (size_t) taps_];
                    for (int k = 0; k < taps_ - (int) back; ++k)
                    {
                        // sample x[i0 - k] is (back + k) samples older than the newest
                        const int idx = histPos_ + taps_ - (int) back - k;
                        acc += coef[k] * hist_[(size_t) idx];
                    }
                }
                out[produced++] = acc;

                phase_ += down_;
                i0_ += phase_ / up_;
                phase_ %= up_;
            }
        }
        return produced;
    }

    double Resampler::latencyOutputSamples() const
    {
        if (isBypass()) return 0.0;
        return (double) (taps_ * up_ - 1) * 0.5 / (double) down_;
    }
}
