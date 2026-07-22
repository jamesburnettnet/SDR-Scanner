#pragma once

#include <QVector>
#include "Frequency.h"

// One channel within a capture group: a Frequency plus its offset (Hz)
// from the group's tuned center frequency.
struct GroupChannel {
    Frequency freq;
    qint64 offsetHz = 0; // freq.hz - group center, can be negative
};

// A set of frequencies close enough together to be captured in a single
// wideband RTL-SDR tune, so the scan engine can check squelch on all of
// them simultaneously from one capture instead of retuning per-frequency.
struct ScanGroup {
    qint64 centerHz = 0;
    QVector<GroupChannel> channels;
};

namespace ScanGrouping {

// Device sample rate used for grouped capture. Chosen so that decimating
// by 50 lands exactly on a 48kHz audio/channel rate.
constexpr qint64 kDeviceSampleRateHz = 2'400'000;

// Usable half-bandwidth around a group's center frequency. Kept below the
// full Nyquist width (1.2MHz) to leave margin for channel filter roll-off
// and RTL-SDR edge-of-band artifacts.
constexpr qint64 kUsableHalfSpanHz = 1'000'000;

// Greedily groups a (frequency-sorted) list of enabled frequencies into
// ScanGroups whose channels all fit within kUsableHalfSpanHz of a common
// center frequency, suitable for one wideband capture per group.
QVector<ScanGroup> buildGroups(const QVector<Frequency> &frequencies);

} // namespace ScanGrouping
