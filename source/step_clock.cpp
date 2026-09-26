#include "step_clock.h"

#include <algorithm>
#include <cmath>

namespace phraseator {

void StepClock::reset() noexcept {
    sampleRate_ = 48000.0;
    tempoBpm_ = 120.0;
    samplesPerStep_ = 6000.0;
}

void StepClock::configure(double sampleRate, double tempoBpm) noexcept {
    sampleRate_ = sampleRate > 1.0 ? sampleRate : 48000.0;
    tempoBpm_ = std::clamp(tempoBpm, 20.0, 400.0);

    const double quarterSamples = sampleRate_ * 60.0 / tempoBpm_;
    samplesPerStep_ = quarterSamples / 4.0;
}

std::size_t StepClock::stepIndexAt(double projectTimeSamples) const noexcept {
    if (samplesPerStep_ <= 0.0)
        return 0;

    const double safeTime = std::max(0.0, projectTimeSamples);
    const auto absoluteStep = static_cast<std::uint64_t>(std::floor(safeTime / samplesPerStep_));
    return static_cast<std::size_t>(absoluteStep % 16u);
}

double StepClock::nextStepBoundary(double projectTimeSamples) const noexcept {
    if (samplesPerStep_ <= 0.0)
        return projectTimeSamples;

    const double safeTime = std::max(0.0, projectTimeSamples);
    const double absoluteStep = std::floor(safeTime / samplesPerStep_);
    return (absoluteStep + 1.0) * samplesPerStep_;
}

} // namespace phraseator
