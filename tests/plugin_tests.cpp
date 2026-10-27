// Milestone 5: the real plugin code (SmrAudioProcessor, JUCE) run headless in CI.
//
// Two modes, one executable (a JUCE console app that compiles src/plugin/PluginProcessor.cpp and PluginEditor.cpp, no host):
//
//   smr_plugin_tests selftest
//       Plugin against a reference that drives smr_host::BlockAdapter and smr_plugin::ParamApplier directly, the way
//       PluginProcessor.cpp does, but without JUCE parameters, without ScopedNoDenormals, without the state hand-off.
//       Differences therefore come from the JUCE layer, the denormal flush, mono handling, chunking or saved state.
//       Every check prints one line starting PASS:, FAIL: or INFO:. Exit code 0 only if no check failed.
//
//   smr_plugin_tests IN.wav OUT.wav [--pot 0=V] [--gain-db X] [--chan CH=BANK,SCALE,NOTE] [--trackoffset 0=V] [--script FILE]
//                                   [--tail SECONDS] [--block N]
//       Renders IN.wav through the plugin (host rate = file rate, stereo out) and writes a 32-bit float stereo WAV, left =
//       first output slot, right = second, latency trimmed. Same file layout as smr_render, so tools/analysis/port_passes.py
//       can call it with --render. Only the options above exist; --trackoffset 0=V is emulated through the Freq (odds)
//       parameter (the plugin has no trackoffset), see docs/open-questions.md M5-c. Script lines: "TIME rotup" only.
//
// Nothing here says anything about the hardware. See docs/open-questions.md M5-a to M5-e.
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>

#include "BlockAdapter.h"
#include "ParamMap.h"
#include "PluginProcessor.h"
#include "StateCodec.h"
#include "Tables.h"
#include "wav.h"

using namespace smr_plugin;

namespace
{
    int failures = 0, checks = 0;

    void line (const char* tag, const std::string& what, const std::string& detail = "")
    {
        std::printf ("%s: %s%s%s\n", tag, what.c_str(), detail.empty() ? "" : " - ", detail.c_str());
        std::fflush (stdout);
    }
    void check (bool ok, const std::string& what, const std::string& detail = "")
    {
        ++checks;
        if (! ok) ++failures;
        line (ok ? "PASS" : "FAIL", what, detail);
    }
    void info (const std::string& what, const std::string& detail = "") { line ("INFO", what, detail); }

    std::string fmt (const char* f, double a = 0, double b = 0, double c = 0, double d = 0)
    {
        char buf[400];
        std::snprintf (buf, sizeof buf, f, a, b, c, d);
        return buf;
    }

    // The plugin as the tests drive it. The using-declarations reach the protected XML helpers of juce::AudioProcessor.
    struct TestProc : SmrAudioProcessor
    {
        using juce::AudioProcessor::copyXmlToBinary;
        using juce::AudioProcessor::getXmlFromBinary;
    };

    void setLayout (TestProc& p, int nIn, int nOut)
    {
        juce::AudioProcessor::BusesLayout l;
        l.inputBuses.add (nIn == 1 ? juce::AudioChannelSet::mono() : juce::AudioChannelSet::stereo());
        l.outputBuses.add (nOut == 1 ? juce::AudioChannelSet::mono() : juce::AudioChannelSet::stereo());
        p.setBusesLayout (l);
    }

    // Adds an SMR_ENGINE block to the saved state, as a project file would hold it, and loads it (before prepareToPlay).
    void injectSnapshot (TestProc& p, const smr::Snapshot& s)
    {
        juce::MemoryBlock mb;
        p.getStateInformation (mb);
        auto xml = p.getXmlFromBinary (mb.getData(), (int) mb.getSize());
        juce::ValueTree tree = juce::ValueTree::fromXml (*xml);
        juce::ValueTree old = tree.getChildWithName ("SMR_ENGINE");
        if (old.isValid()) tree.removeChild (old, nullptr);
        juce::ValueTree eng ("SMR_ENGINE");
        eng.setProperty ("version", kStateVersion, nullptr);
        eng.setProperty ("data", juce::String (encodeSnapshot (s)), nullptr);
        tree.appendChild (eng, nullptr);
        auto out = tree.createXml();
        juce::MemoryBlock mb2;
        p.copyXmlToBinary (*out, mb2);
        p.setStateInformation (mb2.getData(), (int) mb2.getSize());
    }

    smr::Snapshot defaultSnapshot()
    {
        float v[kNumParams];
        for (int i = 0; i < kNumParams; ++i) v[i] = paramDefault (i);
        std::unique_ptr<smr::Engine> e (new smr::Engine());
        e->setControls (ParamApplier::buildControls (v));
        e->reset();
        return e->saveSnapshot();
    }

