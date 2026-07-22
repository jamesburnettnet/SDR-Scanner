#include "AudioOutput.h"
#include <QAudioSink>
#include <QAudioDevice>
#include <QMediaDevices>
#include <QAudioFormat>
#include <algorithm>
#include <cstring>
#include <cstdint>

AudioRingBuffer::AudioRingBuffer(QObject *parent)
    : QIODevice(parent)
{
}

void AudioRingBuffer::writeSamples(const float *samples, size_t count)
{
    QMutexLocker lock(&m_mutex);
    const int startSize = m_buffer.size();
    m_buffer.resize(startSize + static_cast<int>(count) * 2);
    auto *out = reinterpret_cast<int16_t *>(m_buffer.data() + startSize);
    for (size_t n = 0; n < count; ++n) {
        float v = std::clamp(samples[n], -1.0f, 1.0f);
        out[n] = static_cast<int16_t>(v * 32760.0f);
    }
    // Bound latency: if the consumer isn't keeping up, drop the oldest data
    // rather than let the buffer (and thus audio delay) grow unbounded.
    if (m_buffer.size() > kMaxBufferedBytes) {
        const int excess = m_buffer.size() - kMaxBufferedBytes;
        m_buffer.remove(0, excess);
    }
}

void AudioRingBuffer::clear()
{
    QMutexLocker lock(&m_mutex);
    m_buffer.clear();
}

qint64 AudioRingBuffer::readData(char *data, qint64 maxSize)
{
    QMutexLocker lock(&m_mutex);
    const qint64 available = std::min<qint64>(maxSize, m_buffer.size());
    if (available > 0) {
        std::memcpy(data, m_buffer.constData(), static_cast<size_t>(available));
        m_buffer.remove(0, static_cast<int>(available));
    }
    if (available < maxSize) {
        // Pad with silence so playback never stalls/clicks when idle.
        std::memset(data + available, 0, static_cast<size_t>(maxSize - available));
    }
    return maxSize;
}

qint64 AudioRingBuffer::writeData(const char *, qint64 maxSize)
{
    // External writes aren't used; feeding happens via writeSamples().
    return maxSize;
}

AudioOutput::AudioOutput(QObject *parent)
    : QObject(parent)
    , m_ringBuffer(this)
{
}

AudioOutput::~AudioOutput()
{
    stop();
}

void AudioOutput::start()
{
    if (m_sink)
        return;

    QAudioFormat format;
    format.setSampleRate(48000);
    format.setChannelCount(1);
    format.setSampleFormat(QAudioFormat::Int16);

    const QAudioDevice device = QMediaDevices::defaultAudioOutput();
    m_sink = std::make_unique<QAudioSink>(device, format);
    m_ringBuffer.open(QIODevice::ReadOnly);
    m_sink->start(&m_ringBuffer);
}

void AudioOutput::stop()
{
    if (m_sink) {
        m_sink->stop();
        m_sink.reset();
    }
    m_ringBuffer.close();
    m_ringBuffer.clear();
}

void AudioOutput::setVolume(double linear01)
{
    if (m_sink)
        m_sink->setVolume(std::clamp(linear01, 0.0, 1.0));
}

void AudioOutput::pushAudio(const std::vector<float> &samples)
{
    if (m_muted || samples.empty())
        return;
    m_ringBuffer.writeSamples(samples.data(), samples.size());
}
