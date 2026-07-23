#pragma once

#include <QThread>
#include <memory>
#include <vector>
#include "Frequency.h"

class ISdrDevice;
class ChannelProcessor;

// One-shot helper used by the "Auto Tune" button in the add/edit frequency
// dialog: briefly tunes the SDR to a single frequency, measures the noise
// floor over a short capture using the same channelizer/filter chain as
// real scanning, and reports back a suggested fixed squelch threshold.
//
// This exists as an alternative to live auto-squelch: a one-time
// measurement produces a fixed dB value stored on the Frequency, so it
// doesn't keep adapting while scanning.
class SquelchCalibrator : public QThread {
    Q_OBJECT
public:
    explicit SquelchCalibrator(QObject *parent = nullptr);
    // Out-of-line so ChannelProcessor only needs to be a complete type in
    // SquelchCalibrator.cpp, not everywhere this header is included.
    ~SquelchCalibrator() override;

    // device must already be open; caller must ensure nothing else (i.e.
    // no ScanEngine) is using it concurrently.
    void configure(ISdrDevice *device, qint64 freqHz, Modulation modulation);

signals:
    void calibrationFinished(bool ok, double noiseFloorDb, double suggestedSquelchDb, QString error);

protected:
    void run() override;

private:
    ISdrDevice *m_device = nullptr;
    qint64 m_freqHz = 0;
    Modulation m_modulation = Modulation::NFM;

    // Measurement state, deliberately members rather than run()-local
    // stack variables: the streaming callback below captures [this], not
    // [&], specifically so that if librtlsdr's Windows backend ever
    // delivers a transfer completion after startStreaming() has already
    // returned (its documented "incomplete concurrency implementation" --
    // see the comment in ScanEngine::requestStop() for the same issue),
    // it lands on still-valid heap-owned object state instead of a freed
    // stack frame.
    std::unique_ptr<ChannelProcessor> m_proc;
    std::vector<float> m_scratch;
    int m_blocksReceived = 0;
    double m_sumDb = 0.0;
};
