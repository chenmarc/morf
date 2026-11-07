# Step 1 results: single-note baseline

Tracker: `mono-yin` (YIN pitch detection, onset-restarted analysis window, bend/legato/mute state machine).
Test audio: synthetic guitar (physical string model), 48 kHz, host buffer 128. Each run is reproducible with `morf_eval --seed N`.

Parameters were tuned on seed 7. The numbers below come from **seed 1234, which wasn't used for tuning** (10 files per category, 1040 single-note events plus 232 chord notes).

| Category | Notes | Precision | Recall | F1 | Latency median | Latency p90 | Pitch error median | Within 10 cents |
|---|---|---|---|---|---|---|---|---|
| Single notes | 120 | 0.99 | 1.00 | 1.00 | 13.7 ms | 28.2 ms | 0.6 c | 99% |
| Legato lines | 240 | 0.99 | 1.00 | 0.99 | 17.1 ms | 28.7 ms | 1.2 c | 90% |
| Repeated notes | 240 | 0.98 | 1.00 | 0.99 | 20.9 ms | 36.4 ms | 0.8 c | 97% |
| Bends | 60 | 0.98 | 1.00 | 0.99 | 12.1 ms | 18.2 ms | 2.1 c | 81% |
| Vibrato | 60 | 1.00 | 1.00 | 1.00 | 11.7 ms | 20.3 ms | 3.3 c | 85% |
| Palm mutes (low strings) | 320 | 0.98 | 1.00 | 0.99 | 28.8 ms | 30.3 ms | 1.6 c | 98% |
| **All single-note** | 1040 | 0.99 | 1.00 | 0.99 | 21.9 ms | 29.9 ms | 1.1 c | 91% |
| Chords (out of scope for mono) | 232 | 0.24 | 0.12 | 0.16 | – | – | – | – |

Latency by register:

| Register | Median | p90 |
|---|---|---|
| Low (E2–E3) | 28.8 ms | 36.2 ms |
| Mid (F3–G#4) | 16.7 ms | 22.6 ms |
| High (A4+) | 11.2 ms | 18.1 ms |

Other checks:
- Output is bit-identical for host buffers of 1, 32, 64, 128, 333, 512 and 1024 samples.
- 44.1 kHz and 96 kHz give the same accuracy (F1 0.99–1.00).
- CPU: about 5% of one core.
- Octave errors: 6 in 1040 notes.

Latency is measured from the true start of the note to the Note On. It doesn't include the audio interface's own buffer latency, which every product pays equally.

## How we got here (tuning log, seed 7)

| Change | F1 | Median latency | Octave errors |
|---|---|---|---|
| Plain YIN over a fixed 28 ms window | 0.94 | 28.0 ms | 17 |
| Analyse only the audio since the last pick attack | 0.92 | 28.2 ms | 75 |
| Accept noisy-but-stable pitch during the attack, plus a sub-harmonic check | 0.96 | 23.3 ms | 17 |
| Relative note-off gate; legato switches need a clean signal | 0.98 | 23.3 ms | 17 |
| Short (~4-period) window for following bends | 0.99 | 23.2 ms | 13 |
| Octave-up check, and octave jumps must hold for 10 ms | 0.99 | 23.2 ms | 11 |
| Looser sub-harmonic check | 0.99 | 22.7 ms | 2 |

The tuning was tracked by the bench, which is the point of building it first.

## What this tells us

1. **Single-note tracking is in good shape on clean signals.** It's accurate to about a cent, bends and vibrato are tracked without retriggering, and it barely uses any CPU.
2. **The low strings are the latency problem.** Median latency is about 29 ms on E2–E3 against about 11 ms above A4. A period-based method has to see roughly 2–3 periods of a 12 ms waveform. Getting below that needs a different idea:
   - an early guess from the attack's spectrum, corrected within a few ms;
   - a learned model that recognises the note from less than one period;
   - an optional hex pickup, where each string can be tuned to its own range.
3. **Bend tracking lags a little** (81–85% of frames within 10 cents). A ~4-period window still averages over ~15 ms on a mid note. Phase-based tracking would tighten this.
4. **Chords fail, as expected.** The bench now has a chord scoreboard for step 3 to beat.

## Caveat: synthetic audio only

GuitarSet (real recordings with per-string labels) is hosted on Zenodo, and this build environment can't reach it. Everything above is on a physical string model. It's realistic in the ways that matter for tracking: noisy attacks, a pitch glide after the pick, harmonics that decay at different rates, hum and noise. But real guitars add string coupling, body resonance, fret buzz and messy playing.

**Next validation steps:** download GuitarSet on your Mac, and record a few labelled clips of your own. `morf_eval --dir` scores any folder of `.wav` + `.csv` files. `morf_track` turns any recording into a `.mid` you can drop into Logic.
