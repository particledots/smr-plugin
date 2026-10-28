// The SMR firmware DSP and the control logic that feeds it, ported from third_party/SMR (v5.1.1, 79bc953).
//
// One Engine is one module. All state that the firmware keeps in globals or function-local statics
// is a member of EngineState, under the firmware's own name, so each member can be checked against the
// C source. Nothing here touches hardware, files, threads or the host. See docs/architecture.md
// sections 10 to 14 and docs/port-notes.md for every place where the port is not a literal copy.
//
// Threading: one thread. All calls come from the audio thread (the plugin copies controls in between
// blocks). Real-time safe: no allocation, no locks, no exceptions after construction.
#pragma once

#include <cstdint>

#include "Constants.h"
#include "Lpf.h"
#include "Scheduler.h"
#include "Tables.h"

namespace smr
{
    // Everything the firmware reads from ADC pins and switch pins (dig_inouts.h macros), as plain values.
    // ADC values are 12-bit counts (0..4095); larger values are clamped by setControls().
    struct Controls
    {
        uint16_t adc[kNumAdcs]   = {};   // adc_buffer[]: see Constants.h for the index map
        uint16_t pot[kNumAdc3s]  = {};   // potadc_buffer[]: [0] Q pot, [1..6] sliders

        bool env_mode_pre   = false;  // ENV_MODE      true: PRE, false: POST (params.c param_read_switches)
        bool mod246_six     = false;  // MOD246        true: mod_mode_246 = 6, false: 246
        bool mod135_one     = false;  // MOD135        true: mod_mode_135 = 1, false: 135
        bool range_mode     = false;  // RANGE_MODE    true: rotate_to_next_scale = 1
        bool env_speed_fast = false;  // ENVSPEEDFAST  (switch up)
        bool env_speed_slow = false;  // ENVSPEEDSLOW  (switch down); neither = Trigger (centre)
        bool rotary_sw      = false;  // ROTARY_SW     ROTATE encoder pressed (only read when flipping to Fast)
        bool cvlag          = false;  // CVLAG         CV Slew switch
    };

    // What the firmware keeps in one parameter bank: system_mode.c store_params_into_sram() and
    // load_params_from_sram(). Knob and jack values are not part of it (they are physical in the hardware).
    // The plugin saves this next to the host parameters (docs/open-questions.md M4-b).
    struct Snapshot
    {
        uint8_t  filter_type = 1;      // FilterType (MAXQ = 1, BPRE = 0)
        uint8_t  filter_mode = 2;      // FilterMode (TWOPASS = 2, ONEPASS = 3)
        uint32_t freqblock = 0;
        uint8_t  note[kNumChannels] = {};
        uint8_t  scale[kNumChannels] = {};
        uint8_t  scale_bank[kNumChannels] = {};
        uint8_t  lock[kNumChannels] = {};
        uint8_t  q_locked[kNumChannels] = {};
        uint32_t qval[kNumChannels] = {};
        float    freq_nudge[kNumChannels] = {};
        uint8_t  hover_scale_bank = 0; // not stored by the firmware: load_params_from_sram() derives it (first unlocked channel's bank)
    };

    // All firmware state. Public so the tests and the oracle comparison can read and set it.
    // Names follow the C source; the file that owns each is in the comment.
    struct EngineState
    {
        // inputs (dig_inouts.h, main.c)
        uint16_t adc_buffer[kNumAdcs] = {};
        uint16_t potadc_buffer[kNumAdc3s] = {};
        Controls sw{};                      // switch pins; the ADC arrays inside it are not used

