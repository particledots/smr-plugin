// Test-only definitions of the globals and hardware-facing functions that the five unmodified firmware files
// (filter.c, params.c, rotation.c, lpf.c, envout_pwm.c) reference but that live in files we do not compile
// (adc.c, dig_inouts.c, system_mode.c, leds.c, led_ring.c, rotary.c, audio_util.c, user_scales.c).
// Everything here is either a variable at its power-on value or an empty function. This file is never part of src/core.
#include <stdint.h>
#include <string.h>

#include "globals.h"
#include "dig_inouts.h"
#include "params.h"
#include "system_mode.h"
#include "leds.h"
#include "rotary.h"
#include "user_scales.h"
#include "audio_util.h"

GPIO_TypeDef oracle_gpio[7];
TIM_TypeDef  oracle_tim[4];

// adc.c
__IO uint16_t adc_buffer[NUM_ADCS + 1];
__IO uint16_t potadc_buffer[NUM_ADC3S];

// dig_inouts.c
uint8_t do_LOCK135 = 0, do_LOCK246 = 0, do_ROTUP = 0, do_ROTDOWN = 0;

// system_mode.c
enum UI_Modes ui_mode = PLAY;
uint8_t cur_param_bank = 0;
uint8_t cur_colsch = 0;

// leds.c / led_ring.c
const uint32_t slider_led[6] = { LED_SLIDER1, LED_SLIDER2, LED_SLIDER3, LED_SLIDER4, LED_SLIDER5, LED_SLIDER6 };
uint8_t slider_led_mode = DONT_SHOW_CLIPPING;
uint8_t flag_update_LED_ring = 0;

// rotary.c
uint32_t rotary_state = 0;

// user_scales.c: same variables; the default data is extracted from the upstream file by the CI step (see build.yml)
uint8_t editscale_notelocked = 0, editscale_tracklocked = 0, editscale_voctlocked = 0;
float user_scalebank[231];
#include "default_user_scalebank.inc"
void set_default_user_scalebank (void)
{
    uint8_t i, j;
    for (j = 0; j < NUMSCALES; j++)
    {
        for (i = 0; i < NUM_FILTS + 1; i++)
            user_scalebank[i + j * 21] = DEFAULT_user_scalebank[i];
    }
}

// Stubs for the parts of params.c that the port does not include (UI, System Mode). LOCKBUTTON follows the stimulus (Milestone 7).
void LOCKLED_ON (int led) { (void) led; }
void LOCKLED_OFF (int led) { (void) led; }
static uint32_t oracle_lock_pins = 0;   // bit i: Lock button i pressed (the stimulus sets it each step)
void oracle_set_lock_buttons (uint32_t mask) { oracle_lock_pins = mask; }
uint8_t LOCKBUTTON (uint8_t x) { return (uint8_t) ((oracle_lock_pins >> x) & 1u); }
void update_slider_LEDs (void) {}
uint8_t read_rotary (void) { return 0; }
void factory_reset (void) {}
void save_param_bank (uint8_t b) { (void) b; }
void load_param_bank (uint8_t b) { (void) b; }
void exit_select_colors_mode (void) {}
void enter_system_mode (void) {}
void exit_system_mode (uint8_t r) { (void) r; }
void enter_edit_color_mode (void) {}
void exit_edit_scale (uint8_t s) { (void) s; }
void enter_edit_scale (void) {}
void read_all_params_from_FLASH (void) {}
void store_globals_into_sram (void) {}
void write_all_params_to_FLASH (void) {}

// audio_util.c: the oracle driver supplies the 24-bit-in-32 samples directly and captures the result.
static int32_t oracle_in_l[16], oracle_in_r[16], oracle_out_l[16], oracle_out_r[16];

void oracle_set_audio (const int32_t* l, const int32_t* r)
{
    memcpy (oracle_in_l, l, sizeof oracle_in_l);
    memcpy (oracle_in_r, r, sizeof oracle_in_r);
}

void oracle_get_audio (int32_t* l, int32_t* r)
{
    memcpy (l, oracle_out_l, sizeof oracle_out_l);
    memcpy (r, oracle_out_r, sizeof oracle_out_r);
}

void audio_convert_2x16_to_stereo24 (uint16_t sz, int16_t* src, int32_t* ldst, int32_t* rdst)
{
    (void) sz; (void) src;
    memcpy (ldst, oracle_in_l, sizeof oracle_in_l);
    memcpy (rdst, oracle_in_r, sizeof oracle_in_r);
}

void audio_convert_stereo24_to_2x16 (uint16_t sz, int32_t* lsrc, int32_t* rsrc, int16_t* dst)
{
    (void) sz; (void) dst;
    memcpy (oracle_out_l, lsrc, sizeof oracle_out_l);
    memcpy (oracle_out_r, rsrc, sizeof oracle_out_r);
}

// Pin levels for the switch macros in dig_inouts.h / rotary.h (the driver computes them from the stimulus)
void oracle_set_pins (uint32_t idrB, uint32_t idrD, uint32_t idrE)
{
    GPIOB->IDR = idrB;
    GPIOD->IDR = idrD;
    GPIOE->IDR = idrE;
}
