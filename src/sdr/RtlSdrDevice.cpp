#include "RtlSdrDevice.h"
#include "../core/DebugLog.h"
#include <rtl-sdr.h>
#include <cstring>
#include <QThread>

namespace {
// librtlsdr delivers unsigned 8-bit offset-binary I/Q; convert to
// normalized complex float, centered on 0, roughly -1..1.
constexpr float kByteToFloat = 1.0f / 127.5f;

// Async read buffer sizing: 16384 samples (32768 bytes) per callback is a
// good balance between latency (~6.8ms at 2.4Msps) and per-call overhead.
constexpr uint32_t kBufLenBytes = 16384 * 2;
constexpr uint32_t kBufCount = 8;
}

RtlSdrDevice::RtlSdrDevice() = default;

RtlSdrDevice::~RtlSdrDevice()
{
    close();
}

int RtlSdrDevice::deviceCount()
{
    return static_cast<int>(rtlsdr_get_device_count());
}

QString RtlSdrDevice::deviceName(int index)
{
    const char *name = rtlsdr_get_device_name(static_cast<uint32_t>(index));
    return name ? QString::fromUtf8(name) : QString();
}

QString RtlSdrDevice::deviceSerial(int index)
{
    char manufacturer[256] = {0};
    char product[256] = {0};
    char serial[256] = {0};
    if (rtlsdr_get_device_usb_strings(static_cast<uint32_t>(index), manufacturer, product, serial) != 0)
        return QString();
    return QString::fromUtf8(serial);
}

bool RtlSdrDevice::open(int deviceIndex)
{
    close();
    SDR_LOG("sdr") << "rtlsdr_open(index=" << deviceIndex << ") [thread" << QThread::currentThreadId() << "]";
    const int rc = rtlsdr_open(reinterpret_cast<rtlsdr_dev_t **>(&m_dev), static_cast<uint32_t>(deviceIndex));
    if (rc < 0) {
        m_lastError = QStringLiteral("rtlsdr_open failed (rc=%1)").arg(rc);
        SDR_LOG("sdr") << m_lastError;
        m_dev = nullptr;
        return false;
    }
    // Sensible defaults; caller (ScanEngine) overrides rate/freq/gain.
    rtlsdr_set_tuner_gain_mode(reinterpret_cast<rtlsdr_dev_t *>(m_dev), 0);
    rtlsdr_set_agc_mode(reinterpret_cast<rtlsdr_dev_t *>(m_dev), 1);
    return true;
}

void RtlSdrDevice::close()
{
    SDR_LOG("sdr") << "RtlSdrDevice::close() [thread" << QThread::currentThreadId() << "] streaming="
                    << m_streaming.load() << "hasStreamed=" << m_hasStreamed.load();
    stopStreaming();
    if (m_dev) {
        if (m_hasStreamed) {
            // librtlsdr's rtlsdr_close() calls rtlsdr_deinit_baseband(),
            // which issues further USB control transfers on the same
            // device -- confirmed via --debug logs (two independent
            // repros) that this reliably crashes the vendored Windows
            // build when called on a handle that has ever run an async
            // streaming session, even after a single, correctly-issued,
            // same-thread cancel with no error. Opening a *new* handle on
            // the same device index afterwards works fine every time, so
            // deliberately leak this handle instead of calling
            // rtlsdr_close() on it -- the OS reclaims it when the process
            // exits, and it doesn't block reopening the same device.
            SDR_LOG("sdr") << "close(): device has streamed -- leaking handle instead of calling rtlsdr_close()";
        } else {
            rtlsdr_close(reinterpret_cast<rtlsdr_dev_t *>(m_dev));
        }
        m_dev = nullptr;
    }
}

bool RtlSdrDevice::isOpen() const
{
    return m_dev != nullptr;
}

bool RtlSdrDevice::setSampleRate(uint32_t hz)
{
    if (!m_dev) return false;
    const int rc = rtlsdr_set_sample_rate(reinterpret_cast<rtlsdr_dev_t *>(m_dev), hz);
    if (rc < 0) {
        m_lastError = QStringLiteral("rtlsdr_set_sample_rate failed (rc=%1)").arg(rc);
        return false;
    }
    return true;
}