        // params.c
        uint16_t old_adc_buffer[kNumAdcs] = {};
        float    trackcomp[2]   = { 1.0f, 1.0f };
        int16_t  trackoffset[2] = { 0, 0 };
        float    freq_nudge[kNumChannels] = { 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f };   // initialiser {1.0,1.0} in params.c
        float    coarse_adj[kNumChannels] = { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f };
        float    freq_shift[kNumChannels] = {};
        uint16_t mod_mode_135 = 0, mod_mode_246 = 0;
        float    t_fo = 0.0f, t_fe = 0.0f;
        float    f_nudge_odds = 1.0f, f_nudge_evens = 1.0f;
        OAnalog  freq_jack_conditioning[2];

        uint8_t  lock[kNumChannels] = {};
        uint8_t  lock_pressed[kNumChannels] = {};
        uint8_t  lock_up[kNumChannels] = {};        // params.c: counts main-loop passes after a button release (the release is handled when it passes 5)
        uint8_t  lock_button[kNumChannels] = {};    // LOCKBUTTON(i) (dig_inouts.c): the Lock button pin, 1 while pressed. Set by the host with setLockButton().
        uint32_t lock_down[kNumChannels] = {};
        uint8_t  num_clear_coarse_staged = 0;
        // Display-only state of params.c param_read_freq (Milestone 7 step 3b, docs/port-notes.md P-8). Names are the C names.
        // Not read by the audio, the envelopes or any other core code.
        uint8_t  ongoing_coarse_tuning[2] = {};     // BINARY CONVENTION of the *_envled_state values: params.c comment above saved_envled_state
        uint8_t  ongoing_fine_tuning[2] = {};
        uint32_t fine_tuning_timeout[2] = {};
        uint8_t  fine_timer[2] = { 1, 1 };
        int      saved_envled_state[kNumChannels] = { 0b0001100, 0b0001100, 0b0001100, 0b0001100, 0b0001100, 0b0001100 };
        int      cur_envled_state = 0b0000000;
        int      fine_envled = 0b000000;
        float    coarse_adj_led[kNumChannels] = {};
        uint8_t  already_handled_lock_release[kNumChannels] = {};
        uint8_t  do_LOCK135 = 0, do_LOCK246 = 0, do_ROTUP = 0, do_ROTDOWN = 0;   // dig_inouts.c flags

        uint32_t env_prepost_mode = 0;
        EnvOutMode env_track_mode = ENV_SLOW;
        float    envspeed_attack = 0.0f, envspeed_decay = 0.0f;
        uint16_t rotate_to_next_scale = 0;
        uint32_t freqblock = 0;
        float    channel_level[kNumChannels] = {};
        float    CHANNEL_LEVEL_LPF = 0.0f;

        uint32_t qval[kNumChannels] = {};
        float    qval_goal[kNumChannels] = {};
        uint32_t qvalcv = 0, qvalpot = 0;
        uint8_t  q_locked[kNumChannels] = {};
        uint8_t  user_turned_Q_pot = 0;

        uint8_t  note[kNumChannels] = {};
        uint8_t  scale[kNumChannels] = {};
        uint8_t  scale_bank[kNumChannels] = {};
        uint8_t  hover_scale_bank = 0;      // params.c: the bank shown while ROTATE is held; applied to unlocked channels on release

        // user_scales.c
        float    user_scalebank[kMaxqTableSize] = {};

        // rotation.c
        int8_t   motion_fadeto_note[kNumChannels] = {};
        int8_t   motion_fadeto_scale[kNumChannels] = {};
        int8_t   motion_rotate = 0;
        int8_t   motion_spread_dest[kNumChannels] = {};
        int8_t   motion_spread_dir[kNumChannels] = {};
        int8_t   motion_notejump = 0;
        int8_t   motion_scale_dest[kNumChannels] = {};
        int8_t   motion_scalecv_overage[kNumChannels] = {};
        float    motion_morphpos[kNumChannels] = {};
        float    f_morph = 0.0f;
        int8_t   spread = 0;

        // envout_pwm.c
        uint32_t ENVOUT_PWM[kNumChannels] = {};
        float    ENVOUT_preload[kNumChannels] = {};
        float    stored_trigger_level[kNumChannels] = {};
        float    voltoct_pwm_tracking = 1.0f;

