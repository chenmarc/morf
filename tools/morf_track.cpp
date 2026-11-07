// morf_track: convert a guitar recording to a MIDI file with the baseline tracker.
//   morf_track input.wav output.mid [--print]
// Load the .mid in Logic/Ableton on a synth with pitch-bend range set to 12 semitones.
#include <cstring>
#include <iostream>

#include "../io/audio_io.h"
#include "morf/mono_tracker.h"

using namespace morf;

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "usage: morf_track input.wav output.mid [--print]\n";
        return 2;
    }
    Audio audio;
    std::string err;
    if (!readWav(argv[1], audio, &err)) { std::cerr << argv[1] << ": " << err << "\n"; return 1; }
    MonoTrackerConfig cfg;
    cfg.sampleRate = audio.sampleRate;
    MonoTracker tr(cfg);
    std::vector<MidiEvent> all;
    EventQueue q;
    for (size_t i = 0; i < audio.samples.size(); i += 128) {
        q.clear();
        tr.process(audio.samples.data() + i, int(std::min<size_t>(128, audio.samples.size() - i)), q);
        all.insert(all.end(), q.begin(), q.end());
    }
    const bool print = argc > 3 && !std::strcmp(argv[3], "--print");
    int notes = 0;
    for (const auto& e : all) {
        if (e.isNoteOn()) ++notes;
        if (print && !e.isPitchBend())
            std::printf("%9.4f s  %-8s note %3d vel %3d\n", e.time / audio.sampleRate, e.isNoteOn() ? "NoteOn" : "NoteOff", e.data1, e.data2);
    }
    if (!writeMidiFile(argv[2], all, audio.sampleRate, int(cfg.bendRange))) { std::cerr << "cannot write " << argv[2] << "\n"; return 1; }
    std::cout << notes << " notes, " << all.size() << " MIDI events -> " << argv[2] << "\n";
    return 0;
}
