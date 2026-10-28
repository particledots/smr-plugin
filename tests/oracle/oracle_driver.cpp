// Drives the UNMODIFIED firmware files (filter.c, params.c, rotation.c, lpf.c, envout_pwm.c, compiled by GCC with
// -fsingle-precision-constant, see the CI workflow) over a stimulus file and writes a trace.
//   oracle_driver trace  <stimulus> <trace-out>
//   oracle_driver tables <table-dump-out>
// Test code only. The flow of main.c (boot, then the while(1) body) and of the two interrupt handlers is
// written out here from main.c, params.c and envout_pwm.c, independently of src/core.
#include "trace_common.h"

extern "C"
{
    // firmware functions (C linkage; enum arguments are passed as unsigned int)
    void process_audio_block (int16_t* src, int16_t* dst, uint16_t ht);
    void change_filter_type (unsigned newtype);
    void set_default_param_values (void);
    void set_default_user_scalebank (void);
    void init_freq_lpf (void);
    void param_read_freq (void);
    void param_read_switches (void);
    int8_t read_spread (void);
    void update_spread (int8_t t_spread);
    void process_lock_jacks (void);
    void process_lock_buttons (void);
    void process_rotateCV (void);
    void process_scaleCV (void);
    void rotate_up (void);
    void rotate_down (void);
    void update_motion (void);
    void update_ENVOUT_PWM (void);

    // shim_globals.c
    void oracle_set_audio (const int32_t* l, const int32_t* r);
    void oracle_set_pins (uint32_t idrB, uint32_t idrD, uint32_t idrE);
    void oracle_set_lock_buttons (uint32_t mask);

    // firmware variables
    extern volatile uint16_t adc_buffer[16];
    extern volatile uint16_t potadc_buffer[7];
    extern uint8_t do_LOCK135, do_LOCK246, do_ROTUP, do_ROTDOWN;
    extern int8_t spread;
    extern unsigned filter_type, filter_mode, env_track_mode;
    extern uint32_t qval[6];
    extern uint8_t note[6], scale[6], scale_bank[6], lock[6], lock_pressed[6], q_locked[6];
    extern uint32_t lock_down[6];
    extern uint32_t freqblock;
    extern uint32_t ENVOUT_PWM[6];
    extern float ENVOUT_preload[6];
    extern float motion_morphpos[6], freq_nudge[6], freq_shift[6], channel_level[6];
    extern float CHANNEL_LEVEL_LPF, f_morph;
    extern int8_t motion_fadeto_note[6], motion_fadeto_scale[6], motion_spread_dest[6], motion_spread_dir[6];
    extern int8_t motion_scale_dest[6], motion_scalecv_overage[6];
    extern int8_t motion_rotate, motion_notejump;
    extern int32_t filtered_buffer[16], filtered_bufferR[16];
    extern uint8_t ongoing_coarse_tuning[2], ongoing_fine_tuning[2], fine_timer[2], num_clear_coarse_staged;
    extern uint32_t fine_tuning_timeout[2];
    extern int saved_envled_state[6], cur_envled_state, fine_envled;
    extern float coarse_adj[6], coarse_adj_led[6];
    extern float* c_hiq[6];
    extern float* c_loq[6];
    extern float exp_4096[4096], exp_1voct[4096], log_4096[4096];
    extern uint32_t twopass_calibration[3380];
    extern float user_scalebank[231];
}

using namespace smrtest;