        // filter.c
        FilterType filter_type = MAXQ;
        FilterMode filter_mode = TWOPASS;
        FilterType new_filter_type = MAXQ;
        uint8_t  filter_type_changed = 0;
        const float* c_hiq[kNumChannels] = {};
        const float* c_loq[kNumChannels] = {};
        // buf[channel][scale][filter][3] and buf_a[...] as flat arrays; index with bufIdx().
        float    buf_a[kNumChannels * kNumScales * kNumFilts * 3] = {};
        float    buf  [kNumChannels * kNumScales * kNumFilts * 3] = {};
        float    filter_out[2 * kNumChannels][kBlockFrames] = {};   // the firmware declares [NUM_FILTS][16]; only 12 rows are used
        int32_t  left_buffer[kBlockFrames] = {}, right_buffer[kBlockFrames] = {};
        int32_t  filtered_buffer[kBlockFrames] = {}, filtered_bufferR[kBlockFrames] = {};

        // Counts audio outputs that had to be saturated at the float to int32 conversion (undefined in the C).
        uint64_t saturation_count = 0;

        // Counts the times a search loop in update_spread() / update_motion() gave up after a full cycle of
        // the 20 filter positions. The firmware would loop forever there (docs/port-notes.md item P-7).
        uint64_t loop_guard_trips = 0;

        static constexpr int bufIdx (int ch, int sc, int fl, int k) noexcept
        {
            return ((ch * kNumScales + sc) * kNumFilts + fl) * 3 + k;
        }

        // Function-local statics of the firmware, promoted to members. Names are the C names.
        struct ParamReadFreqStatics            // params.c param_read_freq
        {
            uint8_t sleep_range_saved = 0;
            uint8_t first_run[2] = { 1, 1 };
            uint8_t fknob_lock[2] = { 0, 0 };
            uint8_t coarse_lock[kNumChannels] = { 1, 1, 1, 1, 1, 1 };
            float   sleep_range = 800.0f;
            float   wakeup_evens[2] = {}, wakeup_odds[2] = {};
            float   old_f_nudge_odds = 1.0f, old_f_nudge_evens = 1.0f;
            float   f_nudge_odds_buf = 0.0f, f_nudge_evens_buf = 0.0f;
            float   f_nudge_buf[2] = {};
            float   f_shift_odds = 1.0f, f_shift_evens = 1.0f;
            float   saved_coarse_adj[kNumChannels] = { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f };
            uint8_t clear_coarse_staged[kNumChannels] = {};
            uint8_t old_switch_state[2] = {};
        } prf;

        struct ParamReadQStatics               // params.c param_read_q
        {
            float    qpot_lpf = 0.0f;
            float    old_qpot_lpf = 65535.0f;   // 0xFFFF
            uint32_t poll_ctr = 0;
            float    prev_qval[kNumChannels] = {};
        } prq;

        struct AudioBlockStatics               // filter.c process_audio_block
        {
            uint8_t  old_scale_bank[kNumChannels] = { 255, 255, 255, 255, 255, 255 };   // {-1,...} stored in uint8_t
            float    qval_b[kNumChannels] = {};
            float    qval_a[kNumChannels] = {};
            float    qc[kNumChannels] = {};
            uint32_t poll_ctr[kNumChannels] = {};
            float    prev_level[kNumChannels] = {};
            float    level_goal[kNumChannels] = {};
            float    level_inc[kNumChannels] = {};
        } pab;

        struct EnvStatics                      // envout_pwm.c update_ENVOUT_PWM
        {
            uint32_t env_trigout[kNumChannels] = {};
            uint32_t env_low_ctr[kNumChannels] = {};
            float    envelope[kNumChannels] = {};
        } env;

