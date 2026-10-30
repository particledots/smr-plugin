#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "StateCodec.h"

namespace
{
    const juce::Identifier kEngineTag { "SMR_ENGINE" };
}

juce::AudioProcessorValueTreeState::ParameterLayout SmrAudioProcessor::createLayout()
{
    using namespace smr_plugin;
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    for (int i = 0; i < kNumParams; ++i)
    {
        const ParamSpec& s = spec (i);
        const juce::ParameterID pid { s.id, 1 };

        switch (s.kind)
        {
            case Kind::Float:
                layout.add (std::make_unique<juce::AudioParameterFloat> (
                    pid, s.name, juce::NormalisableRange<float> (paramMin (i), paramMax (i)), paramDefault (i),
                    juce::AudioParameterFloatAttributes().withStringFromValueFunction ([i] (float v, int) { return juce::String (formatValue (i, v)); })));
                break;

            case Kind::Bool:
                layout.add (std::make_unique<juce::AudioParameterBool> (pid, s.name, paramDefault (i) >= 0.5f));
                break;

            case Kind::Choice:
            {
                juce::StringArray names;
                for (int k = 0; k < s.numChoices; ++k) names.add (s.choices[k]);
                layout.add (std::make_unique<juce::AudioParameterChoice> (pid, s.name, names, (int) paramDefault (i)));
                break;
            }
        }
    }
    return layout;
}

SmrAudioProcessor::SmrAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "SMR", createLayout())
{
    for (int i = 0; i < smr_plugin::kNumParams; ++i)
        raw[i] = apvts.getRawParameterValue (smr_plugin::spec (i).id);
    noiseIndex = smr_plugin::indexOfId ("noise");
    startTimerHz (30);
}

void SmrAudioProcessor::syncLockParameters()
{
    const uint32_t mask = lockSyncMask.exchange (0);
    if (mask == 0) return;
    const uint32_t vals = lockSyncValues.load();
    for (int i = 0; i < smr_plugin::kNumParams; ++i)
    {
        const smr_plugin::ParamSpec& s = smr_plugin::spec (i);
        if (s.role != smr_plugin::Role::Lock || ((mask >> s.channel) & 1u) == 0) continue;
        const float want = ((vals >> s.channel) & 1u) ? 1.0f : 0.0f;
        if (auto* p = apvts.getParameter (s.id))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (want);
            p->endChangeGesture();
        }
    }
}

void SmrAudioProcessor::readValues (float* values) const noexcept
{
    for (int i = 0; i < smr_plugin::kNumParams; ++i)
        values[i] = raw[i]->load();
}

void SmrAudioProcessor::restart (const float* values, const smr::Snapshot* snapshot)
{
    smr::Engine& engine = adapter.engine();
    engine.setControls (smr_plugin::ParamApplier::buildControls (values));   // sleep window of the Freq Nudge knobs centres here
    adapter.reset();                                                          // Engine::reset() keeps these controls
    if (snapshot != nullptr)
        engine.loadSnapshot (*snapshot);                                      // false (and no change) if a value is out of range
    applier.apply (engine, values, true);
}

void SmrAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const int rate = (int) std::lround (sampleRate);
    const int maxBlock = juce::jmax (1, samplesPerBlock);

    // Only the exact host/core rate ratios of src/host/Resampler are supported (docs/architecture.md 12.2).
    rateSupported = std::abs (sampleRate - (double) rate) < 0.5 && adapter.prepare (rate, maxBlock);
    scratch.assign ((size_t) maxBlock, 0.0f);
    noise.reset();

    hostRateStat.store (sampleRate);
    rateSupportedStat.store (rateSupported);

    if (! rateSupported)
    {
        setLatencySamples (0);
        latencyStat.store (0);
        display.publish (smr_plugin::DisplayData{});     // nothing is running: the window's lights go dark
        return;
    }

    const int latency = (int) std::ceil (adapter.latencyHostFrames());
    setLatencySamples (latency);
    latencyStat.store (latency);

    float values[smr_plugin::kNumParams];
    readValues (values);

    smr::Snapshot snap;
    bool haveSnap = false;
    {
        const juce::SpinLock::ScopedLockType lock (stateLock);
        if (pending.active)
        {
            snap = pending.snapshot;
            haveSnap = pending.hasSnapshot;
            pending.active = false;
        }
        else if (haveLatest)
        {
            snap = latest;      // the host re-prepared (new buffer size or rate): keep rotation, scale, bank and locks
            haveSnap = true;
        }
    }
    restart (values, haveSnap ? &snap : nullptr);
}

