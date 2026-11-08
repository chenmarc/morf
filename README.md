# morf: guitar to MIDI

Low-latency guitar-to-MIDI as a standalone macOS app plus VST3 and AU plugins. It includes a test bench with a synthetic guitar for measuring accuracy and latency.

**Status:** single-note tracking with bends and mutes works (step 1–2). Polyphonic tracking is step 3. See `docs/results.md` and `docs/daw-setup.md`.

## Build

The same commands work on an Intel Mac, an Apple silicon Mac, Windows (Visual Studio 2022) and Linux:

```sh
cmake --preset default          # configure: plugin, app, tools and tests for this computer (downloads JUCE 8)
cmake --build --preset default  # build
ctest --preset default          # run the tests
./build/bin/morf_eval           # scorecard on the synthetic test set (Windows: build\bin\Release\morf_eval.exe)
```

| Preset | What it builds |
|---|---|
| `default` | Plugin, standalone app, tools and tests for the computer you're on. An Intel Mac gets an Intel build and an Apple silicon Mac gets an Apple silicon build |
| `mac-universal` | One macOS binary that runs natively on both Intel and Apple silicon (what CI ships). Builds into `build-universal/` |
| `bench` | Only the engine, tools and tests: no JUCE download, builds in seconds. Builds into `build-bench/` |

You need CMake 3.22+ and a C++17 compiler: Xcode command-line tools on macOS, Visual Studio 2022 on Windows. macOS builds run on macOS 10.15 or later. Windows builds are 64-bit and need no Visual C++ redistributable.

The plugin and app end up in `build/plugin/morf_plugin_artefacts/Release/`. To install them, run `scripts/install-mac.sh` on a Mac, or `scripts\install-windows.ps1` from an administrator PowerShell on Windows. See `docs/daw-setup.md` for the details and for using morf in Ableton, Logic and the standalone app.

## Tools