    // Per-channel state identified from capture-1 (tools/analysis/port_passes.py BASE_CHANS, docs/open-questions.md M3-f).
    smr::Snapshot capture1Snapshot()
    {
        static const int chans[6][3] = { { 6, 2, 6 }, { 6, 10, 7 }, { 6, 2, 8 }, { 6, 2, 9 }, { 6, 2, 10 }, { 6, 2, 11 } };
        smr::Snapshot s = defaultSnapshot();
        for (int i = 0; i < 6; ++i)
        {
            s.scale_bank[i] = (uint8_t) chans[i][0];
            s.scale[i] = (uint8_t) chans[i][1];
            s.note[i] = (uint8_t) chans[i][2];
        }
        return s;
    }

    // The reference: what PluginProcessor does, minus JUCE.
    struct Ref
    {
        smr_host::BlockAdapter ad;
        ParamApplier ap;
        float v[kNumParams];
        bool ok = false;

        Ref() { for (int i = 0; i < kNumParams; ++i) v[i] = paramDefault (i); }

        bool start (int rate, int maxBlock, const smr::Snapshot* snap)
        {
            ok = ad.prepare (rate, maxBlock);
            if (! ok) return false;
            smr::Engine& e = ad.engine();
            e.setControls (ParamApplier::buildControls (v));
            ad.reset();
            if (snap) e.loadSnapshot (*snap);
            ap.apply (e, v, true);
            return true;
        }
    };

    struct Noise
    {
        uint32_t s;
        explicit Noise (uint32_t seed) : s (seed) {}
        float next()   // uniform in [-1, 1)
        {
            s = s * 1664525u + 1013904223u;
            return (float) ((int32_t) s) * (1.0f / 2147483648.0f);
        }
    };

    struct Diff { size_t n = 0, nDiff = 0, first = 0; double maxAbs = 0.0; double peak = 0.0; bool finite = true; };

    Diff compare (const std::vector<float>& a, const std::vector<float>& b)
    {
        Diff d;
        d.n = std::min (a.size(), b.size());
        for (size_t i = 0; i < d.n; ++i)
        {
            if (! std::isfinite (a[i]) || ! std::isfinite (b[i])) d.finite = false;
            d.peak = std::max (d.peak, (double) std::fabs (a[i]));
            uint32_t x, y;
            std::memcpy (&x, &a[i], 4);
            std::memcpy (&y, &b[i], 4);
            if (x != y)
            {
                if (d.nDiff == 0) d.first = i;
                ++d.nDiff;
                d.maxAbs = std::max (d.maxAbs, (double) std::fabs (a[i] - b[i]));
            }
        }
        if (a.size() != b.size()) d.nDiff += (size_t) std::llabs ((long long) a.size() - (long long) b.size());
        return d;
    }

    std::string describe (const Diff& d)
    {
        if (d.nDiff == 0) return fmt ("%.0f samples identical, peak %.4f", (double) d.n, d.peak);
        return fmt ("%.0f of %.0f samples differ, first at %.0f, largest difference %.3g", (double) d.nDiff, (double) d.n, (double) d.first, d.maxAbs);
    }

    // One plugin instance and one reference, driven with the same input and the same parameter changes.
    struct Rig
    {
        std::unique_ptr<TestProc> proc { new TestProc() };
        Ref ref;
        int rate = 48000, block = 512, nIn = 2, nOut = 2;
        bool haveSnap = false;
        smr::Snapshot snap;
        std::vector<float> plugOut[2], refOut[2];
        double plugSeconds = 0.0, refSeconds = 0.0;

        void setSnapshot (const smr::Snapshot& s) { haveSnap = true; snap = s; injectSnapshot (*proc, s); }

        // Sets the plugin parameter through the host interface, then copies the value the plugin really holds into the reference.
        void set (const char* id, float real)
        {
            auto* p = proc->parameters().getParameter (id);
            p->setValueNotifyingHost (p->convertTo0to1 (real));
            const int i = indexOfId (id);
            const float held = proc->parameters().getRawParameterValue (id)->load();
            ref.v[i] = held;
            const float tol = 1e-5f * std::max (1.0f, std::fabs (real));
            if (std::fabs (held - real) > tol) check (false, std::string ("parameter ") + id + " holds what was set", fmt ("set %.9g, held %.9g", real, held));
        }

