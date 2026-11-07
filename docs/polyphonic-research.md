# Polyphonic tracking: options for step 3

The goal: detect up to 6 simultaneous notes from one mixed guitar signal, with per-note bends, at low latency.

| Approach | How it works | Latency | Accuracy on guitar | Effort |
|---|---|---|---|---|
| Iterative estimate-and-cancel (Klapuri) | Find the strongest pitch in the spectrum, subtract its harmonics, repeat | Needs ~40–90 ms FFT windows to separate close notes | Fair; struggles with octaves and shared harmonics | Medium |
| NMF with guitar templates | Learn a spectral template per string/fret; decompose each frame as a sum of templates | Same window problem; real-time NMF is feasible | Good with templates fitted to the user's guitar (calibration step) | Medium |
| **Small neural network (recommended)** | CNN/CRNN on a short constant-Q or mel spectrogram outputs per-fret note activity, onsets and pitch offsets (like Spotify's Basic Pitch or TabCNN) | Can be trained for very short look-ahead; ~10–20 ms is plausible | Best published results on GuitarSet (frame F1 in the 0.8s for tab-style models) | High: needs data pipeline + training + real-time inference (RTNeural or ONNX Runtime) |
| Hex pickup | One mono tracker per string | Same as mono | Excellent | Requires hardware; good optional "pro" mode |

**Recommendation:** use a small neural model trained on GuitarSet plus synthetic data from our string model (unlimited labelled chords, bends and mutes), with the mono tracker kept as a fast path for single notes. Output MPE, one channel per string, so each note bends independently.

**Main risk:** getting the network small enough to run in under 1 ms per hop on a laptop CPU, at a latency close to the mono tracker. Step 3 should start with a latency-vs-accuracy sweep on the bench before committing.
