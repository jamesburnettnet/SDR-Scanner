#pragma once

#include <QString>
#include <QUuid>
#include <QJsonObject>
#include <QMetaType>
#include <cstdint>

// Demodulation mode for a scanned frequency.
enum class Modulation {
    FM,   // Wide FM (e.g. broadcast / wide analog)
    NFM,  // Narrowband FM (typical land-mobile / public-safety analog)
    AM    // Amplitude modulation (aviation, etc.)
};

QString modulationToString(Modulation m);
Modulation modulationFromString(const QString &s);

// A single scannable frequency entry, as the user would enter on a
// traditional handheld/base scanner: a frequency, a label, a mode,
// and an optional squelch setting.
struct Frequency {
    QUuid id = QUuid::createUuid();
    QString label;
    qint64 hz = 0;          // center frequency in Hz
    Modulation modulation = Modulation::NFM;
    bool enabled = true;

    // Squelch: when autoSquelch is true, the scan engine derives a
    // threshold from the measured noise floor. Otherwise squelchDb is
    // an absolute power threshold in dBFS (roughly -100..0).
    bool autoSquelch = true;
    double squelchDb = -50.0;

    QString group;          // user-defined group / tag, e.g. "Police", "Aviation"

    QJsonObject toJson() const;
    static Frequency fromJson(const QJsonObject &obj);

    double mhz() const { return hz / 1'000'000.0; }
    void setMhz(double mhz) { hz = static_cast<qint64>(mhz * 1'000'000.0 + (mhz >= 0 ? 0.5 : -0.5)); }
};

Q_DECLARE_METATYPE(Modulation)