        bool prepare()
        {
            setLayout (*proc, nIn, nOut);
            proc->setRateAndBufferSizeDetails ((double) rate, block);
            proc->prepareToPlay ((double) rate, block);
            return ref.start (rate, block, haveSnap ? &snap : nullptr);
        }

        template <class Hook>
        void run (size_t frames, size_t noiseFrames, float amp, Hook hook)
        {
            Noise nl (12345), nr (67890);
            juce::AudioBuffer<float> buf (2, block);
            juce::MidiBuffer midi;
            std::vector<float> inL ((size_t) block), inR ((size_t) block), oL ((size_t) block), oR ((size_t) block);
            plugOut[0].clear(); plugOut[1].clear(); refOut[0].clear(); refOut[1].clear();
            int blockIndex = 0;
            for (size_t pos = 0; pos < frames; pos += (size_t) block, ++blockIndex)
            {
                const int n = (int) std::min ((size_t) block, frames - pos);
                hook (*this, blockIndex);
                for (int k = 0; k < n; ++k)
                {
                    const bool on = pos + (size_t) k < noiseFrames;
                    inL[(size_t) k] = on ? amp * nl.next() : 0.0f;
                    inR[(size_t) k] = on ? amp * nr.next() : 0.0f;
                }
                buf.clear();
                std::memcpy (buf.getWritePointer (0), inL.data(), (size_t) n * 4);
                if (nIn == 2) std::memcpy (buf.getWritePointer (1), inR.data(), (size_t) n * 4);
                else if (nOut == 2) for (int k = 0; k < n; ++k) buf.setSample (1, k, 0.12345f);   // an unconnected input must be ignored

                const auto t0 = std::chrono::steady_clock::now();
                proc->processBlock (buf, midi);
                const auto t1 = std::chrono::steady_clock::now();
                plugSeconds += std::chrono::duration<double> (t1 - t0).count();

                ref.ap.apply (ref.ad.engine(), ref.v);
                ref.ad.process (inL.data(), nIn == 2 ? inR.data() : nullptr, oL.data(), oR.data(), n);
                const auto t2 = std::chrono::steady_clock::now();
                refSeconds += std::chrono::duration<double> (t2 - t1).count();

                if (nOut == 1)
                {
                    for (int k = 0; k < n; ++k) oL[(size_t) k] += oR[(size_t) k];   // the plugin sums the two output slots
                    plugOut[0].insert (plugOut[0].end(), buf.getReadPointer (0), buf.getReadPointer (0) + n);
                    refOut[0].insert (refOut[0].end(), oL.begin(), oL.begin() + n);
                }
                else
                {
                    for (int c = 0; c < 2; ++c) plugOut[c].insert (plugOut[c].end(), buf.getReadPointer (c), buf.getReadPointer (c) + n);
                    refOut[0].insert (refOut[0].end(), oL.begin(), oL.begin() + n);
                    refOut[1].insert (refOut[1].end(), oR.begin(), oR.begin() + n);
                }
            }
        }
    };

    void report (const std::string& name, const Rig& r)
    {
        bool all = true;
        std::string detail;
        const int nch = r.nOut;
        for (int c = 0; c < nch; ++c)
        {
            const Diff d = compare (r.plugOut[c], r.refOut[c]);
            all = all && d.nDiff == 0 && d.finite && d.peak > 1e-4;
            detail += (c ? "; ch" : "ch") + std::to_string (c) + ": " + describe (d) + (d.finite ? "" : " NOT FINITE") + (d.peak > 1e-4 ? "" : " SILENT");
        }
        check (all, name + ": plugin equals reference bit for bit", detail);
    }

    void noHook (Rig&, int) {}

    // ------------------------------------------------------------------------------------------------------------
    void testLayouts()
    {
        TestProc p;
        auto supported = [&p] (int i, int o)
        {
            juce::AudioProcessor::BusesLayout l;
            l.inputBuses.add (i == 1 ? juce::AudioChannelSet::mono() : juce::AudioChannelSet::stereo());
            l.outputBuses.add (o == 1 ? juce::AudioChannelSet::mono() : juce::AudioChannelSet::stereo());
            return p.checkBusesLayoutSupported (l);
        };
        check (supported (2, 2), "layout stereo in, stereo out is supported");
        check (supported (1, 1), "layout mono in, mono out is supported");
        check (supported (1, 2), "layout mono in, stereo out is supported");
        check (! supported (2, 1), "layout stereo in, mono out is refused");
    }

