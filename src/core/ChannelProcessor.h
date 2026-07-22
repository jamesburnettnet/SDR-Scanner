#pragma once

#include <complex>
#include <vector>
#include <memory>
#include "Decimator.h"
#include "Squelch.h"
#include "Frequency.h"

// Per-channel DSP chain: NCO mix down from the group's wideband capture,
// two-stage decimate to 48kHz, measure power for squelch, and (optionally)
// demodulate to audio. One instance exists per scanned frequency that is
// currently a member of the active capture group.
class ChannelProcessor {
public:
    // deviceSampleRateHz: the wideband capture rate (2.4Msps).
    // offsetHz: this channel's frequency minus the group's tuned center.
    // stage1Coeffs: shared first-decimation filter (same for every channel).
    ChannelProcessor(qint64 deviceSampleRateHz, qint64 offsetHz, Modulation modulation,
                      std::shared_ptr<const std::vector<float>> stage1Coeffs);

    // Shared first-decimation-stage filter design (same for every channel
    // regardless of offset/modulation -- see the .cpp for why). Callers
    // that build multiple ChannelProcessors (ScanEngine, SquelchCalibrator)
    // should build this once and pass it to every instance.
    static std::shared_ptr<const std::vector<float>> buildStage1Coeffs();

    void setModulation(Modulation modulation);
    void setSquelchManual(double thresholdDb);
    void setSquelchAuto(double marginDb = 10.0);
    void reset();

    // Processes one wideband IQ block. If wantAudio is true, demodulated
    // audio samples (48kHz, float, roughly -1..1) are appended to
    // audioOut. Power/squelch are always evaluated.
    void process(const std::complex<float> *iq, size_t count, bool wantAudio,
                 std::vector<float> &audioOut);

    double lastPowerDb() const { return m_lastPowerDb; }
    bool squelchOpen() const { return m_squelch.isOpen(); }
    double squelchThresholdDb() const { return m_squelch.thresholdDb(); }
    Modulation modulation() const { return m_modulation; }
    qint64 offsetHz() const { return m_offsetHz; }

private:
    void demodulate(const std::complex<float> *samples, size_t count, std::vector<float> &audioOut);

    qint64 m_deviceSampleRateHz;
    qint64 m_offsetHz;
    Modulation m_modulation;

    double m_ncoPhase = 0.0;
    double m_ncoDeltaPhase = 0.0;

    Decimator m_stage1; // wideband -> 240kHz (decimate by 10)
    Decimator m_stage2; // 240kHz -> 48kHz (decimate by 5), mode-dependent cutoff

    Squelch m_squelch;
    double m_lastPowerDb = -120.0;

    // FM discriminator state
    std::complex<float> m_prevSample{0.0f, 0.0f};
    // AM DC removal state
    float m_amDc = 0.0f;

    std::vector<std::complex<float>> m_mixBuf;
    std::vector<std::complex<float>> m_stage1Buf;
    std::vector<std::complex<float>> m_stage2Buf;
};
