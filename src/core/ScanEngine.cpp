#include "ScanEngine.h"
#include "AudioOutput.h"
#include "DebugLog.h"
#include "../sdr/ISdrDevice.h"
#include <QDebug>
#include <QThread>

namespace {
// ~16384 samples per callback at 2.4Msps => ~6.83ms per block.
//
// kDwellBlocksIdle controls how often we retune to the next capture group
// while idle-scanning. Every retune is a burst of I2C register writes to
// the tuner over the same USB link that's also streaming samples
// continuously; retuning too aggressively starves those control transfers
// and shows up as "rtlsdr_demod_write_reg failed" / i2c timeout (-6)
// messages from librtlsdr. Since grouping already lets us check every
// channel in a group at once, there's no need to hop groups fast -- this
// keeps retunes infrequent enough to leave the USB control endpoint room
// to breathe, while still cycling a handful of groups multiple times/sec.
constexpr int kDwellBlocksIdle = 22;      // ~150ms per group while nothing is active
constexpr int kHangBlocks = 150;          // ~1.0s hang time after a signal drops
constexpr int kDisplayStepBlocks = 2; // ~14ms per LCD cursor step while idle-scanning

// Activity log entries shorter than this are almost always a brief
// squelch blip rather than a real transmission; skip logging them so the
// log stays meaningful instead of filling with noise.
constexpr qint64 kMinLoggedDurationMs = 150;
}

ScanEngine::ScanEngine(QObject *parent)
    : QThread(parent)
{
    // Auto-resolves to a queued connection: retuneRequested() is emitted
    // from onSamples() on the scan thread, but this object's own thread
    // affinity is whatever thread constructed it (the GUI thread) --
    // subclassing QThread doesn't move the QObject itself, only run()
    // actually executes on the new thread. See the class comment for why
    // the retune needs to happen there instead of inline in onSamples().
    connect(this, &ScanEngine::retuneRequested, this, &ScanEngine::performRetune);
}

ScanEngine::~ScanEngine()
{
    requestStop();
    wait();
}

void ScanEngine::configure(ISdrDevice *device, AudioOutput *audioOutput, ScannerStateHolder *stateHolder,
                            QVector<Frequency> frequencies, bool isExplore)
{
    m_device = device;
    m_audioOutput = audioOutput;
    m_stateHolder = stateHolder;
    m_frequencies = std::move(frequencies);
    m_isExplore = isExplore;
}

void ScanEngine::requestStop()
{
    QMutexLocker lock(&m_streamMutex);
    SDR_LOG("engine") << "requestStop() [thread" << QThread::currentThreadId() << "] streamActive=" << m_streamActive;
    m_stopRequested = true;
    // Deliberately NOT calling m_device->stopStreaming() here. librtlsdr's
    // rtlsdr_cancel_async() is documented (rtl-sdr.h) as safe to call only
    // from within the streaming callback itself -- "due to incomplete
    // concurrency implementation, this should only be called from within
    // the callback function, so it is in the correct thread." This method
    // runs on the GUI thread while rtlsdr_read_async() is blocked on the
    // scan thread, so calling cancel from here is exactly the cross-thread
    // call the library warns against (and a likely cause of the flaky
    // Windows-only crashes seen around start/stop/reconnect -- the
    // Windows build's vendored librtlsdr has a materially different
    // internal implementation than whatever Linux distros package).
    //
    // Instead, onSamples() checks m_stopRequested on the next incoming
    // sample block (at most one ~6.83ms block away, since streaming is
    // active) and calls stopStreaming() from there, on the correct thread.
    //
    // If nothing is streaming yet (a stop landing right as run() is still
    // setting up), there's nothing to cancel -- but m_stopRequested is now
    // set under the same lock run() takes before starting the stream, so
    // it's guaranteed to notice and skip straight to cleanup instead of
    // starting a blocking read that would never get cancelled.
}