| Command | What it does |
|---|---|
| `build/bin/morf_eval` | Builds the synthetic test set, runs the tracker and prints accuracy, latency and pitch error per playing technique |
| `morf_eval --seed 1234 --files 10` | A different, larger test set (use for held-out checks) |
| `morf_eval --dump out/` | Also writes each test as `.wav`, ground-truth `.csv` and the tracker's `.tracked.mid`, so you can listen |
| `morf_eval --trace bends_00` | Shows the frame-by-frame view: level, pitch, aperiodicity and events |
| `morf_eval --dir my_clips/` | Scores your own recordings: `foo.wav` with `foo.csv` lines of `onset_s,offset_s,midi` |
| `morf_track in.wav out.mid --print` | Converts any guitar recording to MIDI (set your synth's bend range to 12) |

## Layout

```
core/     real-time engine (no dependencies, no allocation in process()): FFT, YIN, MonoTracker
bench/    offline test bench: synthetic guitar, WAV/MIDI file I/O, scoring, morf_eval and morf_track
plugin/   JUCE plugin (VST3, AU) and standalone app: processor, editor, monitor synth, icon
scripts/  one-command install of the built plugins on macOS and Windows
tests/    engine unit tests, plugin integration test and VST3 host test (run with ctest)
cmake/    shared build settings and the JUCE download
docs/     results, DAW setup, DSP primer, polyphonic research
```

Start with `docs/results.md` for where things stand, and `docs/dsp-primer.md` for how it works.

## File tree

```
morf/
├── CMakeLists.txt                       Top-level build: options and the list of subfolders
├── CMakePresets.json                    One-command builds: default, mac-universal, bench
├── README.md                            This file: build steps, tools, and the file tree
├── .gitignore                           Keeps build output and generated audio/MIDI out of git
├── .github/workflows/build.yml          CI: builds and tests on Linux, Intel Mac, Apple silicon Mac and Windows; validates and packages the plugins
├── cmake/
│   ├── MorfTargets.cmake                Target machines: native or universal Mac, minimum macOS, Windows runtime
│   ├── MorfPlatform.cmake               C++ standard, build type, output folders, compiler warnings
│   └── FetchJUCE.cmake                  Downloads the pinned JUCE release, or uses a local checkout
├── core/                                Real-time engine (no dependencies, no allocation in process())
│   ├── CMakeLists.txt                   Builds the morf_core library
│   ├── include/morf/
│   │   ├── events.h                     MIDI event struct and fixed-size event queue for the audio thread
│   │   ├── fft.h                        Declares the in-place radix-2 FFT
│   │   ├── mono_tracker.h               Declares the single-note tracker and its settings
│   │   ├── tracker.h                    Common interface every pitch-to-MIDI engine implements
│   │   └── yin.h                        Declares the YIN pitch detector
│   └── src/
│       ├── fft.cpp                      FFT implementation
│       ├── mono_tracker.cpp             Single-note tracker: onsets, pitch, bends, legato, mutes, note-off
│       └── yin.cpp                      YIN pitch detection, FFT-accelerated, with octave checks
├── bench/                               Offline test bench
│   ├── CMakeLists.txt                   Builds the morf_bench library and the two tools
│   ├── include/morf/bench/
│   │   ├── audio_io.h                   Declares WAV read/write and MIDI file write
│   │   ├── evaluate.h                   Declares the scoring functions and result types
│   │   ├── guitar_synth.h               Declares the synthetic guitar and its note specification
│   │   └── test_set.h                   Declares the labelled synthetic test-set generator
│   ├── src/
│   │   ├── audio_io.cpp                 WAV (16/24/32-bit PCM and float) and Standard MIDI File I/O
│   │   ├── evaluate.cpp                 Matches notes to ground truth; computes latency, pitch error and octave errors
│   │   ├── guitar_synth.cpp             Karplus-Strong string model with bends, vibrato, palm mutes and hum
│   │   └── test_set.cpp                 Generates single, legato, repeat, bend, vibrato, palm and chord test cases
│   └── tools/
│       ├── morf_eval.cpp                Scorecard runner: synthetic set, your own recordings, and frame traces
│       └── morf_track.cpp               Converts any guitar WAV into a MIDI file
├── plugin/                              JUCE plugin and standalone app (built with -DMORF_BUILD_PLUGIN=ON)
│   ├── CMakeLists.txt                   JUCE targets: Standalone, VST3, AU (macOS only); re-signs macOS bundles
│   ├── resources/
│   │   └── icon.png                     App icon: 1024 px macOS squircle
│   └── source/
│       ├── MonitorSynth.h               Small built-in synth so you can hear the tracking
│       ├── PluginEditor.h               Declares the plugin window
│       ├── PluginEditor.cpp             Window: detected note, bend bar, input meter, knobs and bend-range menu
│       ├── PluginProcessor.h            Declares the plugin processor, parameters, virtual MIDI port and VST3 side-chain
│       └── PluginProcessor.cpp          Mixes input to mono, runs the tracker, places MIDI events and drives the synth
├── tests/
│   ├── CMakeLists.txt                   Registers the tests with ctest
│   ├── engine_tests.cpp                 Unit tests: YIN, synth tuning, single notes, block-size invariance, bends, I/O
│   ├── plugin_tests.cpp                 Runs the real plugin processor like a host; also simulates Live's side-chain echo
│   └── vst3_host_tests.cpp              Loads the built morf.vst3 through the VST3 API like a DAW and plays guitar into it
├── scripts/
│   ├── install-mac.sh                   Installs the built VST3, AU and app on a Mac (re-signs, clears quarantine)
│   └── install-windows.ps1              Installs the built VST3 and app on a Windows PC
└── docs/
    ├── daw-setup.md                     Building, installing and using morf in Ableton, Logic and the standalone app, on Mac and Windows
    ├── dsp-primer.md                    Plain-language guide to the DSP behind the tracker
    ├── polyphonic-research.md           Options for chord (polyphonic) tracking in step 3
    └── results.md                       Step 1 accuracy and latency results, with the tuning log
```
