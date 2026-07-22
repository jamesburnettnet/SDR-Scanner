#include "ChannelProcessor.h"
#include "FirDesign.h"
#include "ScanGroup.h"
#include <cmath>
#include <algorithm>

namespace {
constexpr int kStage1Decimation = 10;
constexpr int kStage2Decimation = 5;
constexpr double kStage1RateHz = ScanGrouping::kDeviceSampleRateHz / kStage1Decimation; // 240kHz
constexpr double kChannelRateHz = kStage1RateHz / kStage2Decimation;                    // 48kHz

constexpr int kStage1Taps = 81;
constexpr double kStage1CutoffHz = 100000.0;

constexpr float kNfmGain = 3.2f;
constexpr float kFmGain = 1.6f;
constexpr float kAmGain = 4.0f;
constexpr float kAmDcAlpha = 0.01f;

std::vector<float> stage2CoeffsForMode(Modulation m)
{
    // Cutoff expressed as a fraction of stage2's Nyquist (kStage1RateHz/2 = 120kHz).
    double cutoffHz;
    switch (m) {
        case Modulation::NFM: cutoffHz = 6250.0; break;   // ~12.5kHz channel
        case Modulation::FM:  cutoffHz = 20000.0; break;  // wideband, best effort at this rate
        case Modulation::AM:  cutoffHz = 5000.0; break;
    }
    const double nyquist = kStage1RateHz / 2.0;
    return FirDesign::lowpass(103, cutoffHz / nyquist);
}
} // namespace

std::shared_ptr<const std::vector<float>> ChannelProcessor::buildStage1Coeffs()
{
    return std::make_shared<std::vector<float>>(
        FirDesign::lowpass(kStage1Taps, kStage1CutoffHz / (ScanGrouping::kDeviceSampleRateHz / 2.0)));
}

ChannelProcessor::ChannelProcessor(qint64 deviceSampleRateHz, qint64 offsetHz, Modulation modulation,
                                    std::shared_ptr<const std::vector<float>> stage1Coeffs)
    : m_deviceSampleRateHz(deviceSampleRateHz)
    , m_offsetHz(offsetHz)
    , m_modulation(modulation)
    , m_stage1(*stage1Coeffs, kStage1Decimation)
    , m_stage2(stage2CoeffsForMode(modulation), kStage2Decimation)
{
    m_ncoDeltaPhase = -2.0 * M_PI * static_cast<double>(offsetHz) / static_cast<double>(deviceSampleRateHz);
    m_squelch.setAuto();
}

void ChannelProcessor::setModulation(Modulation modulation)
{
    if (m_modulation == modulation)
        return;
    m_modulation = modulation;
    m_stage2 = Decimator(stage2CoeffsForMode(modulation), kStage2Decimation);
}

void ChannelProcessor::setSquelchManual(double thresholdDb)
{
    m_squelch.setManual(thresholdDb);
}

void ChannelProcessor::setSquelchAuto(double marginDb)
{
    m_squelch.setAuto(marginDb);
}

void ChannelProcessor::reset()
{
    m_ncoPhase = 0.0;
    m_stage1.reset();
    m_stage2.reset();
    m_squelch.reset();
    m_prevSample = {0.0f, 0.0f};
    m_amDc = 0.0f;
}

void ChannelProcessor::process(const std::complex<float> *iq, size_t count, bool wantAudio,
                                std::vector<float> &audioOut)
{
    if (count == 0)
        return;

    m_mixBuf.resize(count);
    for (size_t n = 0; n < count; ++n) {
        const std::complex<float> lo(static_cast<float>(std::cos(m_ncoPhase)),
                                      static_cast<float>(std::sin(m_ncoPhase)));
        m_mixBuf[n] = iq[n] * lo;
        m_ncoPhase += m_ncoDeltaPhase;
        if (m_ncoPhase > M_PI) m_ncoPhase -= 2.0 * M_PI;
        else if (m_ncoPhase < -M_PI) m_ncoPhase += 2.0 * M_PI;
    }

    m_stage1Buf.clear();
    m_stage1.process(m_mixBuf.data(), m_mixBuf.size(), m_stage1Buf);
    if (m_stage1Buf.empty())
        return;

    m_stage2Buf.clear();
    m_stage2.process(m_stage1Buf.data(), m_stage1Buf.size(), m_stage2Buf);
    if (m_stage2Buf.empty())
        return;

    double sumPower = 0.0;
    for (const auto &s : m_stage2Buf)
        sumPower += static_cast<double>(s.real()) * s.real() + static_cast<double>(s.imag()) * s.imag();
    const double meanPower = sumPower / static_cast<double>(m_stage2Buf.size());
    m_lastPowerDb = 10.0 * std::log10(std::max(meanPower, 1e-12));

    m_squelch.update(m_lastPowerDb);

    if (wantAudio)
        demodulate(m_stage2Buf.data(), m_stage2Buf.size(), audioOut);
}

void ChannelProcessor::demodulate(const std::complex<float> *samples, size_t count, std::vector<float> &audioOut)
{
    audioOut.reserve(audioOut.size() + count);

    if (m_modulation == Modulation::AM) {
        for (size_t n = 0; n < count; ++n) {
            const float mag = std::sqrt(samples[n].real() * samples[n].real() +
                                         samples[n].imag() * samples[n].imag());
            m_amDc += kAmDcAlpha * (mag - m_amDc);
            float out = (mag - m_amDc) * kAmGain;
            out = std::clamp(out, -1.0f, 1.0f);
            audioOut.push_back(out);
        }
        return;
    }

    // FM / NFM: polar discriminator (arctan of conjugate product with the
    // previous sample), matching the standard scalar FM demod approach.
    const float gain = (m_modulation == Modulation::NFM) ? kNfmGain : kFmGain;
    for (size_t n = 0; n < count; ++n) {
        const std::complex<float> cur = samples[n];
        const std::complex<float> prod = cur * std::conj(m_prevSample);
        float freq = std::atan2(prod.imag(), prod.real());
        m_prevSample = cur;
        float out = (freq / static_cast<float>(M_PI)) * gain;
        out = std::clamp(out, -1.0f, 1.0f);
        audioOut.push_back(out);
    }
}
