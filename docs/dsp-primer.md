# DSP primer for morf

A short, practical guide to the ideas this codebase uses. It assumes you know C++ well and no signal processing. Each section points at the code that implements it.

## 1. Pitch is periodicity

A plucked string repeats the same waveform shape over and over. The time for one repeat is the **period** `P` (in samples), and the pitch is `f0 = sampleRate / P`. At 48 kHz, the low E string (82.4 Hz) repeats every 583 samples (12.1 ms); the high E at the 12th fret (659 Hz) every 73 samples (1.5 ms).

That's the root of the latency problem: **you can't know the period until you've seen it repeat at least once or twice.** For the low E that's roughly 24 ms of audio no matter how clever the algorithm. Every guitar-to-MIDI product lives with this floor. The tricks are about getting close to it and guessing well before it.

## 2. Finding the period: YIN (`core/src/yin.cpp`)

Take a chunk of audio and compare it with itself shifted by a lag `tau`. If `tau` equals the period, the shifted copy lines up and the difference is small:

```
d(tau) = sum over j of (x[j] - x[j + tau])^2
```

YIN (de Cheveigné & Kawahara, 2002) adds two steps:

1. **Cumulative mean normalisation**: `d'(tau) = d(tau) * tau / (d(1) + ... + d(tau))`. That turns the raw difference into a score where ~0 means "perfect repeat" and ~1 means "noise". We call this score **aperiodicity** and use it as our confidence.
2. **Absolute threshold**: take the *first* lag where `d'` dips below 0.15, not the global minimum. Lags of 2P, 3P... also line up perfectly, so the global minimum often sits at a multiple of the period, which would put the note an octave or more too low.

Then **parabolic interpolation** fits a parabola through the dip and its neighbours to get the period to a fraction of a sample. Without it, a high note would be quantised to whole-sample periods (about 20 cents of error at 1 kHz).

**Making it fast.** Computing `d(tau)` directly costs `W × tauMax` multiply-adds (~450k per frame). Expanding the square gives `d(tau) = energy(a) + energy(b, tau) − 2·r(tau)`, where `r` is a cross-correlation. A correlation can be computed with FFTs (`core/src/fft.cpp`): transform both signals, multiply one by the conjugate of the other, transform back. That's O(N log N). The energy terms come from a running sum of squares.

## 3. Octave errors

The most common pitch-tracking failure is reporting the right note in the wrong octave.

- **Too high** (the 2nd harmonic): when the fundamental is weak, the dip at P/2 can scrape under the threshold. Fix in `yin.cpp`: if the dip at twice the chosen lag is several times deeper, take it.
- **Too low** (a sub-harmonic): during a pick attack the true dip can sit just above the threshold while 2P dips below. Fix: if a dip at half or a third of the chosen lag is nearly as good, take the shorter one.
- **In the tracker**: an octave jump in the middle of a note has to persist for ~10 ms before we believe it.

## 4. Latency: only look at the new note (`core/src/mono_tracker.cpp`)

The analysis buffer holds 28 ms of audio (enough for two periods of the lowest note). Right after a pick, most of that buffer is the *previous* note or silence, which drags the estimate. So the tracker detects the **pick attack** (onset) and analyses only the audio since then. The window grows hop by hop (every 64 samples = 1.3 ms) until it's long enough. High notes get detected after ~3 periods (a few ms). Low notes still need the full buffer.

The onset detector watches the energy of the *first difference* `x[n] − x[n−1]`. That's a crude high-pass filter, which exaggerates the bright click of a pick and ignores slow level changes.

There's a catch with short windows. If the window holds only 1.5 periods of a low note, it can't tell the note from its own 2nd harmonic. So a lag is only accepted if the window is at least 3× that lag. That guard is what costs the low strings their extra latency.

## 5. Bends and vibrato

Once a note is playing we know its period, so we re-measure pitch over a short window of ~4 periods. A long window averages the pitch over 28 ms and lags behind a fast bend. The difference from the note's nominal pitch is sent as **MIDI pitch bend** (14-bit, ±12 semitones by default, so ~0.15 cent resolution).

To tell a **bend** from a **new note without a pick** (hammer-on, pull-off): a bend moves continuously (<0.05 semitones per 1.3 ms frame even when fast); a hammer-on jumps. A jump over 0.35 semitones that holds for 2 frames is treated as a new note.

## 6. Note endings and mutes

A note ends when:
- its level drops below the noise gate (−60 dBFS) or 45 dB below its own peak;
- the level falls more than 12 dB within 15 ms. A ringing string never decays that fast, so this means a hand or palm muted it. After a mute we wait for a fresh pick, so the decaying remains don't retrigger;
- the pitch stays unreadable for 40 ms.

## 7. Real-time rules

The audio callback can never wait, so the engine (`core/`):
- allocates all memory up front (constructor), never in `process()`;
- takes no locks, does no file I/O or logging;
- produces identical output whatever the host's buffer size is. That's checked by `testBlockSizeInvariance`. The engine runs its own fixed 64-sample hop internally.

## Glossary

| Term | Meaning |
|---|---|
| Sample rate | Samples per second (48 000 here) |
| Hop | How often we analyse: every 64 samples (1.33 ms) |
| Window | The stretch of audio one analysis looks at |
| dBFS | Level relative to digital full scale; 0 is the loudest possible, −60 is quiet |
| Cent | 1/100 of a semitone. Most people can't hear errors under ~5 cents |
| Onset | The start of a note (the pick attack) |
| F1 | Harmonic mean of precision (how many reported notes were real) and recall (how many real notes were reported) |
| MPE | MIDI Polyphonic Expression: one MIDI channel per note, so each note can bend on its own. Needed for polyphonic bends in step 3 |