void ScanEngine::run()
{
    SDR_LOG("engine") << "run() entering [thread" << QThread::currentThreadId() << "] frequencies="
                       << m_frequencies.size() << "explore=" << m_isExplore;
    if (!m_device || !m_device->isOpen()) {
        emit errorOccurred(QStringLiteral("No SDR device open"));
        return;
    }

    m_stopRequested = false;

    if (!m_device->setSampleRate(static_cast<uint32_t>(ScanGrouping::kDeviceSampleRateHz))) {
        emit errorOccurred(QStringLiteral("Failed to set sample rate: %1").arg(m_device->lastError()));
        return;
    }
    m_device->setGainMode(true); // AGC by default -- keeps v1 simple

    m_groups = ScanGrouping::buildGroups(m_frequencies);
    if (m_groups.isEmpty()) {
        emit errorOccurred(QStringLiteral("No enabled frequencies to scan"));
        return;
    }

    m_stage1Coeffs = ChannelProcessor::buildStage1Coeffs();
    buildAllProcessors();
    SDR_LOG("engine") << "groups built:" << m_groups.size();

    m_groupIndex = 0;
    m_blocksInGroup = 0;
    m_locked = false;
    m_lockedChannelIdx = -1;
    m_hangBlocks = 0;
    m_displayCursor = 0;
    m_blocksSinceDisplayStep = 0;

    // The initial tune happens here, before streaming starts, on this same
    // thread -- fine, since rtlsdr_read_async() hasn't started blocking
    // yet. Every subsequent retune (on a group switch) happens live, via
    // retuneRequested()/performRetune() on the GUI thread instead -- see
    // the class comment for why the stream itself is only ever started
    // once, for the whole session.
    {
        QMutexLocker lock(&m_streamMutex);
        if (m_stopRequested) {
            SDR_LOG("engine") << "run(): stop requested before streaming started";
            ScannerSnapshot finalSnap;
            finalSnap.mode = EngineMode::Idle;
            finalSnap.deviceConnected = m_device->isOpen();
            if (m_stateHolder)
                m_stateHolder->publish(finalSnap);
            return;
        }
        m_device->setCenterFrequency(static_cast<uint64_t>(m_groups[m_groupIndex].centerHz));
        m_streamActive = true;
    }

    publishSnapshot();
    m_device->startStreaming([this](const std::complex<float> *s, size_t n) { onSamples(s, n); });

    {
        QMutexLocker lock(&m_streamMutex);
        m_streamActive = false;
    }

    if (m_stopRequested) {
        SDR_LOG("engine") << "run(): stop requested, exiting";
    } else {
        // Streaming ended without us asking for a stop -- most likely the
        // device errored out or was unplugged.
        SDR_LOG("engine") << "run(): streaming ended unexpectedly, lastError=" << m_device->lastError();
        emit errorOccurred(QStringLiteral("SDR streaming stopped unexpectedly: %1").arg(m_device->lastError()));
    }

    SDR_LOG("engine") << "run() exiting [thread" << QThread::currentThreadId() << "]";
    ScannerSnapshot finalSnap;
    finalSnap.mode = EngineMode::Idle;
    finalSnap.deviceConnected = m_device->isOpen();
    if (m_stateHolder)
        m_stateHolder->publish(finalSnap);
}

void ScanEngine::buildAllProcessors()
{
    m_allProcessors.clear();
    m_allProcessors.reserve(m_groups.size());
    m_allWasOpen.clear();
    m_allWasOpen.reserve(m_groups.size());
    m_allOpenStartMs.clear();
    m_allOpenStartMs.reserve(m_groups.size());

    for (const auto &group : m_groups) {
        std::vector<std::unique_ptr<ChannelProcessor>> groupProcs;
        groupProcs.reserve(group.channels.size());
        for (const auto &ch : group.channels) {
            auto proc = std::make_unique<ChannelProcessor>(ScanGrouping::kDeviceSampleRateHz, ch.offsetHz,
                                                             ch.freq.modulation, m_stage1Coeffs);
            if (ch.freq.autoSquelch)
                proc->setSquelchAuto();
            else
                proc->setSquelchManual(ch.freq.squelchDb);
            groupProcs.push_back(std::move(proc));
        }
        m_allProcessors.push_back(std::move(groupProcs));
        m_allWasOpen.push_back(std::vector<bool>(group.channels.size(), false));
        m_allOpenStartMs.push_back(std::vector<qint64>(group.channels.size(), 0));
    }
}

