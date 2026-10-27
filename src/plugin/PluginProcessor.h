// src/plugin: JUCE host glue. Parameters, state and audio hand-off. Holds no DSP of its own: the sound comes from
// src/core through src/host (BlockAdapter). Milestone 4: every control is a host parameter and the core is wired in.
#pragma once

#include <atomic>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>

#include "BlockAdapter.h"
#include "ParamMap.h"

class SmrAudioProcessor : public juce::AudioProcessor
{
public:
    SmrAudioProcessor();
    ~SmrAudioProcessor() override = default;

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
        uint32_t trigSeen[4] = {};      // rising edges the audio side has seen: Rotate CW, Rotate CCW, Scale Up, Scale Down
    };
    Status status() const;

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

    // Hand-off between the message thread (project load and save) and the audio thread. Try-locks on the audio side.
    juce::SpinLock stateLock;
    struct Pending { bool active = false; bool hasSnapshot = false; smr::Snapshot snapshot; } pending;
    smr::Snapshot latest;           // last snapshot published by the audio side
    bool haveLatest = false;

    std::atomic<double> hostRateStat { 0.0 };
    std::atomic<bool> rateSupportedStat { false };
    std::atomic<int> latencyStat { 0 };
    std::atomic<uint64_t> underrunStat { 0 };
    std::atomic<uint32_t> trigSeenStat[4] = {};
    bool rateSupported = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SmrAudioProcessor)
};
