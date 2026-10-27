// src/plugin: the plugin's parameters and how they reach the core. Plain C++, no JUCE, so CI can test it headless.
//
// One table (kNumParams entries) is the single source of the parameter list: the JUCE layer builds the host
// parameters from it, and ParamApplier turns host values into smr::Controls and core calls. The firmware reads
// 12-bit ADC counts (0..4095); every level-type parameter is therefore 0..1 of that range (decision D5, percent,
// because no counts-to-volts scale is confirmed: docs/open-questions.md M1-b, M1-n). Mapping rules, with the
// firmware function that reads each value, are in docs/open-questions.md M4-a.
#pragma once

#include <cstdint>
#include <string>

#include "Engine.h"

namespace smr_plugin
{
    enum class Kind { Float, Bool, Choice };

    enum class Role
    {
        LevelSlider, LevelCv, QKnob, QCv, Morph, Spread, RotateCv, ScaleCv,
        NudgeOdds, NudgeEvens, FreqOdds, FreqEvens,
        OddsMode, EvensMode, PrePost, EnvSpeed, Slew, ScaleRotation,
        RotateCw, RotateCcw, ScaleUp, ScaleDown,
        Bank, Lock, FilterMode, FilterType
    };

    struct ParamSpec
    {
        const char* id;            // permanent once released: saved projects and automation refer to it
        const char* name;
        Kind kind;
        Role role;
        int channel;               // 0..5 for per-channel roles, else 0
        float minValue, maxValue;  // Float: range. Choice: 0..numChoices-1. Bool: 0..1. (Freq: see paramMax())
        float defaultValue;
        int numChoices;
        const char* const* choices;
    };

    constexpr int kNumParams = 41;

    const ParamSpec& spec (int index);
    int indexOfId (const char* id);                 // -1 if unknown
    int indexOfRole (Role role, int channel = 0);   // -1 if none

    float paramMin (int index);
    float paramMax (int index);                     // Freq parameters: the table's top, from exp_1voct.h
    float paramDefault (int index);
    std::string formatValue (int index, float value);

    // The Freq parameters are in octaves. The firmware reads the jack in ADC counts, subtracts BASE_TRACKOFFSET (11)
    // and looks the result up in exp_1voct[] (params.c param_read_freq). octavesToCounts() picks the count whose table
    // entry is closest to 2^octaves, i.e. a jack that is exactly calibrated (trackcomp 1, trackoffset 0, M4-c).
    float freqMaxOctaves();
    uint16_t octavesToCounts (float octaves);

    uint16_t unitToCounts (float unit);             // 0..1 to 0..4095

    // Turns host parameter values into core inputs and fires the actions the firmware has for them.
    class ParamApplier
    {
    public:
        // The ADC and switch inputs for these values (Freq parameters included). rotary_sw is always off here.
        static smr::Controls buildControls (const float* values);

        // Call once per host block, before audio. Sets the controls on the engine; locks, bank, filter mode and
        // type are applied when their value changes; the four triggers fire on a rising edge (value reaches 0.5).
        //
        // initial = true (after Engine::reset() or a snapshot load): locks and filter mode/type are set from the
        // values, bank and triggers are only remembered (the snapshot, not the Bank parameter, holds each
        // channel's bank), and nothing fires.
        void apply (smr::Engine& engine, const float* values, bool initial = false);

        // How many rising edges each trigger has had since this object was made (0 Rotate CW, 1 Rotate CCW, 2 Scale Up, 3 Scale Down).
        // A diagnostic for the window (docs/open-questions.md M6-c); it does not affect the audio.
        uint32_t triggerEdges (int which) const { return edges_[which & 3]; }
        // The same for falling edges (value goes from on to off). The window waits for this before it sends the next pulse.
        uint32_t triggerReleases (int which) const { return releases_[which & 3]; }

    private:
        float prev_[kNumParams] = {};
        uint32_t edges_[4] = {};
        uint32_t releases_[4] = {};
    };
}
