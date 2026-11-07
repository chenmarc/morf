#include "morf/fft.h"

#include <cmath>
#include <stdexcept>
#include <utility>

namespace morf {

FFT::FFT(int size) : n_(size), bitrev_(size), twiddle_(size / 2) {
    if (size < 2 || (size & (size - 1)) != 0) throw std::invalid_argument("FFT size must be a power of two");
    int bits = 0;
    while ((1 << bits) < size) ++bits;
    for (int i = 0; i < size; ++i) {
        int r = 0;
        for (int b = 0; b < bits; ++b)
            if (i & (1 << b)) r |= 1 << (bits - 1 - b);
        bitrev_[i] = r;
    }
    const double twoPi = 6.283185307179586;
    for (int k = 0; k < size / 2; ++k)
        twiddle_[k] = std::complex<float>(float(std::cos(twoPi * k / size)), float(-std::sin(twoPi * k / size)));
}

void FFT::transform(std::complex<float>* x, bool inverse) const {
    for (int i = 0; i < n_; ++i)
        if (i < bitrev_[i]) std::swap(x[i], x[bitrev_[i]]);
    for (int len = 2; len <= n_; len <<= 1) {
        const int half = len >> 1;
        const int step = n_ / len;
        for (int start = 0; start < n_; start += len) {
            for (int k = 0; k < half; ++k) {
                std::complex<float> w = twiddle_[k * step];
                if (inverse) w = std::conj(w);
                const std::complex<float> t = w * x[start + k + half];
                x[start + k + half] = x[start + k] - t;
                x[start + k] += t;
            }
        }
    }
}

}  // namespace morf