    void testRates()
    {
        static const int rates[] = { 22050, 32000, 44100, 48000, 88200, 96000, 176400, 192000 };
        for (int rate : rates)
        {
            TestProc p;
            p.setRateAndBufferSizeDetails ((double) rate, 512);
            p.prepareToPlay ((double) rate, 512);
            const auto s = p.status();
            check (s.rateSupported, fmt ("host rate %.0f Hz is supported", rate));

            // Onset of the response to a single impulse, next to the latency the plugin reports. An indicator only: a
            // resonator's response does not have a sharp first sample.
            juce::AudioBuffer<float> buf (2, 4096);
            juce::MidiBuffer midi;
            buf.clear();
            buf.setSample (0, 0, 0.03f);
            buf.setSample (1, 0, 0.03f);
            p.processBlock (buf, midi);
            int onset = -1;
            for (int k = 0; k < 4096 && onset < 0; ++k)
                if (std::fabs (buf.getSample (0, k)) > 1e-9f || std::fabs (buf.getSample (1, k)) > 1e-9f) onset = k;
            info (fmt ("rate %.0f Hz: reported latency %.0f samples", rate, p.getLatencySamples()),
                  fmt ("first output sample above 1e-9 at index %.0f (impulse at 0)", onset));
        }

        // A rate whose host/core ratio does not reduce to small integers (BlockAdapter::prepare, ratioToCore limit 30000) passes the
        // audio through unchanged. 44101 Hz is such a rate (671875/(7*44101) does not reduce). 50000 Hz is not: it is 215/112.
        {
            TestProc p50;
            p50.setRateAndBufferSizeDetails (50000.0, 256);
            p50.prepareToPlay (50000.0, 256);
            check (p50.status().rateSupported, "host rate 50000 Hz is supported (ratio 215/112)", fmt ("latency %.0f samples", (double) p50.getLatencySamples()));
        }
        TestProc p;
        p.setRateAndBufferSizeDetails (44101.0, 256);
        p.prepareToPlay (44101.0, 256);
        juce::AudioBuffer<float> buf (2, 256), orig (2, 256);
        Noise n (5);
        for (int c = 0; c < 2; ++c) for (int k = 0; k < 256; ++k) orig.setSample (c, k, 0.1f * n.next());
        buf.makeCopyOf (orig);
        juce::MidiBuffer midi;
        p.processBlock (buf, midi);
        bool same = true;
        for (int c = 0; c < 2; ++c) for (int k = 0; k < 256; ++k) same = same && buf.getSample (c, k) == orig.getSample (c, k);
        check (! p.status().rateSupported && same && p.getLatencySamples() == 0, "unsupported host rate 44101 Hz: audio unchanged, latency 0",
               fmt ("rateSupported %.0f, unchanged %.0f, latency %.0f", (double) p.status().rateSupported, (double) same, (double) p.getLatencySamples()));
    }

    void testDefaults()
    {
        const int rates[] = { 44100, 48000, 96000 };
        for (int rate : rates)
        {
            Rig r;
            r.rate = rate;
            r.block = 512;
            if (! r.prepare()) { check (false, "prepare", "rate refused"); continue; }
            r.run ((size_t) rate * 8, (size_t) rate * 2, 0.03f, noHook);
            report (fmt ("default settings, %.0f Hz, block 512, stereo", rate), r);
        }

        // Block-size independence of the plugin itself (same settings, same input, different host buffer sizes).
        std::vector<float> first[2];
        bool equal = true;
        std::string detail;
        const int blocks[] = { 64, 333, 1000 };
        for (int bi = 0; bi < 3; ++bi)
        {
            Rig r;
            r.rate = 48000;
            r.block = blocks[bi];
            r.prepare();
            r.run (48000 * 4, 48000 * 2, 0.03f, noHook);
            if (bi == 0) { first[0] = r.plugOut[0]; first[1] = r.plugOut[1]; }
            else
            {
                for (int c = 0; c < 2; ++c)
                {
                    const Diff d = compare (first[c], r.plugOut[c]);
                    equal = equal && d.nDiff == 0;
                    detail += fmt ("block %.0f ch%.0f: ", blocks[bi], c) + describe (d) + "; ";
                }
            }
            report (fmt ("default settings, 48000 Hz, block %.0f", blocks[bi]), r);
        }
        check (equal, "plugin output does not depend on the host buffer size (64, 333, 1000)", detail);
    }

    void testQ()
    {
        const float qs[] = { 0.0f, 0.5f, 1.0f };
        for (float q : qs)
        {
            Rig r;
            r.prepare();
            r.set ("q", q);
            r.run (48000 * 6, 48000 * 2, 0.03f, noHook);
            report (fmt ("Q knob %.2f, 48000 Hz", q), r);
        }
    }

