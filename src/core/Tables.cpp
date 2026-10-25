// Includes the upstream table headers verbatim (no copies, no edits) inside a namespace, then
// copies them into flat core-owned arrays. See docs/architecture.md section 13.2.
#include "Tables.h"

#include <stdint.h>
#include <cstring>

namespace smr_fw
{
    // Upstream headers: they contain no #include of their own (checked by grep in Milestone 2 design).
#include "../../third_party/SMR/exp_4096.h"
#include "../../third_party/SMR/exp_1voct.h"
#include "../../third_party/SMR/log_4096.h"
#include "../../third_party/SMR/twopass_calibration.h"

    // The 19 coefficient files that filter.c uses (gamma and video are not referenced by any code).
#include "../../third_party/SMR/filter_coefs/filter_coef_major.h"
#include "../../third_party/SMR/filter_coefs/filter_coef_minor.h"
#include "../../third_party/SMR/filter_coefs/filter_coef_western_eq.h"
#include "../../third_party/SMR/filter_coefs/filter_coef_western_twointerval_eq.h"
#include "../../third_party/SMR/filter_coefs/filter_coef_twelvetone.h"
#include "../../third_party/SMR/filter_coefs/filter_coef_diatonic_eq.h"
#include "../../third_party/SMR/filter_coefs/filter_coef_western.h"
#include "../../third_party/SMR/filter_coefs/filter_coef_western_twointerval.h"
#include "../../third_party/SMR/filter_coefs/filter_coef_diatonic_just.h"
#include "../../third_party/SMR/filter_coefs/filter_coef_indian.h"
#include "../../third_party/SMR/filter_coefs/filter_coef_shrutis.h"
#include "../../third_party/SMR/filter_coefs/filter_coef_mesopotamian.h"
#include "../../third_party/SMR/filter_coefs/filter_coef_gamelan.h"
#include "../../third_party/SMR/filter_coefs/filter_coef_alpha2.h"
#include "../../third_party/SMR/filter_coefs/filter_coef_alpha.h"
#include "../../third_party/SMR/filter_coefs/filter_coef_gamma_sp1.h"
#include "../../third_party/SMR/filter_coefs/filter_coef_17ET.h"
#include "../../third_party/SMR/filter_coefs/filter_coef_bohlen_pierce.h"
#include "../../third_party/SMR/filter_coefs/filter_coef_b296.h"
}

namespace smr
{
    namespace
    {
        // Bank order is filter.c's scale_bank order (docs/architecture.md section 3.2, "Bank map").
        struct BankSrc { const void* maxq; const void* hi; const void* lo; };