void ScanEngine::requestGroupSwitch()
{
    if (m_groups.size() <= 1) {
        // Nothing to switch to.
        m_blocksInGroup = 0;
        return;
    }

    m_locked = false;
    m_lockedChannelIdx = -1;
    m_blocksInGroup = 0;
    m_displayCursor = 0;
    m_blocksSinceDisplayStep = 0;
    m_groupIndex = (m_groupIndex + 1) % m_groups.size();

    SDR_LOG("engine") << "requestGroupSwitch() [thread" << QThread::currentThreadId() << "] ->"
                       << m_groupIndex;

    // The stream itself keeps running -- only the center frequency needs
    // to change, and that happens on the GUI thread via performRetune()
    // (see the class comment for why: this method runs on the scan
    // thread, from inside the streaming callback, where issuing another
    // blocking libusb call would be a reentrancy hazard).
    emit retuneRequested(static_cast<quint64>(m_groups[m_groupIndex].centerHz));

    // Note: channel processors for the next group are *not* recreated --
    // they were all built once up front in buildAllProcessors() and keep
    // accumulating their auto-squelch noise-floor estimate across the
    // whole scan session, including every time the loop comes back around
    // to a given group. Any stale FIR filter history from being untuned
    // for a while flushes out within one block, so this is harmless -- as
    // is the handful of blocks right after a retune that still reflect the
    // previous group's RF (normal tuner settling transient, same as
    // before).
}

void ScanEngine::performRetune(quint64 centerHz)
{
    if (m_device)
        m_device->setCenterFrequency(centerHz);
}