    void testParameters()
    {
        Rig r;
        r.block = 333;
        r.prepare();
        auto hook = [] (Rig& rig, int b)
        {
            switch (b)
            {
                case 5:   rig.set ("level2", 0.8f); rig.set ("level3", 0.6f); rig.set ("level4", 0.4f); rig.set ("level5", 0.2f); break;
                case 20:  rig.set ("spread", 0.5f); rig.set ("morph", 0.3f); break;
                case 40:  rig.set ("bank", 5.0f); break;
                case 60:  rig.set ("lock2", 1.0f); break;
                case 70:  rig.set ("rotatecw", 1.0f); break;
                case 71:  rig.set ("rotatecw", 0.0f); break;
                case 80:  rig.set ("scaleup", 1.0f); break;
                case 81:  rig.set ("scaleup", 0.0f); break;
                case 90:  rig.set ("nudgeodds", 0.5f); rig.set ("freqevens", 0.25f); break;
                case 100: rig.set ("slew", 1.0f); rig.set ("oddsmode", 1.0f); break;
                case 110: rig.set ("q", 0.9f); rig.set ("rotateccw", 1.0f); break;
                case 111: rig.set ("rotateccw", 0.0f); break;
                case 130: rig.set ("filtermode", 1.0f); break;
                case 150: rig.set ("filtertype", 1.0f); break;
                case 170: rig.set ("lock2", 0.0f); rig.set ("bank", 12.0f); break;
                case 200: rig.set ("levelcv1", 0.5f); rig.set ("qcv", 0.3f); break;
                default: break;
            }
        };
        r.run (48000 * 6, 48000 * 3, 0.03f, hook);
        report ("scripted parameter changes (levels, spread, morph, bank, lock, rotate, scale, nudge, Freq, slew, switches, Q, filter mode and type), block 333", r);
    }

    void testLayoutAudio()
    {
        {
            Rig r;
            r.nIn = 1; r.nOut = 1;
            r.prepare();
            r.run (48000 * 5, 48000 * 2, 0.03f, noHook);
            report ("mono in, mono out (output slots summed)", r);
        }
        std::vector<float> monoStereo[2], stereoSame[2];
        {
            Rig r;
            r.nIn = 1; r.nOut = 2;
            r.prepare();
            r.run (48000 * 5, 48000 * 2, 0.03f, noHook);
            report ("mono in, stereo out (the unused second input holds 0.12345 and must be ignored)", r);
            monoStereo[0] = r.plugOut[0]; monoStereo[1] = r.plugOut[1];
        }
        {
            // Stereo in with identical left and right must equal mono in. Needs R = L, so drive it by hand.
            TestProc p;
            p.setRateAndBufferSizeDetails (48000.0, 512);
            p.prepareToPlay (48000.0, 512);
            juce::AudioBuffer<float> buf (2, 512);
            juce::MidiBuffer midi;
            Noise nl (12345);
            for (size_t pos = 0; pos < (size_t) 48000 * 5; pos += 512)
            {
                for (int k = 0; k < 512; ++k)
                {
                    const float x = pos + (size_t) k < (size_t) 48000 * 2 ? 0.03f * nl.next() : 0.0f;
                    buf.setSample (0, k, x);
                    buf.setSample (1, k, x);
                }
                p.processBlock (buf, midi);
                for (int c = 0; c < 2; ++c)
                    stereoSame[c].insert (stereoSame[c].end(), buf.getReadPointer (c), buf.getReadPointer (c) + 512);
            }
            for (int c = 0; c < 2; ++c)
            {
                stereoSame[c].resize (monoStereo[c].size());
            }
            const Diff d0 = compare (stereoSame[0], monoStereo[0]);
            const Diff d1 = compare (stereoSame[1], monoStereo[1]);
            check (d0.nDiff == 0 && d1.nDiff == 0, "stereo in with L = R equals mono in", describe (d0) + " / " + describe (d1));
        }
    }