        const BankSrc kBanks[kNumStockBanks] = {
            /*  0 Major                  */ { smr_fw::filter_maxq_coefs_Major,                 smr_fw::filter_bpre_coefs_Major_800Q,                 smr_fw::filter_bpre_coefs_Major_2Q },
            /*  1 Minor                  */ { smr_fw::filter_maxq_coefs_Minor,                 smr_fw::filter_bpre_coefs_Minor_800Q,                 smr_fw::filter_bpre_coefs_Minor_2Q },
            /*  2 western_eq             */ { smr_fw::filter_maxq_coefs_western_eq,            smr_fw::filter_bpre_coefs_western_eq_800Q,            smr_fw::filter_bpre_coefs_western_eq_2Q },
            /*  3 western_twointerval_eq */ { smr_fw::filter_maxq_coefs_western_twointerval_eq, smr_fw::filter_bpre_coefs_western_twointerval_eq_800Q, smr_fw::filter_bpre_coefs_western_twointerval_eq_2Q },
            /*  4 twelvetone             */ { smr_fw::filter_maxq_coefs_twelvetone,            smr_fw::filter_bpre_coefs_twelvetone_800Q,            smr_fw::filter_bpre_coefs_twelvetone_2Q },
            /*  5 diatonic_eq            */ { smr_fw::filter_maxq_coefs_diatonic_eq,           smr_fw::filter_bpre_coefs_diatonic_eq_800Q,           smr_fw::filter_bpre_coefs_diatonic_eq_2Q },
            /*  6 western                */ { smr_fw::filter_maxq_coefs_western,               smr_fw::filter_bpre_coefs_western_800Q,               smr_fw::filter_bpre_coefs_western_2Q },
            /*  7 western_twointerval    */ { smr_fw::filter_maxq_coefs_western_twointerval,   smr_fw::filter_bpre_coefs_western_twointerval_800Q,   smr_fw::filter_bpre_coefs_western_twointerval_2Q },
            /*  8 diatonic_just          */ { smr_fw::filter_maxq_coefs_diatonic_just,         smr_fw::filter_bpre_coefs_diatonic_just_800Q,         smr_fw::filter_bpre_coefs_diatonic_just_2Q },
            /*  9 indian                 */ { smr_fw::filter_maxq_coefs_indian,                smr_fw::filter_bpre_coefs_indian_800Q,                smr_fw::filter_bpre_coefs_indian_2Q },
            /* 10 shrutis                */ { smr_fw::filter_maxq_coefs_shrutis,               smr_fw::filter_bpre_coefs_shrutis_800Q,               smr_fw::filter_bpre_coefs_shrutis_2Q },
            /* 11 mesopotamian           */ { smr_fw::filter_maxq_coefs_mesopotamian,          smr_fw::filter_bpre_coefs_mesopotamian_800Q,          smr_fw::filter_bpre_coefs_mesopotamian_2Q },
            /* 12 gamelan                */ { smr_fw::filter_maxq_coefs_gamelan,               smr_fw::filter_bpre_coefs_gamelan_800Q,               smr_fw::filter_bpre_coefs_gamelan_2Q },
            /* 13 alpha_spread2          */ { smr_fw::filter_maxq_coefs_alpha_spread2,         smr_fw::filter_bpre_coefs_alpha_spread2_800Q,         smr_fw::filter_bpre_coefs_alpha_spread2_2Q },
            /* 14 alpha_spread1          */ { smr_fw::filter_maxq_coefs_alpha_spread1,         smr_fw::filter_bpre_coefs_alpha_spread1_800Q,         smr_fw::filter_bpre_coefs_alpha_spread1_2Q },
            /* 15 gammaspread1           */ { smr_fw::filter_maxq_coefs_gammaspread1,          smr_fw::filter_bpre_coefs_gammaspread1_800Q,          smr_fw::filter_bpre_coefs_gammaspread1_2Q },
            /* 16 17ET                   */ { smr_fw::filter_maxq_coefs_17ET,                  smr_fw::filter_bpre_coefs_17ET_800Q,                  smr_fw::filter_bpre_coefs_17ET_2Q },
            /* 17 bohlen_pierce          */ { smr_fw::filter_maxq_coefs_bohlen_pierce,         smr_fw::filter_bpre_coefs_bohlen_pierce_800Q,         smr_fw::filter_bpre_coefs_bohlen_pierce_2Q },
            /* 18 B296                   */ { smr_fw::filter_maxq_coefs_B296,                  smr_fw::filter_bpre_coefs_B296_800Q,                  smr_fw::filter_bpre_coefs_B296_2Q },
        };
    }

    Tables::Tables()
    {
        static_assert (sizeof (smr_fw::exp_4096)  == sizeof (exp_4096),  "exp_4096 size");
        static_assert (sizeof (smr_fw::exp_1voct) == sizeof (exp_1voct), "exp_1voct size");
        static_assert (sizeof (smr_fw::log_4096)  == sizeof (log_4096),  "log_4096 size");
        static_assert (sizeof (smr_fw::twopass_calibration) == sizeof (twopass_calibration), "twopass_calibration size");

        std::memcpy (exp_4096,  smr_fw::exp_4096,  sizeof (exp_4096));
        std::memcpy (exp_1voct, smr_fw::exp_1voct, sizeof (exp_1voct));
        std::memcpy (log_4096,  smr_fw::log_4096,  sizeof (log_4096));
        std::memcpy (twopass_calibration, smr_fw::twopass_calibration, sizeof (twopass_calibration));

        // Upstream declares each MAXQ table as [231][3] but supplies only 231 initialisers, so only
        // the first 231 floats of the object are meaningful (docs/architecture.md section 3.2).
        for (int b = 0; b < kNumStockBanks; ++b)
        {
            std::memcpy (maxq[b],    kBanks[b].maxq, sizeof (maxq[b]));
            std::memcpy (bpre_hi[b], kBanks[b].hi,   sizeof (bpre_hi[b]));
            std::memcpy (bpre_lo[b], kBanks[b].lo,   sizeof (bpre_lo[b]));
        }
    }

    const Tables& tables()
    {
        static const Tables t;
        return t;
    }
}
