#include "FirDesign.h"
#include <cmath>

namespace FirDesign {

std::vector<float> lowpass(int numTaps, double cutoffNormalized)
{
    if (numTaps < 3)
        numTaps = 3;
    if (numTaps % 2 == 0)
        numTaps += 1;

    if (cutoffNormalized <= 0.0)
        cutoffNormalized = 0.001;
    if (cutoffNormalized >= 1.0)
        cutoffNormalized = 0.999;

    std::vector<float> taps(numTaps);
    const int M = numTaps - 1;
    const double fc = cutoffNormalized; // fraction of Nyquist

    double sum = 0.0;
    for (int n = 0; n < numTaps; ++n) {
        const double m = n - M / 2.0;
        double h;
        if (std::abs(m) < 1e-9) {
            h = fc;
        } else {
            h = fc * std::sin(M_PI * fc * m) / (M_PI * fc * m);
        }
        // Hamming window
        const double w = 0.54 - 0.46 * std::cos(2.0 * M_PI * n / M);
        h *= w;
        taps[n] = static_cast<float>(h);
        sum += h;
    }

    // Normalize for unity DC gain.
    if (sum != 0.0) {
        for (auto &t : taps)
            t = static_cast<float>(t / sum);
    }

    return taps;
}

} // namespace FirDesign