    void testSavedState()
    {
        const smr::Snapshot s = capture1Snapshot();

        // A: the hardware's identified state loaded as a project would load it.
        Rig a;
        a.setSnapshot (s);
        a.prepare();
        a.set ("q", 1876.0f / 4095.0f);
        a.run (48000 * 6, 48000 * 2, 0.03f, noHook);
        report ("capture-1 state through saved state (SMR_ENGINE), Q count 1876", a);

        // B: a second instance given the state of a fresh instance that holds the same snapshot, before any audio.
        TestProc src;
        injectSnapshot (src, s);
        juce::MemoryBlock mb;
        src.getStateInformation (mb);
        Rig b;
        b.proc->setStateInformation (mb.getData(), (int) mb.getSize());
        b.haveSnap = true;
        b.snap = s;
        b.prepare();
        b.set ("q", 1876.0f / 4095.0f);
        b.run (48000 * 6, 48000 * 2, 0.03f, noHook);
        report ("same state copied between instances (getStateInformation, setStateInformation)", b);

        // C: bank and scale state survives a save and load in the middle of a run: the new instance starts from the saved
        // snapshot, so it matches a reference loaded with that snapshot (not the continuing run).
        Rig c;
        c.prepare();
        c.run (48000, 0, 0.03f, noHook);
        juce::MemoryBlock saved;
        c.proc->getStateInformation (saved);
        info ("saved state size after one second of audio", fmt ("%.0f bytes", (double) saved.getSize()));
    }

    void testOddsOffset()
    {
        // The plugin has no trackoffset. The Freq (odds) parameter moves the jack ADC count instead. trackoffset 15 in the
        // port's comparisons (M3-g) is ADC count 15 here. Both enter freq_jack_cv the same way (params.c param_read_freq).
        const float oct = std::log2 (smr::tables().exp_1voct[4]);
        const uint16_t counts = octavesToCounts (oct);
        check (counts == 15, "Freq (odds) value for table index 4 maps to ADC count 15", fmt ("%.0f counts, %.6f octaves", (double) counts, (double) oct));

        const smr::Snapshot s = capture1Snapshot();
        Rig r;
        r.setSnapshot (s);
        r.prepare();
        r.set ("freqodds", oct);
        r.run (48000 * 6, 48000 * 2, 0.03f, noHook);
        report ("Freq (odds) parameter at ADC count 15", r);

        // Against the engine with trackoffset[0] = 15 and the Freq jack at 0, which is what the capture-1 comparisons used. All four
        // cases run through the same reference driver, so any difference is the Freq jack path and nothing else.
        auto renderRef = [&s] (int trackoffset, float freqOct, std::vector<float>& oddsOut)
        {
            Ref ref;
            ref.v[indexOfId ("freqodds")] = freqOct;
            ref.start (48000, 512, &s);
            ref.ad.engine().trackoffset[0] = (int16_t) trackoffset;
            Noise nl (12345), nr (67890);
            std::vector<float> inL (512), inR (512), oL (512), oR (512);
            oddsOut.clear();
            for (size_t pos = 0; pos < (size_t) 48000 * 6; pos += 512)
            {
                const int n = (int) std::min ((size_t) 512, (size_t) 48000 * 6 - pos);
                for (int k = 0; k < n; ++k)
                {
                    const bool on = pos + (size_t) k < (size_t) 48000 * 2;
                    inL[(size_t) k] = on ? 0.03f * nl.next() : 0.0f;
                    inR[(size_t) k] = on ? 0.03f * nr.next() : 0.0f;
                }
                ref.ap.apply (ref.ad.engine(), ref.v);
                ref.ad.process (inL.data(), inR.data(), oL.data(), oR.data(), n);
                oddsOut.insert (oddsOut.end(), oR.begin(), oR.begin() + n);
            }
        };
        auto relDb = [] (const std::vector<float>& x, const std::vector<float>& y)
        {
            double num = 0.0, den = 0.0;
            for (size_t i = 0; i < std::min (x.size(), y.size()); ++i) { const double d = (double) x[i] - (double) y[i]; num += d * d; den += (double) y[i] * (double) y[i]; }
            return den > 0.0 ? 10.0 * std::log10 (std::max (num, 1e-300) / den) : 0.0;
        };
        std::vector<float> t15, adc15, base;
        renderRef (15, 0.0f, t15);
        renderRef (0, oct, adc15);
        renderRef (0, 0.0f, base);
        info ("core driver, odds output: Freq ADC 15 against trackoffset 15", fmt ("%.1f dB relative to the signal (about -300 or lower means identical)", relDb (adc15, t15)));
        info ("core driver, odds output: Freq ADC 15 against no offset", fmt ("%.1f dB relative to the signal", relDb (adc15, base)));
        info ("plugin, odds output: Freq ADC 15 against the core driver with Freq ADC 15", fmt ("%.1f dB relative to the signal", relDb (r.plugOut[1], adc15)));
        info ("plugin, odds output: Freq ADC 15 against the core driver with trackoffset 15", fmt ("%.1f dB relative to the signal", relDb (r.plugOut[1], t15)));
    }

