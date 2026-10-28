// src/plugin: JUCE host glue. Parameters, state and audio hand-off. Holds no DSP of its own: the sound comes from
// src/core through src/host (BlockAdapter). Milestone 4: every control is a host parameter and the core is wired in.
#pragma once

#include <atomic>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>

#include "BlockAdapter.h"
#include "DisplayModel.h"
#include "ParamMap.h"

class SmrAudioProcessor : public juce::AudioProcessor, private juce::Timer
{
public:
    SmrAudioProcessor();
    ~SmrAudioProcessor() override { stopTimer(); }

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return kTailSeconds; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState& parameters() { return apvts; }

    // For the editor's status line (written by the audio side, read by the message thread).
    struct Status
    {
        double hostRate = 0.0;
        bool rateSupported = false;
        int latencySamples = 0;
        uint64_t underruns = 0;
        uint32_t trigSeen[6] = {};      // rising edges the audio side has seen: Rotate CW, Rotate CCW, Scale Up, Scale Down, Lock jack 1 3 5, Lock jack 2 4 6
    };
    Status status() const;

    // For the window's trigger buttons: how many rising and falling edges of each trigger parameter the audio side has seen
    // (0 Rotate CW, 1 Rotate CCW, 2 Scale Up, 3 Scale Down, 4 Lock jack 1 3 5, 5 Lock jack 2 4 6). The window holds a trigger until the audio side has seen it.
    uint32_t triggerRisingCount (int which) const noexcept  { return (unsigned) which < 6u ? trigSeenStat[which].load() : 0u; }
    uint32_t triggerFallingCount (int which) const noexcept { return (unsigned) which < 6u ? trigReleasedStat[which].load() : 0u; }

    // Read-only copy of core state for the window's lights and meters (Milestone 6b). Written by the audio thread after each host
    // block, read by the editor; nothing flows back (docs/m6b-led-notes.md).
    smr_plugin::DisplayHub& displayHub() noexcept { return display; }

    // Milestone 7: copies lock[] changes made by the core (Lock button tap or hold, Lock jacks) into the host Lock parameters.
    // Message thread only. Runs from a timer; public so the headless tests can call it. docs/open-questions.md M7-b.
    void syncLockParameters();

    // Resonators ring after the input stops; hosts use this to keep rendering a bounce (docs/open-questions.md M4-g).
    static constexpr double kTailSeconds = 10.0;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void readValues (float* values) const noexcept;
    // Controls first, then Engine::reset(), then the snapshot, then the initial apply (docs/open-questions.md M4-b, D3).
    void restart (const float* values, const smr::Snapshot* snapshot);

    juce::AudioProcessorValueTreeState apvts;
    std::atomic<float>* raw[smr_plugin::kNumParams] = {};

    smr_host::BlockAdapter adapter;
    smr_plugin::ParamApplier applier;
    std::vector<float> scratch;     // second output slot when the host output is mono
    smr_plugin::DisplayHub display;

    void timerCallback() override { syncLockParameters(); }
    std::atomic<uint32_t> lockSyncMask { 0 };      // channels whose lock[] the core changed (written by the audio thread)
    std::atomic<uint32_t> lockSyncValues { 0 };    // their new values

    // Hand-off between the message thread (project load and save) and the audio thread. Try-locks on the audio side.
    juce::SpinLock stateLock;
    struct Pending { bool active = false; bool hasSnapshot = false; smr::Snapshot snapshot; } pending;
    smr::Snapshot latest;           // last snapshot published by the audio side
    bool haveLatest = false;

    std::atomic<double> hostRateStat { 0.0 };
    std::atomic<bool> rateSupportedStat { false };
    std::atomic<int> latencyStat { 0 };
    std::atomic<uint64_t> underrunStat { 0 };
    std::atomic<uint32_t> trigSeenStat[6] = {};
    std::atomic<uint32_t> trigReleasedStat[6] = {};
    bool rateSupported = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SmrAudioProcessor)
};
