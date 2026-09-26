#pragma once

#include <cstddef>
#include <cstdint>

namespace phraseator {

struct TransportInfo {
    double sampleRate {48000.0};
    double tempoBpm {120.0};
    double projectTimeSamples {0.0};
    bool playing {false};
};

class StepClock {
public:
    void reset() noexcept;

    void configure(double sampleRate, double tempoBpm) noexcept;

    double samplesPerStep() const noexcept { return samplesPerStep_; }

    std::size_t stepIndexAt(double projectTimeSamples) const noexcept;

    double nextStepBoundary(double projectTimeSamples) const noexcept;

private:
    double sampleRate_ {48000.0};
    double tempoBpm_ {120.0};
    double samplesPerStep_ {6000.0}; // 16th @ 120 BPM / 48 kHz
};

} // namespace phraseator
