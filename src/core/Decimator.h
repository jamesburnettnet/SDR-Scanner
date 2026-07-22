#pragma once

#include <complex>
#include <vector>

// Streaming FIR low-pass filter + integer decimator for complex baseband
// samples. Coefficients are shared (passed in by reference at construction)
// since many channels in a group reuse the same filter design; history is
// per-instance so each channel keeps continuity across processing blocks.
class Decimator {
public:
    Decimator(std::vector<float> coeffs, int decimation);

    void reset();

    // Filters + decimates `in`, appending results to `out` (out is not
    // cleared first, so callers can chain/reuse a buffer).
    void process(const std::complex<float> *in, size_t count, std::vector<std::complex<float>> &out);

    int decimation() const { return m_decimation; }
    size_t numTaps() const { return m_coeffs.size(); }

private:
    std::vector<float> m_coeffs;
    int m_decimation;
    std::vector<std::complex<float>> m_history; // ring buffer, size == numTaps
    size_t m_historyPos = 0;
    int m_samplesSinceOutput = 0;
};