        uint32_t old_cvlag = 0xFFFF;                     // params.c param_read_switches
        uint16_t old_rotcv_adc = 0;                      // params.c process_rotateCV
        int8_t   rot_offset = 0, old_rot_offset = 0;
        int32_t  t_scalecv = 0, t_old_scalecv = 0;       // params.c process_scaleCV
        float    scalecv_lpf_buf = 0.0f;
        int8_t   old_spread = 1;                         // rotation.c update_spread
    };

    class Engine : public EngineState
    {
    public:
        Engine();
        Engine (const Engine&) = delete;
        Engine& operator= (const Engine&) = delete;

        // Mirrors main() up to the while(1) loop: defaults, tables, init_freq_lpf(), spread, then one
        // main-loop pass followed by one TIM10 tick, so that no block sees freq_shift at 0 (decision M2-d;
        // the order is explained in docs/port-notes.md item P-6). The controls last given to setControls()
        // are kept. Call setControls() first.
        void reset();

        // Copy new control values in. Call between blocks. ADC counts above 4095 are clamped
        // (the firmware's ADCs are 12 bit; larger values would index past the tables).
        void setControls (const Controls& c) noexcept;

        // Process exactly one block of kBlockFrames (16) frames at the core rate. Samples are int32 with
        // 24 significant bits left-justified, exactly as filter.c sees them (docs/architecture.md 12.3).
        //   inLeft / inRight : the two input slots (left_buffer, right_buffer). inLeft feeds channels 1,3,5.
        //   outFiltered      : filtered_buffer  = channels 2,4,6 (j odd)
        //   outFilteredR     : filtered_bufferR = channels 1,3,5 (j even)
        // After the audio block, the scheduler runs every TIM9 tick, TIM10 tick and main-loop pass that
        // fell due during the block, in that order. Inputs are copied before any output is written, so the
        // output pointers may equal the input pointers.
        void processBlock (const int32_t* inLeft, const int32_t* inRight,
                           int32_t* outFiltered, int32_t* outFilteredR);

        // The pieces of processBlock, exposed so the tests can replay a schedule exactly.
        void processAudioOnly (const int32_t* inLeft, const int32_t* inRight,
                               int32_t* outFiltered, int32_t* outFilteredR);
        void runTim9Tick();      // envout_pwm.c  update_ENVOUT_PWM()
        void runTim10Tick();     // params.c      TIM1_UP_TIM10_IRQHandler: param_read_freq(), update_motion()
        void runLoopPass();      // main.c        body of while(1), ported parts only (see Engine.cpp)

        // Events that the firmware turns into flags polled by the main loop (dig_inouts.c debounce
        // sets them; main.c consumes them). Debounce is the host's job. Consumed at the next loop pass.
        void triggerRotateUp()   noexcept { do_ROTUP = 1; }
        void triggerRotateDown() noexcept { do_ROTDOWN = 1; }
        void triggerLock135()    noexcept { do_LOCK135 = 1; }
        void triggerLock246()    noexcept { do_LOCK246 = 1; }

        // Direct calls (rotation.c); the rotary encoder UI that normally calls these is Milestone 7.
        void change_scale_up();
        void change_scale_down();

        // Filter selection as system_mode.c does it: filter_mode is set directly, filter_type through
        // change_filter_type() (applied at the start of the next block, which also clears history).
        void setFilterMode (FilterMode m) noexcept { filter_mode = m; }
        void changeFilterType (FilterType t) noexcept { filter_type_changed = 1; new_filter_type = t; }

        // freqblock is set by the lock-button UI in the firmware (params.c process_freq_blocks, Milestone 7).
        // The firmware allows at most 14 of the 20 positions to be blocked (num_freq_blocked<14), and
        // system_mode.c load_params_from_sram() discards a mask with more than 14 bits. Returns false and
        // changes nothing if mask has bits above bit 19 or more than 14 bits set.
        bool setFreqBlock (uint32_t mask) noexcept;

