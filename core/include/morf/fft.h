#pragma once
// Minimal in-place radix-2 complex FFT. All memory is allocated in the
// constructor so transform() is real-time safe (no allocation, no locks).
#include <complex>
#include <vector>

namespace morf {

class FFT {
public:
    explicit FFT(int size);  // size must be a power of two
    int size() const { return n_; }

    // In-place forward transform (sign -1). inverse() is unnormalised; divide by size().
    void forward(std::complex<float>* data) const { transform(data, false); }
    void inverse(std::complex<float>* data) const { transform(data, true); }

private:
    void transform(std::complex<float>* data, bool inverse) const;
    int n_;
    std::vector<int> bitrev_;
    std::vector<std::complex<float>> twiddle_;  // e^{-2*pi*i*k/n}, k < n/2
};

}  // namespace morf
