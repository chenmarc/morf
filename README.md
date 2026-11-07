# morf: guitar to MIDI

Low-latency guitar-to-MIDI engine. Step 1 of the plan includes a test bench and a single-note baseline tracker. The JUCE plugin (standalone, VST3 and AU) comes in step 2 and will link `morf_core` unchanged.

## Build (macOS or Linux)

```sh
cmake -S . -B build -G Ninja      # or omit -G Ninja to use Make / Xcode
cmake --build build
./build/morf_tests                # unit tests
./build/morf_eval                 # scorecard on the synthetic test set
```

You need CMake 3.20+ and a C++17 compiler (Xcode command-line tools are fine). There are no other dependencies.

## Tools

| Command | What it does |
|---|---|
| `morf_eval` | Builds the synthetic test set, runs the tracker and prints accuracy, latency and pitch error per playing technique |
| `morf_eval --seed 1234 --files 10` | A different, larger test set (use for held-out checks) |
| `morf_eval --dump out/` | Also writes each test as `.wav`, ground-truth `.csv` and the tracker's `.tracked.mid`, so you can listen |
| `morf_eval --trace bends_00` | Shows the frame-by-frame view: level, pitch, aperiodicity and events |
| `morf_eval --dir my_clips/` | Scores your own recordings: `foo.wav` with `foo.csv` lines of `onset_s,offset_s,midi` |
| `morf_track in.wav out.mid --print` | Converts any guitar recording to MIDI (set your synth's bend range to 12) |

## Layout

```
core/    real-time engine (no deps, no allocation in process()): FFT, YIN, MonoTracker
synth/   synthetic guitar (Karplus-Strong string model) + test-set generator
eval/    scoring: note matching, latency, pitch error, octave errors
io/      WAV and Standard MIDI File I/O
tools/   morf_eval, morf_track
tests/   unit tests
docs/    results.md, dsp-primer.md, polyphonic-research.md
```

Start with `docs/results.md` for where things stand, and `docs/dsp-primer.md` for how it works.