bool RtlSdrDevice::setCenterFrequency(uint64_t hz)
{
    if (!m_dev) return false;
    const int rc = rtlsdr_set_center_freq(reinterpret_cast<rtlsdr_dev_t *>(m_dev), static_cast<uint32_t>(hz));
    if (rc < 0) {
        m_lastError = QStringLiteral("rtlsdr_set_center_freq failed (rc=%1)").arg(rc);
        return false;
    }
    return true;
}

bool RtlSdrDevice::setGainMode(bool automatic)
{
    if (!m_dev) return false;
    rtlsdr_set_agc_mode(reinterpret_cast<rtlsdr_dev_t *>(m_dev), automatic ? 1 : 0);
    const int rc = rtlsdr_set_tuner_gain_mode(reinterpret_cast<rtlsdr_dev_t *>(m_dev), automatic ? 0 : 1);
    return rc == 0;
}

bool RtlSdrDevice::setGain(int tenthDb)
{
    if (!m_dev) return false;
    const int rc = rtlsdr_set_tuner_gain(reinterpret_cast<rtlsdr_dev_t *>(m_dev), tenthDb);
    return rc == 0;
}

void RtlSdrDevice::rtlsdrCallbackTrampoline(unsigned char *buf, uint32_t len, void *ctx)
{
    auto *self = static_cast<RtlSdrDevice *>(ctx);
    self->handleSamples(buf, len);
}

void RtlSdrDevice::handleSamples(const unsigned char *buf, uint32_t len)
{
    const size_t count = len / 2;
    m_convertBuf.resize(count);
    for (size_t n = 0; n < count; ++n) {
        const float i = (static_cast<float>(buf[2 * n]) - 127.5f) * kByteToFloat;
        const float q = (static_cast<float>(buf[2 * n + 1]) - 127.5f) * kByteToFloat;
        m_convertBuf[n] = std::complex<float>(i, q);
    }
    if (m_callback)
        m_callback(m_convertBuf.data(), m_convertBuf.size());
}

bool RtlSdrDevice::startStreaming(SdrSampleCallback callback)
{
    if (!m_dev) return false;
    m_callback = std::move(callback);
    m_streaming = true;
    m_cancelRequested = false;
    m_hasStreamed = true;
    rtlsdr_reset_buffer(reinterpret_cast<rtlsdr_dev_t *>(m_dev));
    SDR_LOG("sdr") << "rtlsdr_read_async() entering (blocking) [thread" << QThread::currentThreadId() << "]";
    // Blocks (in the caller's thread) until stopStreaming() -> rtlsdr_cancel_async().
    const int rc = rtlsdr_read_async(reinterpret_cast<rtlsdr_dev_t *>(m_dev), &RtlSdrDevice::rtlsdrCallbackTrampoline,
                                      this, kBufCount, kBufLenBytes);
    m_streaming = false;
    SDR_LOG("sdr") << "rtlsdr_read_async() returned rc=" << rc << "[thread" << QThread::currentThreadId() << "]";
    if (rc < 0 && rc != -5 /* LIBUSB_ERROR_INTERRUPTED-ish on cancel, backend-dependent */) {
        m_lastError = QStringLiteral("rtlsdr_read_async exited (rc=%1)").arg(rc);
    }
    return true;
}

void RtlSdrDevice::stopStreaming()
{
    if (!m_dev || !m_streaming)
        return;

    // ScanEngine's onSamples() calls this once per incoming block for as
    // long as m_streaming stays true, i.e. potentially a dozen-plus times
    // in a row while rtlsdr_read_async() unwinds -- this used to be assumed
    // harmless ("repeated calls just re-request cancellation"), but the
    // vendored Windows build's rtlsdr_cancel_async() is not safe to call
    // again while a cancel is already in flight: --debug logs showed 15-17
    // back-to-back calls followed by a crash on the *next* rtlsdr_* call on
    // the same handle (including plain rtlsdr_close()), the signature of
    // heap corruption from a double-cancel/double-free inside the DLL, not
    // a stale-handle issue. Only the first call per session actually
    // issues the cancel; later ones are no-ops.
    bool expected = false;
    if (!m_cancelRequested.compare_exchange_strong(expected, true))
        return;

    SDR_LOG("sdr") << "rtlsdr_cancel_async() [thread" << QThread::currentThreadId() << "]";
    rtlsdr_cancel_async(reinterpret_cast<rtlsdr_dev_t *>(m_dev));
}

QString RtlSdrDevice::lastError() const
{
    return m_lastError;
}
