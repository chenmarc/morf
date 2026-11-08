#pragma once
#include <string>
#include <vector>

#include "morf/events.h"

namespace morf {

struct Audio {
    double sampleRate = 48000.0;
    std::vector<float> samples;  // mono
};

// Reads 16/24/32-bit PCM or 32-bit float WAV; multichannel files are mixed to mono.
bool readWav(const std::string& path, Audio& out, std::string* err = nullptr);
bool writeWav(const std::string& path, const Audio& audio);  // 32-bit float mono

// Writes a type-0 Standard MIDI File. Event times are in samples at sampleRate.
bool writeMidiFile(const std::string& path, const std::vector<MidiEvent>& events, double sampleRate,
                   int bendRangeSemitones);

}  // namespace morf
