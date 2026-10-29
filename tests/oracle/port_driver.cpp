// Drives src/core (the port) over the same stimulus file as oracle_driver.cpp and writes a trace in the same format.
//   port_driver trace  <stimulus> <trace-out>
//   port_driver tables <table-dump-out>
// Test code only.
#include <memory>

#include "Engine.h"
#include "trace_common.h"

using namespace smrtest;

static smr::Controls controlsOf (const Step& s)
{
    smr::Controls c;
    for (int k = 0; k < 15; ++k) c.adc[k] = s.adc[k];
    for (int k = 0; k < 7; ++k) c.pot[k] = s.pot[k];
    c.env_mode_pre   = s.sw & 1;
    c.mod246_six     = s.sw & 2;
    c.mod135_one     = s.sw & 4;
    c.range_mode     = s.sw & 8;
    c.env_speed_fast = s.sw & 16;
    c.env_speed_slow = s.sw & 32;
    c.rotary_sw      = s.sw & 64;
    c.cvlag          = s.sw & 128;
    return c;
}

static void emitState (Trace& t, const smr::Engine& e)
{
    View v;
    v.outL = e.filtered_buffer;
    v.outR = e.filtered_bufferR;
    v.envPwm = e.ENVOUT_PWM;
    v.envPreload = e.ENVOUT_preload;
    v.qval = e.qval;
    v.note = e.note;
    v.scale = e.scale;
    v.scaleBank = e.scale_bank;
    v.morphpos = e.motion_morphpos;
    v.fadetoNote = e.motion_fadeto_note;
    v.fadetoScale = e.motion_fadeto_scale;
    v.spreadDest = e.motion_spread_dest;
    v.spreadDir = e.motion_spread_dir;
    v.scaleDest = e.motion_scale_dest;
    v.scalecvOverage = e.motion_scalecv_overage;
    v.nudge = e.freq_nudge;
    v.shift = e.freq_shift;
    v.level = e.channel_level;
    v.lock = e.lock;
    v.lockPressed = e.lock_pressed;
    v.qLocked = e.q_locked;
    v.lockDown = e.lock_down;
    v.motionRotate = e.motion_rotate;
    v.motionNotejump = e.motion_notejump;
    v.spread = e.spread;
    v.filterType = (uint32_t) e.filter_type;
    v.filterMode = (uint32_t) e.filter_mode;
    v.envTrackMode = (uint32_t) e.env_track_mode;
    v.levelLpf = e.CHANNEL_LEVEL_LPF;
    v.fMorph = e.f_morph;
    v.ongoingCoarse = e.ongoing_coarse_tuning;
    v.ongoingFine = e.ongoing_fine_tuning;
    v.fineTimeout = e.fine_tuning_timeout;
    v.fineTimer = e.fine_timer;
    v.savedEnvled = (const int32_t*) e.saved_envled_state;
    v.curEnvled = e.cur_envled_state;
    v.fineEnvled = e.fine_envled;
    v.coarseAdj = e.coarse_adj;
    v.coarseAdjLed = e.coarse_adj_led;
    v.numClearStaged = e.num_clear_coarse_staged;
    v.freqblock = e.freqblock;
    v.numFreqBlocked = e.num_freq_blocked;
    v.handledRelease = e.already_handled_lock_release;
    emit (t, v);
}

