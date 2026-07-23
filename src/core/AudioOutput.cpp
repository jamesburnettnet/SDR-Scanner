#include "AudioOutput.h"
#include "DebugLog.h"
#include <QAudioSink>
#include <QAudioDevice>
#include <QMediaDevices>
#include <QAudioFormat>
#include <QTimer>
#include <algorithm>
#include <cstring>
#include <cstdint>

namespace {
QString audioStateToString(QAudio::State state)
{
    switch (state) {
        case QAudio::ActiveState:  return QStringLiteral("Active");
        case QAudio::SuspendedState: return QStringLiteral("Suspended");
        case QAudio::StoppedState: return QStringLiteral("Stopped");
        case QAudio::IdleState:    return QStringLiteral("Idle (buffer underrun/no data)");
        default: return QStringLiteral("Unknown(%1)").arg(static_cast<int>(state));
    }
}

QString audioErrorToString(QAudio::Error error)
{
    switch (error) {
        case QAudio::NoError:        return QStringLiteral("NoError");
        case QAudio::OpenError:      return QStringLiteral("OpenError");
        case QAudio::IOError:        return QStringLiteral("IOError");
        case QAudio::UnderrunError:  return QStringLiteral("UnderrunError");
        case QAudio::FatalError:     return QStringLiteral("FatalError");
        default: return QStringLiteral("Unknown(%1)").arg(static_cast<int>(error));
    }
}
} // namespace

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
    lock.unlock();
    emit readyRead();
}

void AudioRingBuffer::clear()
{
    QMutexLocker lock(&m_mutex);
    m_buffer.clear();
}

qint64 AudioRingBuffer::bytesAvailable() const
{
    QMutexLocker lock(&m_mutex);
    // QAudioSink's pull loop uses this to decide Active vs Idle and whether
    // there's anything worth reading -- QIODevice's default implementation
    // always returns 0 for a device like this one that manages its own
    // buffer instead of QIODevice's built-in one, which left the sink
    // concluding "no data" forever after the first empty read and never
    // pulling from us again, even once writeSamples() had real audio queued.
    return m_buffer.size() + QIODevice::bytesAvailable();
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
    SDR_LOG("audio") << "start(): default output device=" << device.description()
                      << "isNull=" << device.isNull()
                      << "formatSupported=" << device.isFormatSupported(format);

    m_sink = std::make_unique<QAudioSink>(device, format);
    m_sink->setVolume(m_volume);
    connect(m_sink.get(), &QAudioSink::stateChanged, this, [this](QAudio::State state) {
        SDR_LOG("audio") << "QAudioSink stateChanged:" << audioStateToString(state)
                          << "error=" << audioErrorToString(m_sink->error());
        // WASAPI hands back IOError (AUDCLNT_E_DEVICE_INVALIDATED) when the
        // endpoint it opened gets reconfigured or disabled out from under
        // it -- e.g. the user changes the default playback device, or a
        // driver renegotiates format/exclusive-mode. Left alone, the sink
        // just sits dead in StoppedState for the rest of the app's life and
        // every scan session after that point is silent even though
        // nothing else is wrong. Reopening against the (possibly new)
        // default device recovers instead of requiring an app restart.
        // Deferred via singleShot(0) since we're inside a signal from the
        // very m_sink this would reset() -- destroying it here would be
        // reentrant.
        if (state == QAudio::StoppedState && m_sink && m_sink->error() != QAudio::NoError) {
            SDR_LOG("audio") << "QAudioSink stopped with error, reopening output device";
            QTimer::singleShot(0, this, &AudioOutput::reopen);
        }
    });
    m_ringBuffer.open(QIODevice::ReadOnly);
    m_sink->start(&m_ringBuffer);
    SDR_LOG("audio") << "start(): QAudioSink started, initial state="
                      << audioStateToString(m_sink->state()) << "error=" << audioErrorToString(m_sink->error());
}

void AudioOutput::stop()
{
    if (m_sink) {
        SDR_LOG("audio") << "stop(): stopping QAudioSink";
        m_sink->stop();
        m_sink.reset();
    }
    m_ringBuffer.close();
    m_ringBuffer.clear();
}

void AudioOutput::reopen()
{
    stop();
    start();
}

void AudioOutput::setVolume(double linear01)
{
    m_volume = std::clamp(linear01, 0.0, 1.0);
    if (m_sink)
        m_sink->setVolume(m_volume);
}

void AudioOutput::pushAudio(const std::vector<float> &samples)
{
    if (m_muted || samples.empty())
        return;
    m_ringBuffer.writeSamples(samples.data(), samples.size());
}
