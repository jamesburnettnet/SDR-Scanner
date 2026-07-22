#include "Decimator.h"

Decimator::Decimator(std::vector<float> coeffs, int decimation)
    : m_coeffs(std::move(coeffs))
    , m_decimation(decimation > 0 ? decimation : 1)
{
    m_history.assign(m_coeffs.size(), std::complex<float>(0.0f, 0.0f));
}

void Decimator::reset()
{
    std::fill(m_history.begin(), m_history.end(), std::complex<float>(0.0f, 0.0f));
    m_historyPos = 0;
    m_samplesSinceOutput = 0;
}

void Decimator::process(const std::complex<float> *in, size_t count, std::vector<std::complex<float>> &out)
{
    const size_t taps = m_coeffs.size();
    if (taps == 0)
        return;

    for (size_t n = 0; n < count; ++n) {
        m_history[m_historyPos] = in[n];
        m_historyPos = (m_historyPos + 1) % taps;

        ++m_samplesSinceOutput;
        if (m_samplesSinceOutput < m_decimation)
            continue;
        m_samplesSinceOutput = 0;

        // Dot product of coefficients with history, oldest-to-newest.
        // m_historyPos currently points at the oldest sample (the one
        // about to be overwritten next), which lines up taps[0] with the
        // oldest sample -- a standard causal FIR convolution.
        std::complex<float> acc(0.0f, 0.0f);
        size_t idx = m_historyPos;
        for (size_t t = 0; t < taps; ++t) {
            acc += m_history[idx] * m_coeffs[t];
            idx = (idx + 1) % taps;
        }
        out.push_back(acc);
    }
}