static void setInputs (const Step& s)
{
    for (int k = 0; k < 15; ++k) adc_buffer[k] = s.adc[k] > 4095 ? 4095 : s.adc[k];
    for (int k = 0; k < 7; ++k) potadc_buffer[k] = s.pot[k] > 4095 ? 4095 : s.pot[k];

    // Pin levels. The macros in dig_inouts.h / rotary.h read active-low or active-high pins:
    //   ENV_MODE (GPIOD.8, low = PRE)   MOD246 (GPIOD.15, low = 6)   RANGE_MODE (GPIOD.14, high = on)
    //   ROTARY_SW (GPIOD.9, low = pressed)   MOD135 (GPIOB.2, low = 1)   ENVSPEEDSLOW (GPIOB.3, low = on)
    //   CVLAG (GPIOE.8, high = on)   ENVSPEEDFAST (GPIOE.9, low = on)
    const bool envPre = s.sw & 1, mod246 = s.sw & 2, mod135 = s.sw & 4, range = s.sw & 8, fast = s.sw & 16, slow = s.sw & 32, rot = s.sw & 64, lag = s.sw & 128;
    uint32_t B = 0, D = 0, E = 0;
    if (! envPre) D |= 1u << 8;
    if (! mod246) D |= 1u << 15;
    if (range)    D |= 1u << 14;
    if (! rot)    D |= 1u << 9;
    if (! mod135) B |= 1u << 2;
    if (! slow)   B |= 1u << 3;
    if (lag)      E |= 1u << 8;
    if (! fast)   E |= 1u << 9;
    oracle_set_pins (B, D, E);
}

// main.c, body of while(1), for the parts that are ported
static void loopPass()
{
    int32_t t_spread;

    param_read_switches();

    t_spread = read_spread();
    if (t_spread != -1) update_spread ((int8_t) t_spread);

    process_lock_jacks();

    process_lock_buttons();

    if (do_ROTUP)
    {
        do_ROTUP = 0;
        rotate_up();
    }

    if (do_ROTDOWN)
    {
        do_ROTDOWN = 0;
        rotate_down();
    }

    process_rotateCV();

    process_scaleCV();
}

// params.c TIM1_UP_TIM10_IRQHandler (update_slider_LEDs omitted)
static void tim10Tick()
{
    param_read_freq();
    update_motion();
}

static void boot (const Step& s0)
{
    setInputs (s0);

    set_default_param_values();
    set_default_user_scalebank();
    spread = (int8_t) ((adc_buffer[11] >> 8) + 1);   // SPREAD_ADC
    init_freq_lpf();

    // M2-d: one main-loop pass, then one TIM10 tick, before block 0
    loopPass();
    tim10Tick();
}

static void emitState (Trace& t)
{
    View v;
    v.outL = filtered_buffer;
    v.outR = filtered_bufferR;
    v.envPwm = ENVOUT_PWM;
    v.envPreload = ENVOUT_preload;
    v.qval = qval;
    v.note = note;
    v.scale = scale;
    v.scaleBank = scale_bank;
    v.morphpos = motion_morphpos;
    v.fadetoNote = motion_fadeto_note;
    v.fadetoScale = motion_fadeto_scale;
    v.spreadDest = motion_spread_dest;
    v.spreadDir = motion_spread_dir;
    v.scaleDest = motion_scale_dest;
    v.scalecvOverage = motion_scalecv_overage;
    v.nudge = freq_nudge;
    v.shift = freq_shift;
    v.level = channel_level;
    v.lock = lock;
    v.lockPressed = lock_pressed;
    v.qLocked = q_locked;
    v.lockDown = lock_down;
    v.motionRotate = motion_rotate;
    v.motionNotejump = motion_notejump;
    v.spread = spread;
    v.filterType = filter_type;
    v.filterMode = filter_mode;
    v.envTrackMode = env_track_mode;
    v.levelLpf = CHANNEL_LEVEL_LPF;
    v.fMorph = f_morph;
    v.ongoingCoarse = ongoing_coarse_tuning;
    v.ongoingFine = ongoing_fine_tuning;
    v.fineTimeout = fine_tuning_timeout;
    v.fineTimer = fine_timer;
    static_assert (sizeof (int) == 4, "int is 32 bit");
    v.savedEnvled = (const int32_t*) saved_envled_state;
    v.curEnvled = cur_envled_state;
    v.fineEnvled = fine_envled;
    v.coarseAdj = coarse_adj;
    v.coarseAdjLed = coarse_adj_led;
    v.numClearStaged = num_clear_coarse_staged;
    emit (t, v);
}

