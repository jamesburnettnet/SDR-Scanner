#pragma once

#include <QThread>
#include "Frequency.h"

class ISdrDevice;

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
};
