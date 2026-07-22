#include "RtlSdrDevice.h"
#include <rtl-sdr.h>
#include <cstring>

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
    const int rc = rtlsdr_open(reinterpret_cast<rtlsdr_dev_t **>(&m_dev), static_cast<uint32_t>(deviceIndex));
    if (rc < 0) {
        m_lastError = QStringLiteral("rtlsdr_open failed (rc=%1)").arg(rc);
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
    stopStreaming();
    if (m_dev) {
        rtlsdr_close(reinterpret_cast<rtlsdr_dev_t *>(m_dev));
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
    rtlsdr_reset_buffer(reinterpret_cast<rtlsdr_dev_t *>(m_dev));
    // Blocks (in the caller's thread) until stopStreaming() -> rtlsdr_cancel_async().
    const int rc = rtlsdr_read_async(reinterpret_cast<rtlsdr_dev_t *>(m_dev), &RtlSdrDevice::rtlsdrCallbackTrampoline,
                                      this, kBufCount, kBufLenBytes);
    m_streaming = false;
    if (rc < 0 && rc != -5 /* LIBUSB_ERROR_INTERRUPTED-ish on cancel, backend-dependent */) {
        m_lastError = QStringLiteral("rtlsdr_read_async exited (rc=%1)").arg(rc);
    }
    return true;
}

void RtlSdrDevice::stopStreaming()
{
    if (m_dev && m_streaming) {
        rtlsdr_cancel_async(reinterpret_cast<rtlsdr_dev_t *>(m_dev));
    }
}

QString RtlSdrDevice::lastError() const
{
    return m_lastError;
}
