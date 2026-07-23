#include "SquelchCalibrator.h"
#include "ChannelProcessor.h"
#include "ScanGroup.h"
#include "DebugLog.h"
#include "../sdr/ISdrDevice.h"
#include <QThread>

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

SquelchCalibrator::~SquelchCalibrator() = default;

void SquelchCalibrator::configure(ISdrDevice *device, qint64 freqHz, Modulation modulation)
{
    m_device = device;
    m_freqHz = freqHz;
    m_modulation = modulation;
}

void SquelchCalibrator::run()
{
    SDR_LOG("calib") << "run() entering [thread" << QThread::currentThreadId() << "] freqHz=" << m_freqHz;
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
    m_proc = std::make_unique<ChannelProcessor>(ScanGrouping::kDeviceSampleRateHz, 0, m_modulation, stage1);
    // Manual, effectively-never-open threshold: we only read lastPowerDb(),
    // squelch state itself is irrelevant here.
    m_proc->setSquelchManual(0.0);

    m_blocksReceived = 0;
    m_sumDb = 0.0;
    m_scratch.clear();

    // Captures [this], not a reference to run()'s stack locals -- see the
    // comment on the member fields in SquelchCalibrator.h for why.
    m_device->startStreaming([this](const std::complex<float> *samples, size_t count) {
        if (m_blocksReceived >= kCalibrationBlocks)
            return;
        m_proc->process(samples, count, false, m_scratch);
        m_sumDb += m_proc->lastPowerDb();
        ++m_blocksReceived;
        if (m_blocksReceived >= kCalibrationBlocks)
            m_device->stopStreaming();
    });

    SDR_LOG("calib") << "run(): streaming ended, blocksReceived=" << m_blocksReceived;
    if (m_blocksReceived == 0) {
        emit calibrationFinished(false, 0.0, 0.0, QStringLiteral("No samples captured"));
        return;
    }

    const double noiseFloorDb = m_sumDb / m_blocksReceived;
    emit calibrationFinished(true, noiseFloorDb, noiseFloorDb + kMarginDb, QString());
}
