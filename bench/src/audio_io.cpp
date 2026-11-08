#include "morf/bench/audio_io.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iterator>

namespace morf {

namespace {
uint32_t rd32(const uint8_t* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | (uint32_t(p[3]) << 24); }
uint16_t rd16(const uint8_t* p) { return uint16_t(p[0] | (p[1] << 8)); }
void put32(std::ofstream& f, uint32_t v) { for (int i = 0; i < 4; ++i) f.put(char((v >> (8 * i)) & 0xFF)); }
void put16(std::ofstream& f, uint16_t v) { f.put(char(v & 0xFF)); f.put(char(v >> 8)); }
bool fail(std::string* err, const char* msg) { if (err) *err = msg; return false; }
}  // namespace

bool readWav(const std::string& path, Audio& out, std::string* err) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return fail(err, "cannot open file");
    std::vector<uint8_t> d((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    if (d.size() < 12 || std::memcmp(d.data(), "RIFF", 4) || std::memcmp(d.data() + 8, "WAVE", 4))
        return fail(err, "not a RIFF/WAVE file");
    int format = 0, channels = 0, bits = 0;
    uint32_t rate = 0;
    const uint8_t* data = nullptr;
    size_t dataLen = 0;
    for (size_t p = 12; p + 8 <= d.size();) {
        const uint32_t len = rd32(&d[p + 4]);
        const uint8_t* body = &d[p + 8];
        const size_t avail = std::min<size_t>(len, d.size() - p - 8);
        if (!std::memcmp(&d[p], "fmt ", 4) && avail >= 16) {
            format = rd16(body);
            channels = rd16(body + 2);
            rate = rd32(body + 4);
            bits = rd16(body + 14);
            if (format == 0xFFFE && avail >= 26) format = rd16(body + 24);  // WAVE_FORMAT_EXTENSIBLE
        } else if (!std::memcmp(&d[p], "data", 4)) {
            data = body;
            dataLen = avail;
        }
        p += 8 + len + (len & 1);
    }
    if (!data || channels <= 0) return fail(err, "missing fmt or data chunk");
    if (!((format == 1 && (bits == 16 || bits == 24 || bits == 32)) || (format == 3 && bits == 32)))
        return fail(err, "unsupported sample format (need 16/24/32-bit PCM or 32-bit float)");
    const int bps = bits / 8;
    const size_t frames = dataLen / (size_t(bps) * channels);
    out.sampleRate = rate;
    out.samples.assign(frames, 0.f);
    for (size_t i = 0; i < frames; ++i) {
        float acc = 0.f;
        for (int c = 0; c < channels; ++c) {
            const uint8_t* s = data + (i * channels + c) * bps;
            float v;
            if (format == 3) std::memcpy(&v, s, 4);
            else if (bits == 16) v = int16_t(rd16(s)) / 32768.f;
            else if (bits == 24) v = float(int32_t((s[0] << 8) | (s[1] << 16) | (uint32_t(s[2]) << 24)) >> 8) / 8388608.f;
            else v = float(int32_t(rd32(s))) / 2147483648.f;
            acc += v;
        }
        out.samples[i] = acc / channels;
    }
    return true;
}

bool writeWav(const std::string& path, const Audio& a) {
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    const uint32_t dataLen = uint32_t(a.samples.size() * 4);
    f.write("RIFF", 4); put32(f, 36 + dataLen); f.write("WAVE", 4);
    f.write("fmt ", 4); put32(f, 16); put16(f, 3); put16(f, 1);
    put32(f, uint32_t(a.sampleRate)); put32(f, uint32_t(a.sampleRate) * 4); put16(f, 4); put16(f, 32);
    f.write("data", 4); put32(f, dataLen);
    f.write(reinterpret_cast<const char*>(a.samples.data()), dataLen);
    return bool(f);
}

bool writeMidiFile(const std::string& path, const std::vector<MidiEvent>& events, double sampleRate, int bendRange) {
    // 120 bpm, 960 ticks per quarter => 1920 ticks per second.
    const int ppq = 960;
    const double ticksPerSample = 1920.0 / sampleRate;
    std::vector<uint8_t> trk;
    auto varlen = [&](uint32_t v) {
        uint8_t buf[5]; int n = 0;
        buf[n++] = v & 0x7F;
        while (v >>= 7) buf[n++] = uint8_t(0x80 | (v & 0x7F));
        while (n) trk.push_back(buf[--n]);
    };
    // Tempo 500000 us/quarter, then RPN 0 to set the pitch-bend range on channel 1.
    varlen(0); trk.insert(trk.end(), {0xFF, 0x51, 0x03, 0x07, 0xA1, 0x20});
    const uint8_t rpn[][3] = {{0xB0, 101, 0}, {0xB0, 100, 0}, {0xB0, 6, uint8_t(bendRange)}, {0xB0, 38, 0}, {0xB0, 101, 127}, {0xB0, 100, 127}};
    for (auto& m : rpn) { varlen(0); trk.insert(trk.end(), m, m + 3); }
    uint32_t lastTick = 0;
    for (const auto& e : events) {
        const uint32_t tick = uint32_t(std::llround(double(e.time) * ticksPerSample));
        varlen(tick > lastTick ? tick - lastTick : 0);
        lastTick = std::max(lastTick, tick);
        trk.push_back(e.status); trk.push_back(e.data1); trk.push_back(e.data2);
    }
    varlen(0); trk.insert(trk.end(), {0xFF, 0x2F, 0x00});

    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    auto be32 = [&](uint32_t v) { for (int i = 3; i >= 0; --i) f.put(char((v >> (8 * i)) & 0xFF)); };
    auto be16 = [&](uint16_t v) { f.put(char(v >> 8)); f.put(char(v & 0xFF)); };
    f.write("MThd", 4); be32(6); be16(0); be16(1); be16(ppq);
    f.write("MTrk", 4); be32(uint32_t(trk.size()));
    f.write(reinterpret_cast<const char*>(trk.data()), std::streamsize(trk.size()));
    return bool(f);
}

}  // namespace morf
