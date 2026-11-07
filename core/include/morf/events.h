#pragma once
#include <array>
#include <cstdint>

namespace morf {

// A raw 3-byte MIDI channel message stamped with the absolute sample index
// at which the tracker emitted it (the end of the analysis hop that produced it).
struct MidiEvent {
    int64_t time = 0;
    uint8_t status = 0, data1 = 0, data2 = 0;

    int type() const { return status & 0xF0; }
    int channel() const { return status & 0x0F; }
    bool isNoteOn() const { return type() == 0x90 && data2 > 0; }
    bool isNoteOff() const { return type() == 0x80 || (type() == 0x90 && data2 == 0); }
    bool isPitchBend() const { return type() == 0xE0; }
    int pitchBendValue() const { return (int(data2) << 7) | int(data1); }  // 0..16383, centre 8192
};

// Fixed-capacity event queue so the audio thread never allocates.
class EventQueue {
public:
    static constexpr int kCapacity = 1024;
    bool push(const MidiEvent& e) {
        if (size_ >= kCapacity) { ++dropped_; return false; }
        events_[size_t(size_++)] = e;
        return true;
    }
    void clear() { size_ = 0; }
    int size() const { return size_; }
    const MidiEvent& operator[](int i) const { return events_[size_t(i)]; }
    const MidiEvent* begin() const { return events_.data(); }
    const MidiEvent* end() const { return events_.data() + size_; }
    int dropped() const { return dropped_; }

private:
    std::array<MidiEvent, kCapacity> events_{};
    int size_ = 0;
    int dropped_ = 0;
};

inline MidiEvent makeNoteOn(int64_t t, int ch, int note, int vel) { return {t, uint8_t(0x90 | ch), uint8_t(note), uint8_t(vel)}; }
inline MidiEvent makeNoteOff(int64_t t, int ch, int note) { return {t, uint8_t(0x80 | ch), uint8_t(note), 0}; }
inline MidiEvent makePitchBend(int64_t t, int ch, int value14) {
    return {t, uint8_t(0xE0 | ch), uint8_t(value14 & 0x7F), uint8_t((value14 >> 7) & 0x7F)};
}

}  // namespace morf
