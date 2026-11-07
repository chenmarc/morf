#pragma once
#include "morf/events.h"

namespace morf {

// Common interface for every pitch-to-MIDI engine (the mono baseline now,
// the polyphonic engine in step 3). The plugin and the test bench only
// talk to this, so engines can be swapped and compared like for like.
class Tracker {
public:
    virtual ~Tracker() = default;
    virtual const char* name() const = 0;
    virtual void reset() = 0;
    // Consume n mono samples. Any block size works; results don't depend on it.
    // Emitted events carry absolute sample times. Must be real-time safe.
    virtual void process(const float* in, int n, EventQueue& out) = 0;
    // Samples of look-back the engine needs before it can report a note.
    virtual int analysisWindow() const = 0;
};

}  // namespace morf
