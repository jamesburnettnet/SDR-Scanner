#pragma once

#include "ISdrDevice.h"
#include <atomic>
#include <vector>

struct rtlsdr_dev; // fwd-declare librtlsdr's opaque handle type (rtlsdr_dev_t)

// ISdrDevice backend for any RTL2832U-based dongle supported by
// librtlsdr -- this covers plain RTL-SDR (V3/V4) dongles as well as the
// Nooelec NESDR family (including the V5), which use the same RTL2832U +
// R820T2/R828D tuner combination and the same driver.
class RtlSdrDevice : public ISdrDevice {
public:
    RtlSdrDevice();
    ~RtlSdrDevice() override;

    bool open(int deviceIndex) override;
    void close() override;
    bool isOpen() const override;

    bool setSampleRate(uint32_t hz) override;
    bool setCenterFrequency(uint64_t hz) override;
    bool setGainMode(bool automatic) override;
    bool setGain(int tenthDb) override;

    bool startStreaming(SdrSampleCallback callback) override;
    void stopStreaming() override;

    QString lastError() const override;

    // Enumeration helpers (static: don't require an open device).
    static int deviceCount();
    static QString deviceName(int index);
    static QString deviceSerial(int index);

private:
    static void rtlsdrCallbackTrampoline(unsigned char *buf, uint32_t len, void *ctx);
    void handleSamples(const unsigned char *buf, uint32_t len);

    rtlsdr_dev *m_dev = nullptr;
    SdrSampleCallback m_callback;
    std::vector<std::complex<float>> m_convertBuf;
    std::atomic<bool> m_streaming{false};
    // Set once rtlsdr_cancel_async() has been issued for the in-progress
    // streaming session -- see the comment in stopStreaming() for why it
    // must not be called more than once per session.
    std::atomic<bool> m_cancelRequested{false};
    // Set the first time this handle starts an async streaming session --
    // see the comment in close() for why a handle that has ever streamed
    // must never be passed to rtlsdr_close() again.
    std::atomic<bool> m_hasStreamed{false};
    QString m_lastError;
};
