// Minimal WAV reader/writer for the offline renderer (RIFF/WAVE; PCM 8/16/24/32, float 32/64; WAVE_FORMAT_EXTENSIBLE).
#pragma once

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace wav
{
    struct Audio
    {
        int rate = 0;
        int channels = 0;
        std::vector<float> data;   // interleaved
        size_t frames() const { return channels ? data.size() / (size_t) channels : 0; }
    };

    inline uint16_t rd16 (const uint8_t* p) { return (uint16_t) (p[0] | (p[1] << 8)); }
    inline uint32_t rd32 (const uint8_t* p) { return (uint32_t) p[0] | ((uint32_t) p[1] << 8) | ((uint32_t) p[2] << 16) | ((uint32_t) p[3] << 24); }

    inline bool read (const char* path, Audio& a, std::string& err)
    {
        FILE* f = std::fopen (path, "rb");
        if (! f) { err = "cannot open file"; return false; }
        std::vector<uint8_t> buf;
        {
            uint8_t tmp[65536];
            size_t n;
            while ((n = std::fread (tmp, 1, sizeof tmp, f)) > 0) buf.insert (buf.end(), tmp, tmp + n);
        }
        std::fclose (f);

        if (buf.size() < 12 || std::memcmp (buf.data(), "RIFF", 4) != 0 || std::memcmp (buf.data() + 8, "WAVE", 4) != 0)
        { err = "not a RIFF/WAVE file"; return false; }

        int format = 0, bits = 0;
        const uint8_t* dataPtr = nullptr;
        size_t dataLen = 0;
        size_t pos = 12;
        while (pos + 8 <= buf.size())
        {
            const uint8_t* ck = buf.data() + pos;
            size_t len = rd32 (ck + 4);
            const uint8_t* body = ck + 8;
            if (pos + 8 + len > buf.size()) len = buf.size() - pos - 8;   // tolerate a truncated last chunk
            if (std::memcmp (ck, "fmt ", 4) == 0 && len >= 16)
            {
                format = rd16 (body);
                a.channels = rd16 (body + 2);
                a.rate = (int) rd32 (body + 4);
                bits = rd16 (body + 14);
                if (format == 0xFFFE && len >= 26) format = rd16 (body + 24);   // extensible: first 2 bytes of the sub-format GUID
            }
            else if (std::memcmp (ck, "data", 4) == 0)
            {
                dataPtr = body;
                dataLen = len;
            }
            pos += 8 + len + (len & 1);
        }
        if (! dataPtr || a.channels < 1 || a.rate < 1) { err = "missing fmt or data chunk"; return false; }
        if (a.channels > 2) { err = "more than 2 channels (only mono and stereo are supported)"; return false; }

        const int bytes = bits / 8;
        if (! ((format == 1 && (bits == 8 || bits == 16 || bits == 24 || bits == 32)) || (format == 3 && (bits == 32 || bits == 64))))
        { err = "unsupported sample format (format " + std::to_string (format) + ", " + std::to_string (bits) + " bits)"; return false; }

        const size_t count = dataLen / (size_t) bytes;
        a.data.resize (count - count % (size_t) a.channels);
        for (size_t i = 0; i < a.data.size(); ++i)
        {
            const uint8_t* p = dataPtr + i * (size_t) bytes;
            float v = 0.0f;
            if (format == 1)
            {
                if (bits == 8) v = ((float) p[0] - 128.0f) / 128.0f;
                else if (bits == 16) v = (float) (int16_t) rd16 (p) / 32768.0f;
                else if (bits == 24) { int32_t x = (int32_t) (((uint32_t) p[0] << 8) | ((uint32_t) p[1] << 16) | ((uint32_t) p[2] << 24)); v = (float) ((double) x / 2147483648.0); }
                else { int32_t x = (int32_t) rd32 (p); v = (float) ((double) x / 2147483648.0); }
            }
            else if (bits == 32) { uint32_t u = rd32 (p); std::memcpy (&v, &u, 4); }
            else { uint64_t u = (uint64_t) rd32 (p) | ((uint64_t) rd32 (p + 4) << 32); double d; std::memcpy (&d, &u, 8); v = (float) d; }
            a.data[i] = v;
        }
        return true;
    }

    inline void put16 (std::vector<uint8_t>& b, uint16_t v) { b.push_back ((uint8_t) v); b.push_back ((uint8_t) (v >> 8)); }
    inline void put32 (std::vector<uint8_t>& b, uint32_t v) { for (int i = 0; i < 4; ++i) b.push_back ((uint8_t) (v >> (8 * i))); }

    // 32-bit float, interleaved
    inline bool writeFloat (const char* path, const Audio& a)
    {
        std::vector<uint8_t> b;
        const uint32_t dataBytes = (uint32_t) (a.data.size() * 4);
        b.insert (b.end(), { 'R', 'I', 'F', 'F' });
        put32 (b, 36 + dataBytes);
        b.insert (b.end(), { 'W', 'A', 'V', 'E', 'f', 'm', 't', ' ' });
        put32 (b, 16);
        put16 (b, 3);
        put16 (b, (uint16_t) a.channels);
        put32 (b, (uint32_t) a.rate);
        put32 (b, (uint32_t) (a.rate * a.channels * 4));
        put16 (b, (uint16_t) (a.channels * 4));
        put16 (b, 32);
        b.insert (b.end(), { 'd', 'a', 't', 'a' });
        put32 (b, dataBytes);
        const size_t head = b.size();
        b.resize (head + dataBytes);
        std::memcpy (b.data() + head, a.data.data(), dataBytes);
        FILE* f = std::fopen (path, "wb");
        if (! f) return false;
        const bool ok = std::fwrite (b.data(), 1, b.size(), f) == b.size();
        std::fclose (f);
        return ok;
    }
}
