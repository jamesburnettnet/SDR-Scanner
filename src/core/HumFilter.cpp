#include "HumFilter.h"
#include <cmath>

namespace {
// Narrow enough to leave voice content (roughly 300Hz+) untouched, deep
// enough to meaningfully knock down a hum tone at 50/60/100/120/150/180Hz.
constexpr double kQ = 12.0;
}

void HumFilter::Biquad::design(double sampleRateHz, double notchHz, double q)
{
    const double w0 = 2.0 * M_PI * notchHz / sampleRateHz;
    const double alpha = std::sin(w0) / (2.0 * q);
    const double cosw0 = std::cos(w0);

    const double a0 = 1.0 + alpha;
    b0 = static_cast<float>(1.0 / a0);
    b1 = static_cast<float>(-2.0 * cosw0 / a0);
    b2 = static_cast<float>(1.0 / a0);
    a1 = static_cast<float>(-2.0 * cosw0 / a0);
    a2 = static_cast<float>((1.0 - alpha) / a0);
}

float HumFilter::Biquad::processSample(float x)
{
    // Direct Form II Transposed: numerically stable, standard biquad form.
    const float y = b0 * x + z1;
    z1 = b1 * x - a1 * y + z2;
    z2 = b2 * x - a2 * y;
    return y;
}

void HumFilter::Biquad::reset()
{
    z1 = 0.0f;
    z2 = 0.0f;
}

void HumFilter::configure(double sampleRateHz, double fundamentalHz)
{
    for (int i = 0; i < static_cast<int>(m_notches.size()); ++i)
        m_notches[i].design(sampleRateHz, fundamentalHz * (i + 1), kQ);
}

void HumFilter::process(std::vector<float> &samples)
{
    for (float &s : samples) {
        float v = s;
        for (auto &notch : m_notches)
            v = notch.processSample(v);
        s = v;
    }
}

void HumFilter::reset()
{
    for (auto &notch : m_notches)
        notch.reset();
}
