// Derived from the Spectral Multiband Resonator firmware (third_party/SMR, v5.1.1, commit 79bc953).
// Original authors: Dan Green. Copyright (c) 2015 Dan Green. MIT licence: see THIRD-PARTY-LICENSES.txt and licenses/SMR-LICENSE.txt.
// Port of third_party/SMR/lpf.c and lpf.h (MIT, Dan Green). Used only for the two Freq-jack conditioners.
#pragma once

#include <cstdint>

namespace smr
{
    constexpr int kMaxFirLpfSize = 40;   // lpf.h MAX_FIR_LPF_SIZE

    enum AnalogPolarity : int { AP_UNIPOLAR = 0, AP_BIPOLAR = 1 };

    // lpf.h struct o_analog (same fields, same types).
    struct OAnalog
    {
        uint16_t raw_val = 0;
        float    lpf_val = 0.0f;
        int16_t  bracketed_val = 0;

        uint16_t iir_lpf_size = 0;
        uint16_t fir_lpf_size = 0;
        int16_t  bracket_size = 0;
        AnalogPolarity polarity = AP_UNIPOLAR;

        int32_t  fir_lpf[kMaxFirLpfSize] = {};
        uint32_t fir_lpf_i = 0;
    };

    void setup_fir_filter (OAnalog* a);
    void apply_fir_lpf (OAnalog* a);
    void apply_bracket (OAnalog* a);
}
