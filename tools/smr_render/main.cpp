// smr_render: offline renderer for the Unofficial SMR core.  WAV in, WAV out, controls as ADC counts.
// Uses src/host (resampler and block adapter) and src/core. No plugin code. Output is a 32-bit float stereo WAV
// at the input's sample rate. Run "smr_render --help" for the options.
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "BlockAdapter.h"
#include "wav.h"

namespace
{
    const char* kHelp =
        "smr_render IN.wav OUT.wav [options]\n"
        "\n"
        "Plays IN.wav through the Unofficial SMR core (firmware v5.1.1, commit 79bc953, no enhancements) and writes OUT.wav\n"
        "(32-bit float, stereo, same sample rate as IN.wav). Mono input feeds both input slots; stereo input feeds left to the\n"
        "first slot and right to the second. Output left is the first output slot (filtered_buffer, channels 2/4/6) and output\n"
        "right is the second (filtered_bufferR, channels 1/3/5): the source mapping, provisional (open question M1-m).\n"
        "Level scale is a placeholder: 1.0 = full scale (open question M1-n).\n"
        "\n"
        "Controls are raw 12-bit ADC counts (0..4095), as the firmware reads them.\n"
        "  --adc N=V      N in 0..14   0 Morph, 1 Q CV, 2..7 Level CV 1..6, 8 Freq Nudge 1/3/5, 9 Freq Nudge 2/4/6,\n"
        "                              10 Scale CV, 11 Spread, 12 Rotate CV, 13 Freq CV 1/3/5, 14 Freq CV 2/4/6\n"
        "  --pot N=V      N in 0..6    0 Q pot, 1..6 channel sliders\n"
        "  --sw NAME=0|1  env_pre (1 = PRE, 0 = POST), mod246_six, mod135_one, range, env_fast, env_slow, rotary_sw, cvlag\n"
        "  --mode twopass|onepass      --type maxq|bpre      --bank 0..19 (all channels)\n"
        "  --freqblock MASK            blocked frequency positions, bit n = position n (at most 14 bits, below bit 20)\n"
        "  --script FILE               timed control changes, one per line: TIME_SECONDS adc N V | pot N V | sw NAME V |\n"
        "                              rotup | rotdown | lock135 | lock246 | bank B | mode M | type T   (# starts a comment)\n"
        "  --block N                   host block size in frames (default 64)\n"
        "  --tail SECONDS              extra time rendered after the input ends (default 2)\n"
        "  --no-compensate             keep the start-up latency instead of trimming it\n"
        "\n"
        "Defaults if not given: all ADC counts 0 except Level CV 1..6 = 4095; pots 0 except sliders 1..6 = 4095 and Q pot = 2048;\n"
        "PRE/POST = POST, all other switches off; Two-Pass, MAXQ, bank 0; no blocked frequencies.\n"
        "Supported file sample rates (exact ratios only): 22050 32000 44100 48000 88200 96000 176400 192000.\n";

    struct Cfg
    {
        smr::Controls ctl;
        int mode = 0, type = 0, bank = -1;   // 0 keep default
        uint32_t freqblock = 0;
        int block = 64;
        double tail = 2.0;
        bool compensate = true;
        std::string script;
    };

    bool setSwitch (smr::Controls& c, const std::string& name, bool v)
    {
        if (name == "env_pre") c.env_mode_pre = v;
        else if (name == "mod246_six") c.mod246_six = v;
        else if (name == "mod135_one") c.mod135_one = v;
        else if (name == "range") c.range_mode = v;
        else if (name == "env_fast") c.env_speed_fast = v;
        else if (name == "env_slow") c.env_speed_slow = v;
        else if (name == "rotary_sw") c.rotary_sw = v;
        else if (name == "cvlag") c.cvlag = v;
        else return false;
        return true;
    }

    bool splitKV (const std::string& s, int& k, int& v)
    {
        const size_t eq = s.find ('=');
        if (eq == std::string::npos) return false;
        char* e1 = nullptr;
        char* e2 = nullptr;
        k = (int) std::strtol (s.substr (0, eq).c_str(), &e1, 10);
        v = (int) std::strtol (s.substr (eq + 1).c_str(), &e2, 10);
        return *e1 == 0 && *e2 == 0;
    }

    struct Event
    {
        double t;
        std::string kind;   // adc pot sw rotup rotdown lock135 lock246 bank mode type
        int a = 0, b = 0;
        std::string name;
    };