        // Plain inputs. Out-of-range channel numbers are ignored.
        // setLockButton: the Lock button pin. The main loop's process_lock_buttons() (Milestone 7) turns presses into lock_pressed[],
        // lock[] and q_locked[] exactly as the firmware does. setLock, setLockPressed and setLockDown write the state directly (tests).
        void setLockButton (int ch, bool pressed) noexcept { if ((unsigned) ch < (unsigned) kNumChannels) lock_button[ch] = pressed ? 1 : 0; }
        void setLock (int ch, bool v) noexcept         { if ((unsigned) ch < (unsigned) kNumChannels) lock[ch] = v ? 1 : 0; }
        void setLockPressed (int ch, bool v) noexcept  { if ((unsigned) ch < (unsigned) kNumChannels) lock_pressed[ch] = v ? 1 : 0; }
        void setLockDown (int ch, uint32_t n) noexcept { if ((unsigned) ch < (unsigned) kNumChannels) lock_down[ch] = n; }
        void setQLocked (int ch, bool v) noexcept      { if ((unsigned) ch < (unsigned) kNumChannels) q_locked[ch] = v ? 1 : 0; }

        // Lock button released in PLAY mode (params.c process_lock_buttons, release branch): latching on sets
        // lock[] and clears motion_spread_dir[]; latching off clears lock[] and q_locked[]. Does nothing if the
        // channel is already in that state. This is a tap that skips the press and release timing; the hold gestures
        // (Q-lock, coarse tuning, hold to unlock) go through setLockButton() and process_lock_buttons().
        void setLockLatched (int ch, bool on) noexcept;

        // The bank chosen with ROTATE held, applied on release (params.c process_rotary_button, ui_mode PLAY):
        // every channel whose lock[] is not 1 gets the bank. Out-of-range values are ignored.
        void setBank (int bank) noexcept;

        // Parameter bank as in system_mode.c. loadSnapshot() returns false and changes nothing if any value is out
        // of range (the firmware trusts its flash; a project file is not trusted). Otherwise it does what
        // load_params_from_sram() does, including change_filter_type() (applied at the start of the next block),
        // the 14-position limit on freqblock, qval_goal = qval, and the motion state reset.
        Snapshot saveSnapshot() const noexcept;
        bool loadSnapshot (const Snapshot& s) noexcept;

        Scheduler scheduler;

    private:
        void init_table_pointers() noexcept;

        // filter.c
        void process_audio_block (const int32_t* inLeft, const int32_t* inRight);
        void change_filter_type (FilterType newtype) noexcept;
        void select_tables_for_channel (int i);

        // params.c
        void set_default_param_values();
        void param_read_freq();
        void param_read_q();
        void param_read_switches();
        int8_t read_spread();
        void process_rotateCV();
        void process_scaleCV();
        void process_lock_jacks();
        void process_lock_buttons();
        void init_freq_lpf();
        void set_default_user_scalebank();

        // rotation.c
        void update_spread (int8_t t_spread);
        void update_morph();
        void update_motion();
        void rotate_down();
        void rotate_up();
        void jump_scale_with_cv (int8_t shift_amt);
        void jump_rotate_with_cv (int8_t shift_amt);
        bool is_morphing() const;

        // envout_pwm.c
        void update_ENVOUT_PWM();
    };

    // Float to integer conversions that are undefined in C when out of range. The port defines them
    // as saturating, with NaN giving 0 (docs/port-notes.md item P-3).
    inline int32_t f2i32 (float x) noexcept
    {
        if (! (x == x)) return 0;
        if (x >= 2147483648.0f) return INT32_MAX;
        if (x <= -2147483648.0f) return INT32_MIN;
        return (int32_t) x;
    }

    inline uint32_t f2u32 (float x) noexcept
    {
        if (! (x == x)) return 0;
        if (x <= 0.0f) return 0;
        if (x >= 4294967296.0f) return UINT32_MAX;
        return (uint32_t) x;
    }
}