bool SmrAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& in  = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();

    // Stereo in, stereo out. Mono in, mono or stereo out. A mono output carries both output slots summed, as the
    // hardware does when only one OUT jack is patched (manual, Stereo/mono operation).
    if (in == juce::AudioChannelSet::stereo()) return out == juce::AudioChannelSet::stereo();
    if (in == juce::AudioChannelSet::mono())   return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
    return false;
}

void SmrAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    // Flush denormals on this thread: a plugin-side choice with no firmware equivalent (docs/open-questions.md M4-e).
    juce::ScopedNoDenormals noDenormals;

    const int n = buffer.getNumSamples();
    const int nIn = getTotalNumInputChannels();
    const int nOut = getTotalNumOutputChannels();
    if (n <= 0 || nOut < 1) return;

    if (! rateSupported)
    {
        // Unsupported host rate: pass the audio through unchanged (the editor says so).
        if (nIn == 1 && nOut > 1)
            for (int ch = 1; ch < nOut; ++ch) buffer.copyFrom (ch, 0, buffer, 0, 0, n);
        return;
    }

    float values[smr_plugin::kNumParams];
    readValues (values);

    // A project was loaded (or a state change arrived) since the last block.
    smr::Snapshot snap;
    bool restartNow = false, haveSnap = false;
    {
        const juce::SpinLock::ScopedTryLockType lock (stateLock);
        if (lock.isLocked() && pending.active)
        {
            snap = pending.snapshot;
            haveSnap = pending.hasSnapshot;
            pending.active = false;
            restartNow = true;
        }
    }
    if (restartNow)
        restart (values, haveSnap ? &snap : nullptr);

    smr::Engine& engine = adapter.engine();
    applier.apply (engine, values);
    for (int k = 0; k < 7; ++k)
    {
        trigSeenStat[k].store (applier.triggerEdges (k));
        trigReleasedStat[k].store (applier.triggerReleases (k));
    }

    float* ch0 = buffer.getWritePointer (0);
    float* ch1 = nOut > 1 ? buffer.getWritePointer (1) : nullptr;
    const float* in1 = nIn > 1 ? buffer.getReadPointer (1) : nullptr;   // null: mono input, copied to both input slots

    // Noise switch (M7-e, plugin only, stand-in generator): the host input is replaced on both input slots. One signal, mono style
    // (manual: the noise is normalled to the odds IN jack and evens IN is normalled to odds IN). The window's input meters and clip LED
    // below see this noise, because it is what the core is fed.
    if (noiseIndex >= 0 && values[noiseIndex] >= 0.5f)
    {
        noise.fill (ch0, n);
        in1 = nullptr;
    }

    // Window only (Milestone 6b): input peaks and the input clip test, read before the in-place processing overwrites them.
    // Positive samples only, as audio_util.c does (no absolute value); mono input feeds both slots.
    float peakIn0 = 0.0f, peakIn1 = 0.0f;
    bool inClip = false;
    {
        const float* a = ch0;
        const float* b = in1 != nullptr ? in1 : ch0;
        for (int k = 0; k < n; ++k)
        {
            peakIn0 = juce::jmax (peakIn0, std::abs (a[k]));
            peakIn1 = juce::jmax (peakIn1, std::abs (b[k]));
            inClip = inClip || a[k] > smr_plugin::kInputClipFloat || b[k] > smr_plugin::kInputClipFloat;
        }
    }
    float peakOut0 = 0.0f, peakOut1 = 0.0f;

    const int chunk = (int) scratch.size();
    for (int off = 0; off < n; off += chunk)
    {
        const int m = juce::jmin (chunk, n - off);
        float* outR = ch1 != nullptr ? ch1 + off : scratch.data();
        adapter.process (ch0 + off, in1 != nullptr ? in1 + off : nullptr, ch0 + off, outR, m);

        for (int k = 0; k < m; ++k)       // window only: the two output slots before a mono host output sums them
        {
            peakOut0 = juce::jmax (peakOut0, std::abs (ch0[off + k]));
            peakOut1 = juce::jmax (peakOut1, std::abs (outR[k]));
        }

        if (ch1 == nullptr)
            for (int k = 0; k < m; ++k) ch0[off + k] += scratch[(size_t) k];
    }

    for (int ch = 2; ch < nOut; ++ch)
        buffer.clear (ch, 0, n);

    // Milestone 7: tell the message thread about lock[] changes that came from the core.
    {
        uint32_t vals = 0;
        const uint32_t mask = smr_plugin::ParamApplier::coreLockChanges (engine, values, &vals);
        if (mask != 0)
        {
            lockSyncValues.fetch_and (~mask);
            lockSyncValues.fetch_or (vals);
            lockSyncMask.fetch_or (mask);
        }
    }

    // Window only: a read-only copy of core state, clip events and peaks. No locks, no allocation, no writes to the core.
    {
        smr_plugin::DisplayData dd;
        smr_plugin::fillDisplayData (engine, dd);
        display.publish (dd);
        display.addClipBits (smr_plugin::sliderClipBits (engine) | (inClip ? smr_plugin::kInputClipBit : 0u));
        display.notePeak (0, peakIn0);
        display.notePeak (1, peakIn1);
        display.notePeak (2, peakOut0);
        display.notePeak (3, peakOut1);
    }

    {
        const juce::SpinLock::ScopedTryLockType lock (stateLock);
        if (lock.isLocked())
        {
            latest = engine.saveSnapshot();
            haveLatest = true;
        }
    }
    underrunStat.store (adapter.underruns());
}

void SmrAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::ValueTree state = apvts.copyState();

    smr::Snapshot snap;
    bool have = false;
    {
        const juce::SpinLock::ScopedLockType lock (stateLock);
        if (pending.active) { snap = pending.snapshot; have = pending.hasSnapshot; }
        else if (haveLatest) { snap = latest; have = true; }
    }
    if (have)
    {
        juce::ValueTree eng (kEngineTag);
        eng.setProperty ("version", smr_plugin::kStateVersion, nullptr);
        eng.setProperty ("data", juce::String (smr_plugin::encodeSnapshot (snap)), nullptr);
        state.appendChild (eng, nullptr);
    }

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void SmrAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;

    juce::ValueTree tree = juce::ValueTree::fromXml (*xml);

    smr::Snapshot snap;
    bool have = false;
    juce::ValueTree eng = tree.getChildWithName (kEngineTag);
    if (eng.isValid())
    {
        have = (int) eng["version"] == smr_plugin::kStateVersion
               && smr_plugin::decodeSnapshot (eng["data"].toString().toStdString(), snap);
        tree.removeChild (eng, nullptr);
        // M1-h (Bryn, 2026-10-29): forbidden notes are not kept when a project is loaded, as on the hardware (manual p.16: only in
        // settings slots). The block is still written by encodeSnapshot (format unchanged); it is dropped here on load.
        if (have)
            snap.freqblock = 0;
    }

    apvts.replaceState (tree);

    // The audio thread (or prepareToPlay) applies it: the core is single threaded by design.
    const juce::SpinLock::ScopedLockType lock (stateLock);
    pending.active = true;
    pending.hasSnapshot = have;
    pending.snapshot = snap;
}

SmrAudioProcessor::Status SmrAudioProcessor::status() const
{
    Status s;
    s.hostRate = hostRateStat.load();
    s.rateSupported = rateSupportedStat.load();
    s.latencySamples = latencyStat.load();
    s.underruns = underrunStat.load();
    for (int k = 0; k < 7; ++k) s.trigSeen[k] = trigSeenStat[k].load();
    return s;
}

juce::AudioProcessorEditor* SmrAudioProcessor::createEditor()
{
    return new SmrAudioProcessorEditor (*this);
}

// Entry point JUCE looks for in every plugin format.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SmrAudioProcessor();
}