    bool loadScript (const std::string& path, std::vector<Event>& ev, std::string& err)
    {
        FILE* f = std::fopen (path.c_str(), "r");
        if (! f) { err = "cannot open script"; return false; }
        char line[512];
        int ln = 0;
        while (std::fgets (line, sizeof line, f))
        {
            ++ln;
            char* hash = std::strchr (line, '#');
            if (hash) *hash = 0;
            char t[64] = {}, kind[32] = {}, w1[32] = {}, w2[32] = {};
            const int n = std::sscanf (line, "%63s %31s %31s %31s", t, kind, w1, w2);
            if (n < 2) continue;
            Event e;
            e.t = std::atof (t);
            e.kind = kind;
            if (e.kind == "adc" || e.kind == "pot")
            {
                if (n < 4) { err = "line " + std::to_string (ln) + ": adc/pot needs N and V"; std::fclose (f); return false; }
                e.a = std::atoi (w1);
                e.b = std::atoi (w2);
            }
            else if (e.kind == "sw")
            {
                if (n < 4) { err = "line " + std::to_string (ln) + ": sw needs NAME and V"; std::fclose (f); return false; }
                e.name = w1;
                e.b = std::atoi (w2);
            }
            else if (e.kind == "bank") e.a = std::atoi (w1);
            else if (e.kind == "mode" || e.kind == "type") e.name = w1;
            else if (e.kind != "rotup" && e.kind != "rotdown" && e.kind != "lock135" && e.kind != "lock246")
            { err = "line " + std::to_string (ln) + ": unknown event '" + e.kind + "'"; std::fclose (f); return false; }
            ev.push_back (e);
        }
        std::fclose (f);
        std::stable_sort (ev.begin(), ev.end(), [] (const Event& x, const Event& y) { return x.t < y.t; });
        return true;
    }

    void applyEvent (smr::Engine& e, smr::Controls& ctl, const Event& ev)
    {
        if (ev.kind == "adc" && ev.a >= 0 && ev.a < smr::kNumAdcs) ctl.adc[ev.a] = (uint16_t) std::min (std::max (ev.b, 0), 4095);
        else if (ev.kind == "pot" && ev.a >= 0 && ev.a < smr::kNumAdc3s) ctl.pot[ev.a] = (uint16_t) std::min (std::max (ev.b, 0), 4095);
        else if (ev.kind == "sw") setSwitch (ctl, ev.name, ev.b != 0);
        else if (ev.kind == "rotup") e.triggerRotateUp();
        else if (ev.kind == "rotdown") e.triggerRotateDown();
        else if (ev.kind == "lock135") e.triggerLock135();
        else if (ev.kind == "lock246") e.triggerLock246();
        else if (ev.kind == "bank" && ev.a >= 0 && ev.a < smr::kNumScaleBanks) for (int i = 0; i < smr::kNumChannels; ++i) e.scale_bank[i] = (uint8_t) ev.a;
        else if (ev.kind == "mode") e.setFilterMode (ev.name == "onepass" ? smr::ONEPASS : smr::TWOPASS);
        else if (ev.kind == "type") e.changeFilterType (ev.name == "bpre" ? smr::BPRE : smr::MAXQ);
        e.setControls (ctl);
    }
}