static int runTrace (const char* stimPath, const char* outPath)
{
    std::vector<Step> steps;
    if (! readStimulus (stimPath, steps) || steps.empty()) { std::printf ("FAIL: cannot read stimulus %s\n", stimPath); return 2; }

    boot (steps[0]);

    Trace tr;
    for (const Step& s : steps)
    {
        if (s.set_filter_mode) filter_mode = s.set_filter_mode;
        if (s.set_filter_type) change_filter_type (s.set_filter_type == 1 ? 0u : 1u);   // BPRE = 0, MAXQ = 1 (globals.h)
        for (int k = 0; k < 6; ++k)
            if ((s.set_bank_mask >> k) & 1) scale_bank[k] = s.bank[k];
        for (int k = 0; k < 6; ++k)
        {
            if (s.set_lock)
            {
                lock[k] = (s.lock_mask >> k) & 1;
                q_locked[k] = (s.q_locked_mask >> k) & 1;
            }
            lock_pressed[k] = (s.lock_pressed_mask >> k) & 1;
            lock_down[k] = s.lock_down_big ? 40000u : 0u;
        }
        if (s.lock_down_near)
            for (int k = 0; k < 6; ++k) lock_down[k] = 49990u;
        freqblock = s.freqblock;

        oracle_set_lock_buttons (s.lock_button_mask);
        setInputs (s);
        if (s.events & 1) do_ROTUP = 1;
        if (s.events & 2) do_ROTDOWN = 1;
        if (s.events & 4) do_LOCK135 = 1;
        if (s.events & 8) do_LOCK246 = 1;

        oracle_set_audio (s.inL, s.inR);
        process_audio_block (nullptr, nullptr, 0);

        for (unsigned n = 0; n < s.tim9; ++n)  update_ENVOUT_PWM();
        for (unsigned n = 0; n < s.tim10; ++n) tim10Tick();
        for (unsigned n = 0; n < s.loop; ++n)  loopPass();

        tr.beginStep();
        emitState (tr);
        tr.endStep();
    }
    return tr.write (outPath) ? 0 : 2;
}

// V1a: dump the tables that the firmware really uses, found the way filter.c finds them (by running its bank selection)
static int runTables (const char* outPath)
{
    Trace tr;
    tr.beginStep();

    dumpWords (tr, "exp_4096", exp_4096, 4096);
    dumpWords (tr, "exp_1voct", exp_1voct, 4096);
    dumpWords (tr, "log_4096", log_4096, 4096);
    dumpWords (tr, "twopass_calibration", twopass_calibration, 3380);

    Step s;
    std::memset (&s, 0, sizeof s);
    set_default_param_values();
    set_default_user_scalebank();

    char nm[48];
    for (int type = 0; type < 2; ++type)   // 0: MAXQ, 1: BPRE (ONEPASS)
    {
        for (int bank = 0; bank < 20; ++bank)
        {
            if (type == 1 && bank == 19) continue;   // no BpRe tables for the user bank (M1-o)
            filter_mode = 3;                         // ONEPASS: the only mode that loads BpRe tables
            change_filter_type (type == 0 ? 1u : 0u);
            for (int k = 0; k < 6; ++k) scale_bank[k] = (uint8_t) bank;
            oracle_set_audio (s.inL, s.inR);
            process_audio_block (nullptr, nullptr, 0);
            std::snprintf (nm, sizeof nm, "%s_bank%d_hi", type == 0 ? "maxq" : "bpre", bank);
            dumpWords (tr, nm, c_hiq[0], type == 0 ? 231 : 693);
            if (type == 1)
            {
                std::snprintf (nm, sizeof nm, "bpre_bank%d_lo", bank);
                dumpWords (tr, nm, c_loq[0], 693);
            }
        }
    }
    tr.endStep();
    return tr.write (outPath) ? 0 : 2;
}

int main (int argc, char** argv)
{
    if (argc >= 4 && std::strcmp (argv[1], "trace") == 0) return runTrace (argv[2], argv[3]);
    if (argc >= 3 && std::strcmp (argv[1], "tables") == 0) return runTables (argv[2]);
    std::printf ("usage: oracle_driver trace <stimulus> <out> | tables <out>\n");
    return 2;
}
