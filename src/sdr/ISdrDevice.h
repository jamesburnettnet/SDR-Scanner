#pragma once

#include <QString>
#include <cstdint>
#include <functional>
#include <complex>

// Info about one enumerated (but not necessarily open) SDR device.
struct SdrDeviceInfo {
    int index = -1;
    QString name;        // human-readable, e.g. "Realtek RTL2838UHIDIR (Nooelec NESDR SMArt v5)"
    QString serial;
};

// Callback invoked from the device's read thread with a block of
// interleaved-then-converted complex samples, normalized to roughly
// -1..1. count is the number of complex samples (not raw bytes).
using SdrSampleCallback = std::function<void(const std::complex<float> *samples, size_t count)>;

// Minimal hardware-abstraction interface so the scan engine can work with
// any SDR backend. Only librtlsdr-based devices (RTL-SDR, Nooelec NESDR,
// which are RTL2832U-based and share the same driver) are implemented for
// now, but new backends (e.g. HackRF, SoapySDR) could implement this same
// interface without touching the scan engine.
class ISdrDevice {
public:
    virtual ~ISdrDevice() = default;

    virtual bool open(int deviceIndex) = 0;
    virtual void close() = 0;
    virtual bool isOpen() const = 0;

    virtual bool setSampleRate(uint32_t hz) = 0;
    virtual bool setCenterFrequency(uint64_t hz) = 0;
    virtual bool setGainMode(bool automatic) = 0;
    virtual bool setGain(int tenthDb) = 0; // used when gain mode is manual

    // Begins streaming; callback is invoked from an internal thread until
    // stopStreaming() is called. Blocks until streaming stops if the
    // backend implements it synchronously in the calling thread -- callers
    // should invoke this from a dedicated worker thread.
    virtual bool startStreaming(SdrSampleCallback callback) = 0;
    virtual void stopStreaming() = 0;

    virtual QString lastError() const = 0;
};
