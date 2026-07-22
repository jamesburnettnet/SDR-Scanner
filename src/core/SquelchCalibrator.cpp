#include "SquelchCalibrator.h"
#include "ChannelProcessor.h"
#include "ScanGroup.h"
#include "../sdr/ISdrDevice.h"

namespace {
// ~16384 samples per callback at 2.4Msps => ~6.83ms per block; ~60 blocks
// is a ~410ms measurement window, long enough to average out short blips.
constexpr int kCalibrationBlocks = 60;
constexpr double kMarginDb = 10.0; // same margin auto-squelch uses above the noise floor
}

SquelchCalibrator::SquelchCalibrator(QObject *parent)
    : QThread(parent)
{
}

void SquelchCalibrator::configure(ISdrDevice *device, qint64 freqHz, Modulation modulation)
{
    m_device = device;
    m_freqHz = freqHz;
    m_modulation = modulation;
}

void SquelchCalibrator::run()
{
    if (!m_device || !m_device->isOpen()) {
        emit calibrationFinished(false, 0.0, 0.0, QStringLiteral("No SDR device open"));
        return;
    }

    if (!m_device->setSampleRate(static_cast<uint32_t>(ScanGrouping::kDeviceSampleRateHz))) {
        emit calibrationFinished(false, 0.0, 0.0,
                                  QStringLiteral("Failed to set sample rate: %1").arg(m_device->lastError()));
        return;
    }
    m_device->setCenterFrequency(static_cast<uint64_t>(m_freqHz));
    m_device->setGainMode(true);

    auto stage1 = ChannelProcessor::buildStage1Coeffs();
    ChannelProcessor proc(ScanGrouping::kDeviceSampleRateHz, 0, m_modulation, stage1);
    // Manual, effectively-never-open threshold: we only read lastPowerDb(),
    // squelch state itself is irrelevant here.
    proc.setSquelchManual(0.0);

    int blocksReceived = 0;
    double sumDb = 0.0;
    std::vector<float> scratch;

    m_device->startStreaming([&](const std::complex<float> *samples, size_t count) {
        if (blocksReceived >= kCalibrationBlocks)
            return;
        proc.process(samples, count, false, scratch);
        sumDb += proc.lastPowerDb();
        ++blocksReceived;
        if (blocksReceived >= kCalibrationBlocks)
            m_device->stopStreaming();
    });

    if (blocksReceived == 0) {
        emit calibrationFinished(false, 0.0, 0.0, QStringLiteral("No samples captured"));
        return;
    }

    const double noiseFloorDb = sumDb / blocksReceived;
    emit calibrationFinished(true, noiseFloorDb, noiseFloorDb + kMarginDb, QString());
}
