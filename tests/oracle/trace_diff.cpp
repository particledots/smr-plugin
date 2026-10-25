// Compares two trace files (see trace_common.h). Exit 0: identical. Exit 1: first difference is printed
// as step number, field name and both values, and the count of differing words is given.
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

static bool load (const char* path, uint32_t& steps, uint32_t& wps, std::vector<std::string>& names, std::vector<uint32_t>& data)
{
    FILE* f = std::fopen (path, "rb");
    if (! f) return false;
    char magic[8];
    if (std::fread (magic, 1, 8, f) != 8 || std::memcmp (magic, "SMRTRC01", 8) != 0) { std::fclose (f); return false; }
    if (std::fread (&steps, 4, 1, f) != 1 || std::fread (&wps, 4, 1, f) != 1) { std::fclose (f); return false; }
    for (uint32_t k = 0; k < wps; ++k)
    {
        std::string s;
        int c;
        while ((c = std::fgetc (f)) > 0) s.push_back ((char) c);
        names.push_back (s);
    }
    data.resize ((size_t) steps * wps);
    bool ok = data.empty() || std::fread (data.data(), 4, data.size(), f) == data.size();
    std::fclose (f);
    return ok;
}

static bool isFloatField (const std::string& n)
{
    return n.rfind ("ENVOUT_preload", 0) == 0 || n.rfind ("motion_morphpos", 0) == 0 || n.rfind ("freq_nudge", 0) == 0
           || n.rfind ("freq_shift", 0) == 0 || n.rfind ("channel_level", 0) == 0 || n == "CHANNEL_LEVEL_LPF" || n == "f_morph";
}

int main (int argc, char** argv)
{
    if (argc < 3) { std::printf ("usage: trace_diff <oracle.trace> <port.trace>\n"); return 2; }
    uint32_t sa = 0, wa = 0, sb = 0, wb = 0;
    std::vector<std::string> na, nb;
    std::vector<uint32_t> da, db;
    if (! load (argv[1], sa, wa, na, da)) { std::printf ("FAIL: cannot read %s\n", argv[1]); return 2; }
    if (! load (argv[2], sb, wb, nb, db)) { std::printf ("FAIL: cannot read %s\n", argv[2]); return 2; }
    if (sa != sb || wa != wb || na != nb)
    {
        std::printf ("FAIL: trace shapes differ (steps %u vs %u, words per step %u vs %u)\n", sa, sb, wa, wb);
        return 1;
    }
    size_t bad = 0, first = (size_t) -1;
    for (size_t k = 0; k < da.size(); ++k)
        if (da[k] != db[k])
        {
            if (first == (size_t) -1) first = k;
            ++bad;
        }
    if (! bad)
    {
        std::printf ("PASS: %u steps x %u words identical\n", sa, wa);
        return 0;
    }
    const size_t step = first / wa, field = first % wa;
    auto show = [&] (uint32_t u) -> std::string {
        char b[64];
        if (isFloatField (na[field])) { float f; std::memcpy (&f, &u, 4); std::snprintf (b, sizeof b, "0x%08x (%.9g)", u, (double) f); }
        else std::snprintf (b, sizeof b, "0x%08x (%d)", u, (int) u);
        return b;
    };
    std::printf ("FAIL: first difference at step %zu, field %s: oracle %s, port %s; %zu differing words in total\n",
                 step, na[field].c_str(), show (da[first]).c_str(), show (db[first]).c_str(), bad);
    return 1;
}
