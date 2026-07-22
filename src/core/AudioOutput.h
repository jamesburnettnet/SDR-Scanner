#pragma once

#include <QObject>
#include <QIODevice>
#include <QMutex>
#include <QByteArray>
#include <memory>
#include <vector>

class QAudioSink;

// Streaming ring buffer that QAudioSink pulls PCM16 audio from. Scan
// engine (worker thread) pushes demodulated samples in via writeSamples();
// Qt's audio backend pulls via readData() on its own thread. When the
// buffer runs dry (squelch closed / nothing active) it serves silence
// rather than stalling, so audio stays glitch-free between transmissions.
class AudioRingBuffer : public QIODevice {
    Q_OBJECT
public:
    explicit AudioRingBuffer(QObject *parent = nullptr);

    void writeSamples(const float *samples, size_t count); // thread-safe
    void clear();

    bool isSequential() const override { return true; }

protected:
    qint64 readData(char *data, qint64 maxSize) override;
    qint64 writeData(const char *data, qint64 maxSize) override;

private:
    mutable QMutex m_mutex;
    QByteArray m_buffer; // PCM16 mono, little-endian
    static constexpr int kMaxBufferedBytes = 48000 * 2 * 1; // ~1s cap, bounds latency
};

// Owns the QAudioSink and exposes a simple start/stop/push/mute API.
class AudioOutput : public QObject {
    Q_OBJECT
public:
    explicit AudioOutput(QObject *parent = nullptr);
    ~AudioOutput() override;

    void start();
    void stop();

    void setMuted(bool muted) { m_muted = muted; }
    bool isMuted() const { return m_muted; }

    void setVolume(double linear01); // 0..1

    // Thread-safe: called from ScanEngine's worker thread.
    void pushAudio(const std::vector<float> &samples);

private:
    std::unique_ptr<QAudioSink> m_sink;
    AudioRingBuffer m_ringBuffer;
    bool m_muted = false;
};