void ScanEngine::onSamples(const std::complex<float> *samples, size_t count)
{
    if (m_stopRequested) {
        // Cancel here, not from requestStop() on the GUI thread -- see the
        // comment there. This is the streaming callback, i.e. the only
        // thread librtlsdr documents rtlsdr_cancel_async() as safe to call
        // from. May end up calling stopStreaming() again for a block or two
        // more that were already in flight before the cancel takes effect;
        // RtlSdrDevice::stopStreaming() itself only issues the actual
        // rtlsdr_cancel_async() once per session, since calling it again
        // while a cancel is already in flight corrupts the vendored Windows
        // build's internal state (see the comment there).
        SDR_LOG("engine") << "onSamples(): stop noticed, cancelling from callback thread";
        m_device->stopStreaming();
        return;
    }

    const ScanGroup &group = m_groups[m_groupIndex];
    auto &processors = m_allProcessors[m_groupIndex];
    auto &wasOpen = m_allWasOpen[m_groupIndex];
    auto &openStartMs = m_allOpenStartMs[m_groupIndex];

    bool anyOpen = false;
    int openIdx = -1;

    m_audioScratch.clear();

    for (int i = 0; i < static_cast<int>(processors.size()); ++i) {
        const bool wantAudio = m_locked && (i == m_lockedChannelIdx);
        processors[i]->process(samples, count, wantAudio, m_audioScratch);
        const bool isOpen = processors[i]->squelchOpen();
        if (isOpen) {
            anyOpen = true;
            if (openIdx < 0)
                openIdx = i;
        }

        // Activity log: tracked for every channel in this group, not just
        // the locked/displayed one, so traffic on a channel you're not
        // currently parked on still shows up.
        if (isOpen && !wasOpen[i]) {
            openStartMs[i] = QDateTime::currentMSecsSinceEpoch();
        } else if (!isOpen && wasOpen[i]) {
            const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
            const qint64 durationMs = nowMs - openStartMs[i];
            if (durationMs >= kMinLoggedDurationMs) {
                const auto &ch = group.channels[i];
                emit activityLogged(QDateTime::fromMSecsSinceEpoch(openStartMs[i]), ch.freq.hz, ch.freq.label,
                                     ch.freq.modulation, durationMs);
            }
        }
        wasOpen[i] = isOpen;
    }

    if (m_locked) {
        // squelchOpen() here reflects this block's freshly-updated state
        // (process() just ran above), not last block's.
        const bool stillOpen = processors[m_lockedChannelIdx]->squelchOpen();
        if (stillOpen) {
            m_hangBlocks = kHangBlocks;
        } else if (--m_hangBlocks <= 0) {
            SDR_LOG("engine") << "hang time expired, unlocking channel" << m_lockedChannelIdx;
            m_locked = false;
            m_lockedChannelIdx = -1;
            m_blocksInGroup = 0;
        }
        // Hang time keeps the receiver parked on this channel so a
        // transmission that resumes after a brief pause doesn't force a
        // full re-scan -- but only send audio to the speaker while
        // squelch is actually open. Otherwise the hang period plays raw
        // demodulated noise for up to ~1s after every transmission ends.
        if (m_audioOutput && stillOpen && !m_audioScratch.empty()) {
            if (m_humFilterEnabled) {
                const int desiredHz = m_humFilterHz;
                if (desiredHz != m_humFilterConfiguredHz) {
                    m_humFilter.configure(48000.0, desiredHz);
                    m_humFilterConfiguredHz = desiredHz;
                }
                m_humFilter.process(m_audioScratch);
            }
            m_audioOutput->pushAudio(m_audioScratch);
        } else if (!m_audioOutput) {
            SDR_LOG("audio") << "squelch open but m_audioOutput is null -- no audio possible";
        }
    } else if (anyOpen) {
        SDR_LOG("engine") << "squelch open, locking onto channel" << openIdx << "freqHz="
                           << group.channels[openIdx].freq.hz;
        m_locked = true;
        m_lockedChannelIdx = openIdx;
        m_hangBlocks = kHangBlocks;
        m_humFilter.reset(); // fresh transmission -- don't carry filter state from a previous one
    } else {
        ++m_blocksInGroup;
        if (m_blocksInGroup >= kDwellBlocksIdle) {
            requestGroupSwitch();
        } else if (!processors.empty() && ++m_blocksSinceDisplayStep >= kDisplayStepBlocks) {
            m_blocksSinceDisplayStep = 0;
            m_displayCursor = (m_displayCursor + 1) % static_cast<int>(processors.size());
        }
    }

    publishSnapshot();
}

void ScanEngine::publishSnapshot()
{
    if (!m_stateHolder)
        return;

    const ScanGroup &group = m_groups[m_groupIndex];
    auto &processors = m_allProcessors[m_groupIndex];
    ScannerSnapshot snap;
    snap.deviceConnected = m_device && m_device->isOpen();
    snap.mode = m_locked ? EngineMode::Holding : (m_isExplore ? EngineMode::Exploring : EngineMode::Scanning);
    snap.groupIndex = m_groupIndex;
    snap.groupCount = m_groups.size();
    snap.channelCount = group.channels.size();

    const int idx = m_locked ? m_lockedChannelIdx : (group.channels.isEmpty() ? -1 : m_displayCursor);
    if (idx >= 0 && idx < group.channels.size()) {
        const auto &ch = group.channels[idx];
        snap.channelIndex = idx;
        snap.currentFreqHz = ch.freq.hz;
        snap.currentLabel = ch.freq.label;
        snap.currentModulation = ch.freq.modulation;
        snap.signalDb = processors[idx]->lastPowerDb();
        snap.squelchThresholdDb = processors[idx]->squelchThresholdDb();
        snap.squelchOpen = processors[idx]->squelchOpen();
        if (snap.mode == EngineMode::Holding)
            snap.activeFrequencyId = ch.freq.id;
    }

    m_stateHolder->publish(snap);
}
