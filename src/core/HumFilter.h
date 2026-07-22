#pragma once

#include <vector>
#include <array>

// Cascaded IIR notch filter targeting AC mains hum (fundamental + 2nd/3rd
// harmonics) in demodulated audio -- a standard mitigation for hum coupled
// electrically into an SDR's front end via USB power/ground loops, which
// shows up as a low tone riding along with otherwise-clean audio
// regardless of which RF frequency is tuned (i.e. not something our
// channelizer/demod math can fix, since it isn't part of the received
// signal).
class HumFilter {
public:
    // fundamentalHz is typically 60 (US/Canada) or 50 (most of the rest
    // of the world); harmonics at 2x/3x are notched too since rectified
    // mains ripple is rarely a pure single tone.
    void configure(double sampleRateHz, double fundamentalHz);

    // Filters `samples` in place. Assumes continuous, in-order calls for
    // a single ongoing audio stream -- see reset().
    void process(std::vector<float> &samples);

    // Clears the filter's internal state (delay lines). Call this when
    // starting a new, unrelated audio stream (e.g. a new transmission) so
    // stale state from a previous one can't bleed a tiny transient into
    // the start of the next.
    void reset();

private:
    struct Biquad {
        float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;
        float z1 = 0.0f, z2 = 0.0f; // Direct Form II transposed state

        void design(double sampleRateHz, double notchHz, double q);
        float processSample(float x);
        void reset();
    };

    std::array<Biquad, 3> m_notches; // fundamental, 2nd harmonic, 3rd harmonic
};
