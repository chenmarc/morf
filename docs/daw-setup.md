# Using morf in your DAW

morf is an **instrument plugin with an audio input**. It listens to the guitar, sends MIDI out, and has a simple built-in synth so you can hear the tracking straight away. Turn the synth off once your own instrument is wired up.

It comes in three forms:

| Form | Platforms | Use it in |
|---|---|---|
| AU (`morf.component`) | macOS | Logic, GarageBand, Ableton Live |
| VST3 (`morf.vst3`) | macOS, Windows | Ableton Live, Reaper, Cubase, Bitwig and most other DAWs |
| Standalone app (`morf.app`, `morf.exe`) | macOS, Windows | On its own, sending MIDI to any DAW or synth |

## 1. Build

You need CMake 3.22+ and a C++17 compiler (Xcode on macOS, Visual Studio 2022 on Windows). The first configure downloads JUCE 8, which takes a few minutes.

```sh
cmake --preset default
cmake --build --preset default
```

`default` builds for the computer you're on: an Intel Mac gets Intel binaries, an Apple silicon Mac gets arm64, Windows gets 64-bit. To build one Mac version that runs on both Intel and Apple silicon, use the `mac-universal` preset instead (output goes to `build-universal/`).

The bundles end up in `build/plugin/morf_plugin_artefacts/Release/`.

### Or download a build

Every push builds morf on GitHub Actions. Open the **build** workflow run, scroll to *Artifacts*, and download:

- **morf-macos-universal**: runs on Intel and Apple silicon Macs. GitHub wraps it in a second zip, so unzip twice to get the `morf` folder.
- **morf-windows-x64**: 64-bit Windows.

These builds aren't signed with a developer certificate. The install scripts below remove the download-quarantine flag so macOS doesn't block them.

## 2. Install

### macOS

```sh
scripts/install-mac.sh                    # after building with the default preset
scripts/install-mac.sh build-universal    # after building with the mac-universal preset
./install-mac.sh .                        # inside a downloaded, unzipped morf folder
```

It asks for your password, then:

- copies the VST3 to `/Library/Audio/Plug-Ins/VST3`, the AU to `/Library/Audio/Plug-Ins/Components` and the app to `/Applications`. These are the system folders that Live and Logic scan by default;
- deletes any old copies in your personal `~/Library/Audio/Plug-Ins`, which would otherwise shadow the new ones;
- removes the quarantine flag, re-signs each bundle and checks the signature. A VST3 whose signature doesn't verify is skipped silently by Ableton Live 11, so the script stops if a signature fails;
- restarts macOS's Audio Unit registry so the AU shows up without a reboot.

### Windows

Open PowerShell with **Run as administrator**, go to the morf folder, and run:

```powershell
powershell -ExecutionPolicy Bypass -File scripts\install-windows.ps1              # after building
powershell -ExecutionPolicy Bypass -File install-windows.ps1 -BuildDir .          # inside a downloaded morf folder
```

It copies the VST3 to `C:\Program Files\Common Files\VST3` (the folder every Windows DAW scans) and the app to `C:\Program Files\morf`.

## 3. Ableton Live

After installing, **quit and reopen Live**, then go to *Preferences → Plug-ins*, make sure *Use VST3 Plug-in System Folders* is on (and, on a Mac, *Use Audio Units v2* if you want the AU), and **hold Option (Mac) or Alt (Windows) while clicking Rescan**. That forces a full rescan instead of reusing Live's cache.

The AU and the VST3 are wired up the same way. Use whichever one you like; on Windows only the VST3 exists.

1. **Guitar track.** Make an audio track, set *Audio From* to your interface input, and set Monitor to *In*. You'll usually want the dry guitar quiet: turn this track's volume down, or set *Audio To* to *Sends Only*.
2. **morf track.** Make a MIDI track and drop **morf** onto it (*Plug-ins → morf*). Set its Monitor to *In*.
3. **Feed the guitar into morf.** Expand the morf device with the small triangle in its title bar, switch **Sidechain** on, and choose the guitar track as the source. Pick *Pre FX* in the second chooser so morf hears the clean guitar, before any effects or the fader on that track.
4. Play. You should hear the monitor synth.

Without step 3, morf hears nothing and stays silent. That's expected: Live sends an unconnected side-chain the plugin's own output, and morf ignores it so it can't track itself.