static int runTrace (const char* stimPath, const char* outPath)
{
    std::vector<Step> steps;
    if (! readStimulus (stimPath, steps) || steps.empty()) { std::printf ("FAIL: cannot read stimulus %s\n", stimPath); return 2; }

    std::unique_ptr<smr::Engine> e (new smr::Engine());
    e->setControls (controlsOf (steps[0]));
    e->reset();

    Trace tr;
    int32_t outA[16], outB[16];
    // The comparison is only meaningful before the first output saturation: the firmware converts float to int32 with ARM
    // saturation, the x86 oracle converts overflow to INT_MIN. valid_steps counts the steps before that block.
    uint32_t validSteps = 0;
    bool saturated = false;
    for (const Step& s : steps)
    {
        if (s.set_filter_mode) e->setFilterMode ((smr::FilterMode) s.set_filter_mode);
        if (s.set_filter_type) e->changeFilterType (s.set_filter_type == 1 ? smr::BPRE : smr::MAXQ);
        for (int k = 0; k < 6; ++k)
            if ((s.set_bank_mask >> k) & 1) e->scale_bank[k] = s.bank[k];
        for (int k = 0; k < 6; ++k)
        {
            if (s.set_lock)
            {
                e->lock[k] = (s.lock_mask >> k) & 1;
                e->q_locked[k] = (s.q_locked_mask >> k) & 1;
            }
            if (! (s.events & 64))   // bit 6: the Lock button pins drive lock_pressed[] and lock_down[] (Milestone 7 step 4)
            {
                e->lock_pressed[k] = (s.lock_pressed_mask >> k) & 1;
                e->lock_down[k] = s.lock_down_big ? 40000u : 0u;
            }
        }
        if (s.lock_down_near)
            for (int k = 0; k < 6; ++k) e->lock_down[k] = 49990u;
        if (! (s.events & 32)) e->freqblock = s.freqblock;

        for (int k = 0; k < 6; ++k) e->setLockButton (k, (s.lock_button_mask >> k) & 1);
        e->setControls (controlsOf (s));
        if (s.events & 1) e->triggerRotateUp();
        if (s.events & 2) e->triggerRotateDown();
        if (s.events & 4) e->triggerLock135();
        if (s.events & 8) e->triggerLock246();
        if (s.events & 16) e->triggerRotaryPress();

        e->processAudioOnly (s.inL, s.inR, outA, outB);

        for (unsigned n = 0; n < s.tim9; ++n)  e->runTim9Tick();
        for (unsigned n = 0; n < s.tim10; ++n) e->runTim10Tick();
        for (unsigned n = 0; n < s.loop; ++n)  e->runLoopPass();

        if (e->saturation_count != 0) saturated = true;
        if (! saturated) ++validSteps;
        tr.beginStep();
        emitState (tr, *e);
        tr.endStep();
    }
    std::printf ("saturation_count=%llu loop_guard_trips=%llu valid_steps=%u\n", (unsigned long long) e->saturation_count, (unsigned long long) e->loop_guard_trips, validSteps);
    return tr.write (outPath) ? 0 : 2;
}

static int runTables (const char* outPath)
{
    const smr::Tables& tb = smr::tables();
    Trace tr;
    tr.beginStep();

    dumpWords (tr, "exp_4096", tb.exp_4096, 4096);
    dumpWords (tr, "exp_1voct", tb.exp_1voct, 4096);
    dumpWords (tr, "log_4096", tb.log_4096, 4096);
    dumpWords (tr, "twopass_calibration", tb.twopass_calibration, 3380);

    std::unique_ptr<smr::Engine> e (new smr::Engine());
    e->reset();   // loads the factory-default user scale bank (bank 19), as the oracle does with set_default_user_scalebank()
    char nm[48];
    int32_t a[16] = {}, b[16] = {};
    for (int type = 0; type < 2; ++type)
    {
        for (int bank = 0; bank < 20; ++bank)
        {
            if (type == 1 && bank == 19) continue;
            e->setFilterMode (smr::ONEPASS);
            e->changeFilterType (type == 0 ? smr::MAXQ : smr::BPRE);
            for (int k = 0; k < 6; ++k) e->scale_bank[k] = (uint8_t) bank;
            e->processAudioOnly (a, b, a, b);
            std::snprintf (nm, sizeof nm, "%s_bank%d_hi", type == 0 ? "maxq" : "bpre", bank);
            dumpWords (tr, nm, e->c_hiq[0], type == 0 ? 231 : 693);
            if (type == 1)
            {
                std::snprintf (nm, sizeof nm, "bpre_bank%d_lo", bank);
                dumpWords (tr, nm, e->c_loq[0], 693);
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
    std::printf ("usage: port_driver trace <stimulus> <out> | tables <out>\n");
    return 2;
}