    void testDenormals()
    {
        // Long tail at Q max: do the filter states reach denormal numbers, and does the flush the plugin applies change the sound
        // or the CPU time? 48000 Hz, one second of noise, then 99 s of silence.
        Rig r;
        r.block = 512;
        r.prepare();
        r.set ("q", 1.0f);
        r.run ((size_t) 48000 * 100, 48000, 0.03f, noHook);
        report ("100 s run at Q max (denormal flush on in the plugin, off in the reference)", r);

        const Diff d = compare (r.plugOut[0], r.refOut[0]);
        const Diff d1 = compare (r.plugOut[1], r.refOut[1]);
        info ("denormal flush: differing samples", fmt ("ch0 %.0f, ch1 %.0f of %.0f", (double) d.nDiff, (double) d1.nDiff, (double) d.n));

        // Tail level at the end, to show how far the resonators have decayed.
        double tailPeak = 0.0;
        const size_t from = r.plugOut[0].size() - 48000;
        for (size_t i = from; i < r.plugOut[0].size(); ++i) tailPeak = std::max (tailPeak, (double) std::fabs (r.plugOut[0][i]));
        info ("peak of the last second of the plugin output", fmt ("%.3g (%.1f dBFS)", tailPeak, tailPeak > 0.0 ? 20.0 * std::log10 (tailPeak) : -999.0));
        info ("processing time for 100 s of audio (CI runner, not a CPU measurement of the plugin)",
              fmt ("plugin %.2f s, reference %.2f s", r.plugSeconds, r.refSeconds));
    }

    // ------------------------------------------------------------------------------------------------------------
    int selftest()
    {
        testLayouts();
        testRates();
        testDefaults();
        testQ();
        testParameters();
        testLayoutAudio();
        testSavedState();
        testOddsOffset();
        testDenormals();
        std::printf ("%s: %d checks, %d failed\n", failures ? "FAIL" : "PASS", checks, failures);
        return failures ? 1 : 0;
    }