int main (int argc, char** argv)
{
    if (argc >= 2 && (std::strcmp (argv[1], "--help") == 0 || std::strcmp (argv[1], "-h") == 0)) { std::fputs (kHelp, stdout); return 0; }
    if (argc < 3) { std::fputs (kHelp, stderr); return 2; }

    Cfg cfg;
    for (int k = 2; k <= 7; ++k) cfg.ctl.adc[k] = 4095;
    for (int k = 1; k <= 6; ++k) cfg.ctl.pot[k] = 4095;
    cfg.ctl.pot[0] = 2048;

    const char* inPath = argv[1];
    const char* outPath = argv[2];
    for (int i = 3; i < argc; ++i)
    {
        const std::string a = argv[i];
        auto next = [&] () -> std::string { if (i + 1 >= argc) { std::fprintf (stderr, "error: %s needs a value\n", a.c_str()); std::exit (2); } return argv[++i]; };
        int k = 0, v = 0;
        if (a == "--adc")
        {
            if (! splitKV (next(), k, v) || k < 0 || k >= smr::kNumAdcs || v < 0 || v > 4095) { std::fputs ("error: --adc N=V with N 0..14 and V 0..4095\n", stderr); return 2; }
            cfg.ctl.adc[k] = (uint16_t) v;
        }
        else if (a == "--pot")
        {
            if (! splitKV (next(), k, v) || k < 0 || k >= smr::kNumAdc3s || v < 0 || v > 4095) { std::fputs ("error: --pot N=V with N 0..6 and V 0..4095\n", stderr); return 2; }
            cfg.ctl.pot[k] = (uint16_t) v;
        }
        else if (a == "--sw")
        {
            const std::string s = next();
            const size_t eq = s.find ('=');
            if (eq == std::string::npos || ! setSwitch (cfg.ctl, s.substr (0, eq), s.substr (eq + 1) != "0")) { std::fputs ("error: --sw NAME=0|1 (see --help for names)\n", stderr); return 2; }
        }
        else if (a == "--mode") { const std::string s = next(); if (s == "twopass") cfg.mode = 2; else if (s == "onepass") cfg.mode = 3; else { std::fputs ("error: --mode twopass|onepass\n", stderr); return 2; } }
        else if (a == "--type") { const std::string s = next(); if (s == "maxq") cfg.type = 2; else if (s == "bpre") cfg.type = 1; else { std::fputs ("error: --type maxq|bpre\n", stderr); return 2; } }
        else if (a == "--bank") { cfg.bank = std::atoi (next().c_str()); if (cfg.bank < 0 || cfg.bank >= smr::kNumScaleBanks) { std::fputs ("error: --bank 0..19\n", stderr); return 2; } }
        else if (a == "--freqblock") cfg.freqblock = (uint32_t) std::strtoul (next().c_str(), nullptr, 0);
        else if (a == "--script") cfg.script = next();
        else if (a == "--block") { cfg.block = std::atoi (next().c_str()); if (cfg.block < 1 || cfg.block > 8192) { std::fputs ("error: --block 1..8192\n", stderr); return 2; } }
        else if (a == "--tail") cfg.tail = std::atof (next().c_str());
        else if (a == "--no-compensate") cfg.compensate = false;
        else { std::fprintf (stderr, "error: unknown option %s (see --help)\n", a.c_str()); return 2; }
    }

    wav::Audio in;
    std::string err;
    if (! wav::read (inPath, in, err)) { std::fprintf (stderr, "error: %s: %s\n", inPath, err.c_str()); return 1; }

    smr_host::BlockAdapter ad;
    if (! ad.prepare (in.rate, cfg.block))
    {
        std::fprintf (stderr, "error: sample rate %d Hz is not in the exact-ratio list (22050 32000 44100 48000 88200 96000 176400 192000). "
                              "Convert the file to one of these rates first.\n", in.rate);
        return 1;
    }

    std::vector<Event> events;
    if (! cfg.script.empty() && ! loadScript (cfg.script, events, err)) { std::fprintf (stderr, "error: %s\n", err.c_str()); return 1; }

    smr::Engine& e = ad.engine();
    smr::Controls ctl = cfg.ctl;
    e.setControls (ctl);
    ad.reset();   // boot with these controls
    if (cfg.mode) e.setFilterMode (cfg.mode == 3 ? smr::ONEPASS : smr::TWOPASS);
    if (cfg.type) e.changeFilterType (cfg.type == 1 ? smr::BPRE : smr::MAXQ);
    if (cfg.bank >= 0) for (int i = 0; i < smr::kNumChannels; ++i) e.scale_bank[i] = (uint8_t) cfg.bank;
    if (cfg.freqblock && ! e.setFreqBlock (cfg.freqblock)) { std::fputs ("error: --freqblock must be below bit 20 with at most 14 bits set\n", stderr); return 1; }

    const size_t inFrames = in.frames();
    const size_t tailFrames = (size_t) std::llround (cfg.tail * (double) in.rate);
    const size_t latency = cfg.compensate ? (size_t) std::llround (ad.latencyHostFrames()) : 0;
    const size_t total = inFrames + tailFrames + latency;

    std::vector<float> inL (cfg.block), inR (cfg.block), outL (cfg.block), outR (cfg.block);
    wav::Audio out;
    out.rate = in.rate;
    out.channels = 2;
    out.data.reserve ((total - latency) * 2);

    size_t evIdx = 0;
    for (size_t pos = 0; pos < total; pos += (size_t) cfg.block)
    {
        const int n = (int) std::min ((size_t) cfg.block, total - pos);
        const double t = (double) pos / (double) in.rate;
        while (evIdx < events.size() && events[evIdx].t <= t) applyEvent (e, ctl, events[evIdx++]);

        for (int k = 0; k < n; ++k)
        {
            const size_t fr = pos + (size_t) k;
            if (fr < inFrames)
            {
                inL[(size_t) k] = in.data[fr * (size_t) in.channels];
                inR[(size_t) k] = in.data[fr * (size_t) in.channels + (size_t) (in.channels - 1)];
            }
            else inL[(size_t) k] = inR[(size_t) k] = 0.0f;
        }
        ad.process (inL.data(), inR.data(), outL.data(), outR.data(), n);
        for (int k = 0; k < n; ++k)
            if (pos + (size_t) k >= latency)
            {
                out.data.push_back (outL[(size_t) k]);
                out.data.push_back (outR[(size_t) k]);
            }
    }

    if (! wav::writeFloat (outPath, out)) { std::fprintf (stderr, "error: cannot write %s\n", outPath); return 1; }

    float peak = 0.0f;
    for (float x : out.data) peak = std::max (peak, std::fabs (x));
    std::printf ("smr_render: %zu frames in, %zu frames out at %d Hz\n", inFrames, out.frames(), in.rate);
    std::printf ("  host block %d, core blocks %llu, trimmed latency %zu frames, underruns %llu\n", cfg.block,
                 (unsigned long long) ad.blocksProcessed(), latency, (unsigned long long) ad.underruns());
    std::printf ("  output peak %.6f (%.2f dBFS), audio saturation events %llu, search-loop guard trips %llu\n", (double) peak,
                 peak > 0.0f ? 20.0 * std::log10 ((double) peak) : -999.0, (unsigned long long) e.saturation_count,
                 (unsigned long long) e.loop_guard_trips);
    return 0;
}
