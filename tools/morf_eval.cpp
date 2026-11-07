// morf_eval: run a tracker over a test set and print a scorecard.
//
//   morf_eval                         synthetic test set, markdown report to stdout
//   morf_eval --dump out/             also write each test file as .wav + .notes.csv
//   morf_eval --dir recordings/       score your own labelled recordings: for each foo.wav,
//                                     foo.csv holds lines "onset_s,offset_s,midi"
//   morf_eval --trace bends_00        print the tracker's frame-by-frame view of one test file
//   options: --seed N --files N --block N (host buffer size) --hop N --rate HZ
#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>

#include "../eval/evaluate.h"
#include "../io/audio_io.h"
#include "../synth/test_set.h"
#include "morf/mono_tracker.h"

using namespace morf;
namespace fs = std::filesystem;

static std::vector<MidiEvent> runTracker(Tracker& tr, const Audio& a, int block, double* cpuSeconds) {
    std::vector<MidiEvent> all;
    EventQueue q;
    tr.reset();
    const auto t0 = std::chrono::steady_clock::now();
    for (size_t i = 0; i < a.samples.size(); i += block) {
        q.clear();
        tr.process(a.samples.data() + i, int(std::min<size_t>(block, a.samples.size() - i)), q);
        all.insert(all.end(), q.begin(), q.end());
    }
    *cpuSeconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    return all;
}

int main(int argc, char** argv) {
    uint32_t seed = 7;
    int files = 6, block = 128;
    double rate = 48000;
    MonoTrackerConfig cfg;
    std::string dumpDir, userDir, traceName;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        auto next = [&]() -> std::string { return i + 1 < argc ? argv[++i] : ""; };
        if (a == "--seed") seed = uint32_t(std::stoul(next()));
        else if (a == "--files") files = std::stoi(next());
        else if (a == "--block") block = std::stoi(next());
        else if (a == "--hop") cfg.hop = std::stoi(next());
        else if (a == "--rate") rate = std::stod(next());
        else if (a == "--dump") dumpDir = next();
        else if (a == "--dir") userDir = next();
        else if (a == "--trace") traceName = next();
        else { std::cerr << "unknown option " << a << "\n"; return 2; }
    }

    std::vector<CaseResult> results;
    if (!userDir.empty()) {
        for (const auto& entry : fs::directory_iterator(userDir)) {
            if (entry.path().extension() != ".wav") continue;
            Audio audio;
            std::string err;
            if (!readWav(entry.path().string(), audio, &err)) { std::cerr << entry.path() << ": " << err << "\n"; continue; }
            std::vector<NoteSpec> notes;
            std::ifstream csv(fs::path(entry.path()).replace_extension(".csv"));
            for (std::string line; std::getline(csv, line);) {
                NoteSpec n;
                if (std::sscanf(line.c_str(), "%lf,%lf,%d", &n.onset, &n.offset, &n.midi) == 3) notes.push_back(n);
            }
            cfg.sampleRate = audio.sampleRate;
            MonoTracker tr(cfg);
            double cpu = 0;
            const auto ev = runTracker(tr, audio, block, &cpu);
            auto r = evaluateCase(notes, nullptr, ev, audio.sampleRate, audio.samples.size() / audio.sampleRate);
            r.name = entry.path().stem().string();
            r.category = "recordings";
            r.cpuSeconds = cpu;
            results.push_back(r);
        }
        std::cout << summarise(results, "Your recordings — " + std::string(MonoTracker(cfg).name()));
        return 0;
    }

    cfg.sampleRate = rate;
    const auto set = makeSyntheticTestSet(seed, rate, files);
    MonoTracker tr(cfg);
    if (!traceName.empty()) {
        for (const auto& tc : set) {
            if (tc.name != traceName) continue;
            for (const auto& n : tc.notes)
                std::printf("truth %7.3f-%7.3f midi %d%s\n", n.onset, n.offset, n.midi, n.bend.empty() ? "" : " (bend)");
            EventQueue q;
            for (size_t i = 0; i + cfg.hop <= tc.audio.samples.size(); i += cfg.hop) {
                q.clear();
                tr.process(tc.audio.samples.data() + i, cfg.hop, q);
                const auto& f = tr.lastFrame();
                std::printf("%7.4f lvl %6.1f f0 %7.2f (%6.2f) ap %.3f %s%s", (i + cfg.hop) / rate, f.levelDb, f.f0,
                            f.f0 > 0 ? hzToMidi(f.f0) : 0.f, f.aperiodicity, f.voiced ? "V" : "-", f.onset ? " ONSET" : "");
                for (const auto& e : q) {
                    if (e.isNoteOn()) std::printf("  [ON %d v%d]", e.data1, e.data2);
                    else if (e.isNoteOff()) std::printf("  [OFF %d]", e.data1);
                    else std::printf("  [PB %+.2f]", (e.pitchBendValue() - 8192) / 8191.f * cfg.bendRange);
                }
                std::printf("\n");
            }
        }
        return 0;
    }
    if (!dumpDir.empty()) fs::create_directories(dumpDir);
    for (const auto& tc : set) {
        double cpu = 0;
        const auto ev = runTracker(tr, tc.audio, block, &cpu);
        auto r = evaluateCase(tc.notes, &tc.truth, ev, rate, tc.audio.samples.size() / rate);
        r.name = tc.name;
        r.category = tc.category;
        r.cpuSeconds = cpu;
        results.push_back(r);
        if (!dumpDir.empty()) {
            const std::string base = dumpDir + "/" + tc.name;
            writeWav(base + ".wav", tc.audio);
            std::ofstream csv(base + ".csv");
            for (const auto& n : tc.notes) csv << n.onset << "," << n.offset << "," << n.midi << "\n";
            writeMidiFile(base + ".tracked.mid", ev, rate, int(cfg.bendRange));
        }
    }
    std::ostringstream title;
    title << "Synthetic set (seed " << seed << ", " << rate / 1000 << " kHz, hop " << cfg.hop << ", host block " << block
          << ") — " << tr.name();
    std::cout << summarise(results, title.str());
    return 0;
}
