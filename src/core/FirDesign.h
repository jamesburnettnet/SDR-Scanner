#pragma once

#include <vector>

// Windowed-sinc low-pass FIR filter design. Small, dependency-free
// replacement for a full DSP library since the scan engine only ever
// needs simple low-pass channel/decimation filters.
namespace FirDesign {

// Designs a Hamming-windowed-sinc low-pass filter.
// numTaps should be odd for a symmetric linear-phase filter.
// cutoffNormalized is the cutoff frequency divided by (sampleRate / 2),
// i.e. in the range (0, 1).
std::vector<float> lowpass(int numTaps, double cutoffNormalized);

} // namespace FirDesign
