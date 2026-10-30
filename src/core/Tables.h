// Derived from the Spectral Multiband Resonator firmware (third_party/SMR, v5.1.1, commit 79bc953).
// Original authors: Dan Green, Hugo Paris. Copyright (c) 2015 Dan Green. MIT licence: see THIRD-PARTY-LICENSES.txt and licenses/SMR-LICENSE.txt.
// Firmware lookup tables, flattened. The numbers come from the unmodified upstream headers in
// third_party/SMR (included verbatim in Tables.cpp), copied once into these flat arrays.
#pragma once

#include <cstdint>

#include "Constants.h"

namespace smr
{
    struct Tables
    {
        float    exp_4096[kExpTableSize];            // exp_4096.h
        float    exp_1voct[kExpTableSize];           // exp_1voct.h
        float    log_4096[kExpTableSize];            // log_4096.h
        uint32_t twopass_calibration[kTwopassCalSize];  // twopass_calibration.h (declared 3380, 3354 initialisers)

        // Banks 0..18, same numbering as filter.c (scale_bank to table).
        float maxq[kNumStockBanks][kMaxqTableSize];      // filter_maxq_coefs_*   (231 floats)
        float bpre_hi[kNumStockBanks][kBpreTableSize];   // filter_bpre_coefs_*_800Q (693 floats)
        float bpre_lo[kNumStockBanks][kBpreTableSize];   // filter_bpre_coefs_*_2Q   (693 floats)

        Tables();
    };

    // Built once on first use (thread-safe), then read-only.
    const Tables& tables();
}
