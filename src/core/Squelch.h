#pragma once

// Power-based squelch with an optional auto-squelch mode that tracks the
// channel's noise floor and opens a fixed margin above it, similar to the
// "auto squelch" found on consumer scanners.
//
// In auto mode, the margin itself self-adjusts over time:
//  - If the squelch is chattering (opening/closing rapidly -- the
//    signature of static breaking through right at the threshold rather
//    than an actual transmission), the margin is nudged up to reject it.
//  - If it's stayed closed for a long stretch with no activity at all, the
//    margin is nudged back down a little to try to catch weaker
//    transmissions that might not have been breaking squelch.
// Tightening reacts quickly (a few chattery seconds); loosening is slow
// and small, so the margin settles near the tightest setting that doesn't
// chatter rather than oscillating.
//
// None of this applies in manual mode -- a fixed squelch value (e.g. from
// the Auto Tune measurement) is left exactly as set.
class Squelch {
public:
    void setManual(double thresholdDb);
    void setAuto(double marginDb = 10.0);

    bool isAuto() const { return m_auto; }
    bool isOpen() const { return m_open; }
    double thresholdDb() const;
    double noiseFloorDb() const { return m_noiseFloorDb; }
    double marginDb() const { return m_marginDb; }

    // Feed one power reading (in dBFS, i.e. 10*log10(meanPower) referenced
    // to full-scale) for this evaluation window; returns true if the
    // squelch is open (signal present).
    bool update(double powerDb);

    void reset();

private:
    void adaptMargin(bool wasOpen, bool nowOpen);

    bool m_auto = true;
    double m_manualThresholdDb = -50.0;
    double m_marginDb = 10.0;

    double m_noiseFloorDb = -80.0;
    bool m_noiseFloorInitialized = false;
    bool m_open = false;

    // Chatter (rapid open/close) detection, for tightening the margin.
    int m_blocksInWindow = 0;
    int m_transitionsInWindow = 0;

    // Long-quiet detection, for loosening the margin.
    int m_blocksSinceOpen = 0;
};
