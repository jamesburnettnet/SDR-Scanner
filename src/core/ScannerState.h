#pragma once

#include <QString>
#include <QMutex>
#include <cstdint>
#include "Frequency.h"

enum class EngineMode {
    Idle,
    Scanning,   // cycling through groups, nothing squelch-open
    Holding,    // squelch open on a channel, playing audio
    Exploring
};

// Lightweight, cheap-to-copy snapshot of what the scan engine is currently
// doing. Published frequently by the (high-rate) scan thread and read at a
// throttled rate by the GUI's LCD repaint timer, so the LCD never blocks
// or paces the actual scanning/demod work.
struct ScannerSnapshot {
    EngineMode mode = EngineMode::Idle;
    bool deviceConnected = false;
    QString deviceName;

    qint64 currentFreqHz = 0;
    QString currentLabel;
    Modulation currentModulation = Modulation::NFM;

    // Non-null only while actively holding on a transmission (Holding
    // mode); used to highlight the corresponding row in the frequency
    // table. Deliberately not set while idle-scanning/display-cursor
    // rotating, since that's cosmetic, not a real "active call".
    QUuid activeFrequencyId;

    double signalDb = -120.0;   // last measured power, dBFS
    double squelchThresholdDb = -50.0;
    bool squelchOpen = false;

    int groupIndex = 0;
    int groupCount = 0;
    int channelIndex = 0;
    int channelCount = 0;

    quint64 updateCounter = 0; // increments on every publish; lets the LCD detect staleness
};

// Thread-safe double-buffer-ish holder: writer (scan thread) calls publish()
// often; reader (GUI timer) calls read() at its own pace. A small mutex
// critical section is cheap enough not to perturb scan timing.
class ScannerStateHolder {
public:
    void publish(const ScannerSnapshot &snap)
    {
        QMutexLocker lock(&m_mutex);
        m_snapshot = snap;
        m_snapshot.updateCounter = ++m_counter;
    }

    ScannerSnapshot read() const
    {
        QMutexLocker lock(&m_mutex);
        return m_snapshot;
    }

private:
    mutable QMutex m_mutex;
    ScannerSnapshot m_snapshot;
    quint64 m_counter = 0;
};
