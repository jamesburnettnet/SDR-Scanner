#pragma once

#include <QThread>
#include <QVector>
#include <QMutex>
#include <QDateTime>
#include <atomic>
#include <memory>
#include <vector>
#include "Frequency.h"
#include "ScanGroup.h"
#include "ChannelProcessor.h"
#include "ScannerState.h"
#include "HumFilter.h"

class ISdrDevice;
class AudioOutput;

// Runs the scan loop on its own thread. Tunes the RTL-SDR to one capture
// group at a time, streams asynchronously via the device backend, and on
// every incoming IQ block evaluates squelch on *all* channels in the
// current group at once (the speed win from grouping) before deciding to
// dwell, lock onto a channel, or advance to the next group.
//
// Retuning always happens *between* streaming sessions, never from inside
// the async read callback: interleaving a blocking tuner I2C command with
// an active USB bulk sample stream is a well-known source of tuner
// timeouts (librtlsdr's "i2c wr failed" / "r82xx_set_freq: failed"
// messages). So each group switch briefly stops streaming, retunes, then
// restarts it, keeping control transfers and bulk streaming fully
// separated in time.
//
// Reused for both regular scanning and Explore mode: Explore just supplies
// a synthetic frequency list generated from a start/end/step sweep.
class ScanEngine : public QThread {
    Q_OBJECT
public:
    explicit ScanEngine(QObject *parent = nullptr);
    ~ScanEngine() override;

    // Must be called before start(); device must already be open.
    void configure(ISdrDevice *device, AudioOutput *audioOutput, ScannerStateHolder *stateHolder,
                    QVector<Frequency> frequencies, bool isExplore);

    // Thread-safe: signals the run loop to stop and unblocks the device's
    // blocking async read call (if one is active).
    void requestStop();

    // Thread-safe, effective live (no restart needed): mains-hum notch
    // filter applied to demodulated audio before it reaches the speaker.
    void setHumFilterEnabled(bool enabled) { m_humFilterEnabled = enabled; }
    void setHumFilterHz(int hz) { m_humFilterHz = hz; }

signals:
    void errorOccurred(const QString &message);

    // Emitted once a transmission ends (i.e. that channel's squelch goes
    // from open back to closed), covering *every* channel in whichever
    // group is currently tuned -- not just the one being displayed/held.
    // That's what lets the activity log show traffic on a channel other
    // than the one you're currently parked on.
    void activityLogged(QDateTime startTime, qint64 freqHz, QString label, Modulation modulation,
                         QString group, qint64 durationMs);

protected:
    void run() override;

private:
    void onSamples(const std::complex<float> *samples, size_t count);
    void buildAllProcessors();
    void requestGroupSwitch();
    void publishSnapshot();

    ISdrDevice *m_device = nullptr;
    AudioOutput *m_audioOutput = nullptr;
    ScannerStateHolder *m_stateHolder = nullptr;
    QVector<Frequency> m_frequencies;
    bool m_isExplore = false;

    QVector<ScanGroup> m_groups;
    std::shared_ptr<const std::vector<float>> m_stage1Coeffs;

    // One ChannelProcessor per channel, kept alive for the whole scan
    // session (indexed [group][channel], parallel to m_groups). Built once
    // up front and never recreated on group switches, so each channel's
    // auto-squelch noise-floor estimate keeps accumulating across scan
    // laps instead of being thrown away and re-learned every time the loop
    // comes back around to that group.
    std::vector<std::vector<std::unique_ptr<ChannelProcessor>>> m_allProcessors;

    // Per-channel open/close tracking for the activity log, independent of
    // (and covering more than) the lock/hold logic above -- every channel
    // in the currently tuned group gets tracked, not just the one being
    // displayed or held. Indexed [group][channel], parallel to m_groups.
    std::vector<std::vector<bool>> m_allWasOpen;
    std::vector<std::vector<qint64>> m_allOpenStartMs;

    int m_groupIndex = 0;
    int m_blocksInGroup = 0;
    bool m_locked = false;
    int m_lockedChannelIdx = -1;
    int m_hangBlocks = 0;

    // Purely cosmetic: while idle-scanning a group, all channels in it are
    // actually squelch-checked every block (that's the speed win), but we
    // rotate which one the LCD shows so it still reads as a scanner
    // "looping through" frequencies rather than sitting still.
    int m_displayCursor = 0;
    int m_blocksSinceDisplayStep = 0;

    std::atomic<bool> m_stopRequested{false};
    std::vector<float> m_audioScratch;

    // Mains-hum notch filter: m_humFilterEnabled/Hz are set from the GUI
    // thread; m_humFilter itself and m_humFilterConfiguredHz are only ever
    // touched on this scan thread, which lazily rebuilds the filter when
    // it notices the desired Hz changed.
    std::atomic<bool> m_humFilterEnabled{true};
    std::atomic<int> m_humFilterHz{60};
    HumFilter m_humFilter;
    int m_humFilterConfiguredHz = 0;

    // Guards the handoff between "setting up the next streaming session"
    // and requestStop() potentially arriving from the GUI thread at the
    // same time -- without this, a stop request that lands in the brief
    // gap between streaming sessions (when device->stopStreaming() would
    // be a no-op because nothing is streaming yet) could be missed
    // entirely, leaving the Stop button hung waiting on a thread that just
    // went on to start yet another blocking read.
    QMutex m_streamMutex;
    bool m_streamActive = false;
    bool m_switchGroupRequested = false;
};