    // ------------------------------------------------------------------------------------------------------------
    int render (int argc, char** argv)
    {
        const char* inPath = argv[1];
        const char* outPath = argv[2];
        double gainDb = 0.0, tail = 2.0;
        int block = 512, q = 2048, trackoffset = 0;
        bool haveQ = false;
        struct Ch { bool set = false; int bank = 0, scale = 0, note = 0; } ch[6];
        std::string script;

        for (int i = 3; i < argc; ++i)
        {
            const std::string a = argv[i];
            auto next = [&] () -> std::string { if (i + 1 >= argc) { std::fprintf (stderr, "error: %s needs a value\n", a.c_str()); std::exit (2); } return argv[++i]; };
            if (a == "--pot")
            {
                int k = -1, v = -1;
                if (std::sscanf (next().c_str(), "%d=%d", &k, &v) != 2 || k != 0 || v < 0 || v > 4095) { std::fputs ("error: only --pot 0=V (Q) with V 0..4095\n", stderr); return 2; }
                q = v; haveQ = true;
            }
            else if (a == "--gain-db") gainDb = std::atof (next().c_str());
            else if (a == "--tail") tail = std::atof (next().c_str());
            else if (a == "--block") block = std::atoi (next().c_str());
            else if (a == "--script") script = next();
            else if (a == "--trackoffset")
            {
                int k = -1, v = 0;
                if (std::sscanf (next().c_str(), "%d=%d", &k, &v) != 2 || k != 0 || v < 11 || v > 4000) { std::fputs ("error: only --trackoffset 0=V with V 11..4000 (emulated by the Freq odds parameter)\n", stderr); return 2; }
                trackoffset = v;
            }
            else if (a == "--chan")
            {
                int c, b, s, n;
                if (std::sscanf (next().c_str(), "%d=%d,%d,%d", &c, &b, &s, &n) != 4 || c < 1 || c > 6) { std::fputs ("error: --chan CH=BANK,SCALE,NOTE\n", stderr); return 2; }
                ch[c - 1] = { true, b, s, n };
            }
            else { std::fprintf (stderr, "error: unknown option %s\n", a.c_str()); return 2; }
        }
        (void) haveQ;

        wav::Audio in;
        std::string err;
        if (! wav::read (inPath, in, err)) { std::fprintf (stderr, "error: %s: %s\n", inPath, err.c_str()); return 1; }

        std::vector<double> evTimes;
        if (! script.empty())
        {
            FILE* f = std::fopen (script.c_str(), "r");
            if (! f) { std::fputs ("error: cannot open script\n", stderr); return 1; }
            char ln[256];
            while (std::fgets (ln, sizeof ln, f))
            {
                char* h = std::strchr (ln, '#'); if (h) *h = 0;
                double t; char kind[32] = {};
                if (std::sscanf (ln, "%lf %31s", &t, kind) == 2)
                {
                    if (std::strcmp (kind, "rotup") != 0) { std::fputs ("error: the plugin renderer knows only the script command rotup\n", stderr); return 1; }
                    evTimes.push_back (t);
                }
            }
            std::fclose (f);
        }

        bool anyCh = false;
        for (auto& c : ch) anyCh = anyCh || c.set;
        auto proc = std::unique_ptr<TestProc> (new TestProc());
        setLayout (*proc, 1, 2);
        if (anyCh)
        {
            smr::Snapshot s = defaultSnapshot();
            for (int i = 0; i < 6; ++i)
                if (ch[i].set) { s.scale_bank[i] = (uint8_t) ch[i].bank; s.scale[i] = (uint8_t) ch[i].scale; s.note[i] = (uint8_t) ch[i].note; }
            injectSnapshot (*proc, s);
        }
        auto setp = [&proc] (const char* id, float real)
        {
            auto* pp = proc->parameters().getParameter (id);
            pp->setValueNotifyingHost (pp->convertTo0to1 (real));
        };
        setp ("q", (float) q / 4095.0f);
        if (trackoffset)
            setp ("freqodds", std::log2 (smr::tables().exp_1voct[trackoffset - 11]));
        proc->setRateAndBufferSizeDetails ((double) in.rate, block);
        proc->prepareToPlay ((double) in.rate, block);
        if (! proc->status().rateSupported) { std::fprintf (stderr, "error: sample rate %d Hz is not supported by the plugin\n", in.rate); return 1; }

        const size_t inFrames = in.frames();
        const size_t latency = (size_t) proc->getLatencySamples();
        const size_t total = inFrames + (size_t) std::llround (tail * in.rate) + latency;
        const float inGain = (float) std::pow (10.0, gainDb / 20.0);

        juce::AudioBuffer<float> buf (2, block);
        juce::MidiBuffer midi;
        wav::Audio out;
        out.rate = in.rate;
        out.channels = 2;
        size_t evIdx = 0;
        bool pulseOn = false;
        float peak = 0.0f;
        for (size_t pos = 0; pos < total; pos += (size_t) block)
        {
            const int n = (int) std::min ((size_t) block, total - pos);
            const double t = (double) pos / (double) in.rate;
            const bool fire = evIdx < evTimes.size() && evTimes[evIdx] <= t;
            if (fire) { setp ("rotatecw", 1.0f); ++evIdx; pulseOn = true; }
            else if (pulseOn) { setp ("rotatecw", 0.0f); pulseOn = false; }

            buf.clear();
            for (int k = 0; k < n; ++k)
            {
                const size_t fr = pos + (size_t) k;
                if (fr < inFrames) buf.setSample (0, k, in.data[fr * (size_t) in.channels] * inGain);
            }
            proc->processBlock (buf, midi);
            for (int k = 0; k < n; ++k)
                if (pos + (size_t) k >= latency)
                {
                    out.data.push_back (buf.getSample (0, k));
                    out.data.push_back (buf.getSample (1, k));
                    peak = std::max (peak, std::max (std::fabs (buf.getSample (0, k)), std::fabs (buf.getSample (1, k))));
                }
        }
        if (! wav::writeFloat (outPath, out)) { std::fprintf (stderr, "error: cannot write %s\n", outPath); return 1; }
        const auto st = proc->status();
        std::printf ("smr_plugin_render: %zu frames in, %zu frames out at %d Hz\n", inFrames, out.frames(), in.rate);
        std::printf ("  host block %d, trimmed latency %zu frames, underruns %llu\n", block, latency, (unsigned long long) st.underruns);
        std::printf ("  output peak %.6f (%.2f dBFS); plugin path: JUCE parameters, saved state, resampler, denormal flush\n", (double) peak, peak > 0.0f ? 20.0 * std::log10 ((double) peak) : -999.0);
        std::printf ("  audio saturation events n/a (not visible through the plugin interface), input gain %.2f dB, Q count %d, odds ADC count %d\n", gainDb, q, trackoffset);
        return 0;
    }
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    if (argc >= 2 && std::strcmp (argv[1], "selftest") == 0) return selftest();
    if (argc >= 3 && std::strstr (argv[1], ".wav") != nullptr) return render (argc, argv);
    std::fputs ("usage: smr_plugin_tests selftest\n       smr_plugin_tests IN.wav OUT.wav [--pot 0=V] [--gain-db X] [--chan CH=B,S,N] [--trackoffset 0=V] [--script FILE] [--tail S] [--block N]\n", stderr);
    return 2;
}
