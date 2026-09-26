#pragma once

#include <cstddef>
#include <cstdint>

namespace phraseator {

class StepClock {
public:
    void reset() noexcept;

    void configure(double sampleRate, double tempoBpm) noexcept;

    double samplesPerStep() const noexcept { return samplesPerStep_; }

    std::uint64_t absoluteStepAt(double projectTimeSamples) const noexcept;
    std::size_t stepIndexAt(double projectTimeSamples) const noexcept;

    double nextStepBoundary(double projectTimeSamples) const noexcept;

private:
    double sampleRate_ {48000.0};
    double tempoBpm_ {120.0};
    double samplesPerStep_ {6000.0};
};

} // namespace phraseator
