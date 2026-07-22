#include "Squelch.h"
#include <algorithm>

namespace {
constexpr double kHysteresisDb = 3.0;
constexpr double kNoiseFloorAlpha = 0.05; // slow EMA for noise floor tracking

// update() is called once per wideband capture block (~6.83ms at the
// engine's 2.4Msps/16384-sample block size), but only while this
// channel's group is the one currently tuned -- so these windows are in
// "blocks actually monitored", not wall-clock time.
constexpr int kChatterWindowBlocks = 220;        // ~1.5s of monitoring
constexpr int kChatterTransitionThreshold = 4;   // >= 2 full open/close cycles in that window
constexpr double kMarginStepUpDb = 2.0;          // tighten fast when static is detected
constexpr double kMarginStepDownDb = 1.0;        // loosen slowly, avoid re-triggering chatter
constexpr double kMarginMaxDb = 30.0;
constexpr double kMarginMinDb = 3.0;
constexpr int kQuietBlocksBeforeRelax = 2930;    // ~20s of monitored silence
}

void Squelch::setManual(double thresholdDb)
{
    m_auto = false;
    m_manualThresholdDb = thresholdDb;
}

void Squelch::setAuto(double marginDb)
{
    m_auto = true;
    m_marginDb = marginDb;
    m_blocksInWindow = 0;
    m_transitionsInWindow = 0;
    m_blocksSinceOpen = 0;
}

double Squelch::thresholdDb() const
{
    if (m_auto)
        return m_noiseFloorDb + m_marginDb;
    return m_manualThresholdDb;
}

bool Squelch::update(double powerDb)
{
    const double threshold = thresholdDb();

    // Only fold samples into the noise-floor estimate when they look like
    // noise (i.e. we're not currently squelch-open on a real signal), so a
    // long transmission doesn't drag the floor upward.
    if (m_auto && !m_open) {
        if (!m_noiseFloorInitialized) {
            m_noiseFloorDb = powerDb;
            m_noiseFloorInitialized = true;
        } else {
            m_noiseFloorDb = (1.0 - kNoiseFloorAlpha) * m_noiseFloorDb + kNoiseFloorAlpha * powerDb;
        }
    }

    const bool wasOpen = m_open;
    if (m_open) {
        if (powerDb < threshold - kHysteresisDb)
            m_open = false;
    } else {
        if (powerDb > threshold)
            m_open = true;
    }

    if (m_auto)
        adaptMargin(wasOpen, m_open);

    return m_open;
}

void Squelch::adaptMargin(bool wasOpen, bool nowOpen)
{
    if (wasOpen != nowOpen)
        ++m_transitionsInWindow;

    if (++m_blocksInWindow >= kChatterWindowBlocks) {
        if (m_transitionsInWindow >= kChatterTransitionThreshold) {
            // Chattering open/closed rapidly -- that's static breaking
            // through right at the threshold, not a real transmission.
            // Tighten quickly.
            m_marginDb = std::min(m_marginDb + kMarginStepUpDb, kMarginMaxDb);
        }
        m_blocksInWindow = 0;
        m_transitionsInWindow = 0;
    }

    if (nowOpen) {
        m_blocksSinceOpen = 0;
    } else if (++m_blocksSinceOpen >= kQuietBlocksBeforeRelax) {
        // Long stretch with zero activity: nudge more sensitive to try to
        // catch weaker transmissions. If this overshoots into chattering,
        // the check above will tighten it back up.
        m_marginDb = std::max(m_marginDb - kMarginStepDownDb, kMarginMinDb);
        m_blocksSinceOpen = 0;
    }
}

void Squelch::reset()
{
    m_noiseFloorInitialized = false;
    m_open = false;
    m_blocksInWindow = 0;
    m_transitionsInWindow = 0;
    m_blocksSinceOpen = 0;
}
