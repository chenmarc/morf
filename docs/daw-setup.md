# Using morf in your DAW

morf is an **instrument plugin with an audio input**. It listens to the guitar, sends MIDI out and has a simple built-in synth so you can hear the tracking straight away. Turn the synth off once your own instrument is wired up.

## Build and install (macOS)

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DMORF_BUILD_PLUGIN=ON
cmake --build build -j
cp -R build/plugin/morf_plugin_artefacts/Release/AU/morf.component ~/Library/Audio/Plug-Ins/Components/
cp -R build/plugin/morf_plugin_artefacts/Release/VST3/morf.vst3   ~/Library/Audio/Plug-Ins/VST3/
open build/plugin/morf_plugin_artefacts/Release/Standalone/morf.app
```

The first configure downloads JUCE 8, which takes a few minutes. Every push also builds unsigned plugins on GitHub Actions (the **build** workflow, under "Artifacts").

## Ableton Live (VST3)

1. On your guitar's **audio track**, set Monitor to *In* and drop **morf** onto it.
2. Make a **MIDI track** with your synth. Set *MIDI From* to the guitar track, then choose **morf** in the second dropdown. Set Monitor to *In*.
3. Turn morf's *Monitor synth* off so you only hear your synth.
4. Set the synth's pitch-bend range to match morf's (default ±12). morf also sends this as RPN 0, which many synths obey automatically.

## Logic Pro (AU)

1. Create a **Software Instrument** track and load **morf** (AU Instruments → morf → morf).
2. In the plugin header, set **Side Chain** to your guitar's audio input or audio track.
3. You'll hear the monitor synth right away. To drive another instrument, either:
   - set that instrument track's MIDI input to the morf track (Logic 10.5+ *Track → MIDI input*), or
   - run the standalone app and select the **morf** virtual MIDI port as an input.

## Standalone app

Pick your audio interface and the guitar input in *Options → Audio/MIDI Settings*. The app publishes a virtual MIDI port named **morf**, which shows up in any DAW or synth as a MIDI input.

## Controls

| Control | What it does |
|---|---|
| Input gain | Boosts or cuts the guitar before tracking. Aim for peaks around −12 dBFS on the meter |
| Note threshold | Minimum level for a note to start (the orange line on the meter). Raise it if noise triggers notes; lower it for a light touch |
| Pitch-bend range | Must match your synth. ±12 is a good default; ±48 matches MPE synths |
| MIDI channel | Output channel |
| Monitor synth / Synth level | Built-in synth so you can hear the tracking |
| Guitar level | Mixes the dry guitar into the output |

## Latency

The plugin places every MIDI event at the exact sample where it was detected, so it adds nothing beyond tracking time and your interface's buffer. For the lowest latency, set the interface buffer to 64 or 128 samples.

## Licensing note

JUCE 8 is dual-licensed: AGPLv3, or a commercial licence. The free Starter tier covers revenue under $50k a year. To sell morf closed-source you'll need a JUCE licence, or you can open-source morf under AGPL.