### Playing another instrument from morf

- **From the plugin:** make another MIDI track with your instrument. Set its *MIDI From* to the morf track, then choose **morf** in the second chooser if Live offers it, and set Monitor to *In*. Then turn morf's *Monitor synth* off. Live can pass on MIDI from VST plugins but not from AUs, so use the VST3 for this. This route hasn't been tried in Live 11 yet; if morf isn't offered in the second chooser, use the standalone app instead.
- **From the standalone app:** keep the app running with your guitar input selected. On a Mac, set your instrument track's *MIDI From* to **morf**, its virtual MIDI port. On Windows, see [Standalone app](#5-standalone-app) for the extra step.

Set your instrument's pitch-bend range to match morf's (default ±12). morf also sends the range as RPN 0, which many synths pick up automatically.

## 4. Logic Pro (AU)

1. Create a **Software Instrument** track and load **morf** (*AU Instruments → morf → morf*).
2. In the plugin header, set **Side Chain** to your guitar's audio input or audio track.
3. You'll hear the monitor synth right away.
4. To drive another instrument, run the standalone app alongside Logic. Logic treats its **morf** virtual MIDI port like a MIDI keyboard, so selecting your instrument track and playing is enough. Turn the AU's monitor synth off, or remove the AU, so you don't hear both.

## 5. Standalone app

Open it from Applications (Mac) or `C:\Program Files\morf\morf.exe` (Windows). In *Options → Audio/MIDI Settings*, pick your audio interface and the input your guitar is plugged into. Use a buffer of 64 or 128 samples for the lowest latency.

- **macOS:** the app publishes a virtual MIDI port named **morf**, which any DAW or synth can use as a MIDI input. The line next to the morf name at the top of the window says when the port is active.
- **Windows:** Windows has no built-in virtual MIDI ports. Install the free [loopMIDI](https://www.tobias-erichsen.de/software/loopmidi.html), create a port in it, then choose that port as *MIDI Output* in morf's *Audio/MIDI Settings*. Your DAW then reads from the loopMIDI port.

The Windows app doesn't include ASIO drivers, so pick *Windows Audio (Low Latency Mode)* as the device type. Inside a DAW this doesn't matter, because the DAW handles the audio driver and morf runs as a VST3.

## Controls

| Control | What it does |
|---|---|
| Input gain | Boosts or cuts the guitar before tracking. Aim for peaks around −12 dBFS on the meter |
| Note threshold | Minimum level for a note to start (the orange line on the meter). Raise it if noise triggers notes; lower it for a light touch |
| Pitch-bend range | Must match your synth. ±12 is a good default; ±48 matches MPE synths |
| MIDI channel | Output channel |
| Monitor synth / Synth level | Built-in synth so you can hear the tracking |
| Guitar level | Mixes the dry guitar into the output |

## Troubleshooting

**The VST3 doesn't show up in Live (Mac).** In Terminal:

```sh
codesign --verify --deep --strict --verbose=2 /Library/Audio/Plug-Ins/VST3/morf.vst3
```

It should say *valid on disk* and *satisfies its Designated Requirement*. If it doesn't, run `scripts/install-mac.sh` again, which re-signs the bundle. Then quit Live and Option-click Rescan. Also check that no older copy is sitting in `~/Library/Audio/Plug-Ins/VST3`.

**The AU doesn't show up.** Run `auval -v aumu Mrf1 Mrfm`. It should end with *AU VALIDATION SUCCEEDED*. If Live still doesn't list it, check that *Use Audio Units v2* is on in Live's preferences, then Option-click Rescan.

**morf is silent in Live.** Check the side-chain from step 3 and that the guitar track's Monitor is *In*. The meter in morf's window should move when you play.

**Notes trigger on their own.** Raise *Note threshold*, or lower *Input gain*.

## Latency

The plugin places every MIDI event at the exact sample where it was detected, so it adds nothing beyond tracking time and your interface's buffer. For the lowest latency, set the interface buffer to 64 or 128 samples.

## Licensing note

JUCE 8 is dual-licensed: AGPLv3, or a commercial licence. The free Starter tier covers revenue under $50k a year. To sell morf closed-source you'll need a JUCE licence, or you can open-source morf under AGPL.
