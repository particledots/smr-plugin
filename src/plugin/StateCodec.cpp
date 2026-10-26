#include "StateCodec.h"

#include <cstring>
#include <vector>

namespace smr_plugin
{
    namespace
    {
        constexpr size_t kBytes = 1 + 3 + 4 + 5 * smr::kNumChannels + 4 * smr::kNumChannels + 4 * smr::kNumChannels;   // 86

        void put32 (std::vector<uint8_t>& b, uint32_t v) { for (int i = 0; i < 4; ++i) b.push_back ((uint8_t) (v >> (8 * i))); }
        uint32_t get32 (const uint8_t* p) { return (uint32_t) p[0] | ((uint32_t) p[1] << 8) | ((uint32_t) p[2] << 16) | ((uint32_t) p[3] << 24); }
    }

    std::string encodeSnapshot (const smr::Snapshot& s)
    {
        std::vector<uint8_t> b;
        b.reserve (kBytes);
        b.push_back ((uint8_t) kStateVersion);
        b.push_back (s.filter_type);
        b.push_back (s.filter_mode);
        b.push_back (s.hover_scale_bank);
        put32 (b, s.freqblock);
        for (int i = 0; i < smr::kNumChannels; ++i) b.push_back (s.note[i]);
        for (int i = 0; i < smr::kNumChannels; ++i) b.push_back (s.scale[i]);
        for (int i = 0; i < smr::kNumChannels; ++i) b.push_back (s.scale_bank[i]);
        for (int i = 0; i < smr::kNumChannels; ++i) b.push_back (s.lock[i]);
        for (int i = 0; i < smr::kNumChannels; ++i) b.push_back (s.q_locked[i]);
        for (int i = 0; i < smr::kNumChannels; ++i) put32 (b, s.qval[i]);
        for (int i = 0; i < smr::kNumChannels; ++i)
        {
            uint32_t bits;
            std::memcpy (&bits, &s.freq_nudge[i], 4);
            put32 (b, bits);
        }

        static const char* hex = "0123456789abcdef";
        std::string out;
        out.reserve (b.size() * 2);
        for (uint8_t x : b) { out.push_back (hex[x >> 4]); out.push_back (hex[x & 15]); }
        return out;
    }

    bool decodeSnapshot (const std::string& text, smr::Snapshot& out)
    {
        if (text.size() != kBytes * 2) return false;

        uint8_t b[kBytes];
        for (size_t i = 0; i < kBytes; ++i)
        {
            int v[2];
            for (int k = 0; k < 2; ++k)
            {
                const char c = text[i * 2 + (size_t) k];
                if (c >= '0' && c <= '9') v[k] = c - '0';
                else if (c >= 'a' && c <= 'f') v[k] = c - 'a' + 10;
                else if (c >= 'A' && c <= 'F') v[k] = c - 'A' + 10;
                else return false;
            }
            b[i] = (uint8_t) (v[0] * 16 + v[1]);
        }
        if (b[0] != kStateVersion) return false;

        smr::Snapshot s;
        s.filter_type = b[1];
        s.filter_mode = b[2];
        s.hover_scale_bank = b[3];
        s.freqblock = get32 (b + 4);
        const uint8_t* p = b + 8;
        for (int i = 0; i < smr::kNumChannels; ++i) s.note[i] = *p++;
        for (int i = 0; i < smr::kNumChannels; ++i) s.scale[i] = *p++;
        for (int i = 0; i < smr::kNumChannels; ++i) s.scale_bank[i] = *p++;
        for (int i = 0; i < smr::kNumChannels; ++i) s.lock[i] = *p++;
        for (int i = 0; i < smr::kNumChannels; ++i) s.q_locked[i] = *p++;
        for (int i = 0; i < smr::kNumChannels; ++i) { s.qval[i] = get32 (p); p += 4; }
        for (int i = 0; i < smr::kNumChannels; ++i)
        {
            const uint32_t bits = get32 (p);
            p += 4;
            std::memcpy (&s.freq_nudge[i], &bits, 4);
        }
        out = s;
        return true;
    }
}
