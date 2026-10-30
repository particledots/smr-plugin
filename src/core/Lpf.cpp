// Derived from the Spectral Multiband Resonator firmware (third_party/SMR, v5.1.1, commit 79bc953).
// Original authors: Dan Green. Copyright (c) 2015 Dan Green. MIT licence: see THIRD-PARTY-LICENSES.txt and licenses/SMR-LICENSE.txt.
// Port of third_party/SMR/lpf.c. Statement order, types and conversions follow the C source.
#include "Lpf.h"

namespace smr
{
    // lpf.c setup_fir_filter
    void setup_fir_filter (OAnalog* a)
    {
        uint8_t i;

        //range check the lpf filter size
        if (a->fir_lpf_size > kMaxFirLpfSize) a->fir_lpf_size = kMaxFirLpfSize;

        //initialize the moving average buffer
        for (i = 0; i < kMaxFirLpfSize; i++)
        {
            //Bipolar CVs default to value of 2048
            //Other CVs default to 0
            if (a->polarity == AP_BIPOLAR)
                a->fir_lpf[i] = 2048;
            else
                a->fir_lpf[i] = 0;
        }

        //set inital values (same chained assignment as the C source)
        a->lpf_val = a->raw_val = a->bracketed_val = (int16_t) a->fir_lpf[0];
    }

    // lpf.c apply_fir_lpf
    void apply_fir_lpf (OAnalog* a)
    {
        float old_value, new_value;

        old_value = (float) a->fir_lpf[a->fir_lpf_i];   //oldest value: remove this from the array
        new_value = (float) a->raw_val;                 //new value: insert this into the array

        a->fir_lpf[a->fir_lpf_i] = (int32_t) new_value;

        //Increment the index, wrapping around the whole buffer
        if (++a->fir_lpf_i >= a->fir_lpf_size) a->fir_lpf_i = 0;

        //Calculate the arithmetic average (FIR LPF)
        float sum = (a->lpf_val * (float) a->fir_lpf_size) - old_value;   // -ffp-contract=off keeps these separate
        sum = sum + new_value;
        a->lpf_val = sum / (float) (a->fir_lpf_size);

        //Range check
        if (a->lpf_val < 0.0f) a->lpf_val = 0.0f;
        else if (a->lpf_val > 4095.0f) a->lpf_val = 4095.0f;
    }

    // lpf.c apply_bracket
    void apply_bracket (OAnalog* a)
    {
        int16_t diff;

        diff = (int16_t) ((int16_t) a->lpf_val - a->bracketed_val);

        if (diff > a->bracket_size)
        {
            a->bracketed_val = (int16_t) ((uint16_t) a->lpf_val - a->bracket_size);
        }
        else if (diff < (-1 * (a->bracket_size)))
        {
            a->bracketed_val = (int16_t) ((uint16_t) a->lpf_val + a->bracket_size);
        }

        if (a->bracketed_val < 0)         a->bracketed_val = 0;
        else if (a->bracketed_val > 4095) a->bracketed_val = 4095;
    }
}
