#include "phrase_scheduler.h"

#include <algorithm>
#include <cmath>

namespace phraseator {

void PhraseScheduler::reset() noexcept {
    player_.reset();
    clock_.reset();
    lastTriggeredAbsoluteStep_ = 0u;
    pendingAbsoluteStep_ = 0u;
    pendingStepTimeSamples_ = 0.0;
    nextRatchetTimeSamples_ = 0.0;
    ratchetIntervalSamples_ = 0.0;
    ratchetsRemaining_ = 0u;
    hasTriggeredStep_ = false;
    hasPendingStep_ = false;
}

void PhraseScheduler::prepare(double sampleRate, double tempoBpm) noexcept {
    clock_.configure(sampleRate, tempoBpm);
}

void PhraseScheduler::triggerStep(
    std::size_t stepIndex,
    const SourcePool& pool,
    const std::array<AudioBufferView, kMaxSources>& buffers) noexcept {

    if (stepIndex >= pattern_.size())
        return;

    const auto& step = pattern_[stepIndex];
    if (!step.active || pool.fragmentCount() == 0)
        return;

    FragmentRef ref {};
    const auto flatIndex = static_cast<std::size_t>(step.fragment) % pool.fragmentCount();
    if (!pool.fragmentAt(flatIndex, ref))
        return;

    player_.trigger(pool, buffers, ref, step.velocity, step.pan, step.pitchSemitones);
}

double PhraseScheduler::stepTriggerTime(std::uint64_t absoluteStep) const noexcept {
    const auto stepIndex = static_cast<std::size_t>(absoluteStep % kStepCount);
    const auto& step = pattern_[stepIndex];

    const double gridStart = static_cast<double>(absoluteStep) * clock_.samplesPerStep();
    const double offset = std::clamp(static_cast<double>(step.timingOffset), 0.0, 0.49);
    const double offsetSamples = std::round(offset * clock_.samplesPerStep());
    return gridStart + offsetSamples;
}

void PhraseScheduler::scheduleRatchets(
    std::uint64_t absoluteStep,
    double actualStepTriggerTimeSamples,
    double currentTimeSamples) noexcept {

    const auto stepIndex = static_cast<std::size_t>(absoluteStep % kStepCount);
    const auto& step = pattern_[stepIndex];

    const auto repeats = static_cast<std::uint8_t>(std::max<int>(1, step.repeats));
    if (!step.active || repeats <= 1u) {
        ratchetsRemaining_ = 0u;
        ratchetIntervalSamples_ = 0.0;
        nextRatchetTimeSamples_ = 0.0;
        return;
    }

    ratchetIntervalSamples_ = clock_.samplesPerStep() / static_cast<double>(repeats);
    nextRatchetTimeSamples_ = actualStepTriggerTimeSamples + ratchetIntervalSamples_;
    ratchetsRemaining_ = static_cast<std::uint8_t>(repeats - 1u);

    while (ratchetsRemaining_ > 0u &&
           nextRatchetTimeSamples_ < currentTimeSamples) {
        nextRatchetTimeSamples_ += ratchetIntervalSamples_;
        --ratchetsRemaining_;
    }
}

void PhraseScheduler::triggerAbsoluteStep(
    std::uint64_t absoluteStep,
    double triggerTimeSamples,
    const SourcePool& pool,
    const std::array<AudioBufferView, kMaxSources>& buffers) noexcept {

    triggerStep(static_cast<std::size_t>(absoluteStep % kStepCount), pool, buffers);
    lastTriggeredAbsoluteStep_ = absoluteStep;
    hasTriggeredStep_ = true;
    hasPendingStep_ = false;
    scheduleRatchets(absoluteStep, triggerTimeSamples, triggerTimeSamples);
}

void PhraseScheduler::scheduleAbsoluteStep(
    std::uint64_t absoluteStep,
    double blockTimeSamples,
    const SourcePool& pool,
    const std::array<AudioBufferView, kMaxSources>& buffers) noexcept {

    const double triggerTime = stepTriggerTime(absoluteStep);

    if (triggerTime <= blockTimeSamples) {
        triggerAbsoluteStep(absoluteStep, blockTimeSamples, pool, buffers);
        return;
    }

    pendingAbsoluteStep_ = absoluteStep;
    pendingStepTimeSamples_ = triggerTime;
    hasPendingStep_ = true;
}

void PhraseScheduler::processBlock(
    const SourcePool& pool,
    const std::array<AudioBufferView, kMaxSources>& buffers,
    double projectTimeSamples,
    bool playing,
    float* outLeft,
    float* outRight,
    std::size_t numSamples) noexcept {

    if (outLeft == nullptr || outRight == nullptr || numSamples == 0)
        return;

    std::fill(outLeft, outLeft + numSamples, 0.0f);
    std::fill(outRight, outRight + numSamples, 0.0f);

    if (!playing) {
        player_.reset();
        hasTriggeredStep_ = false;
        hasPendingStep_ = false;
        ratchetsRemaining_ = 0u;
        return;
    }

    const double blockStart = std::max(0.0, projectTimeSamples);
    const auto startAbsoluteStep = clock_.absoluteStepAt(blockStart);

    if ((!hasTriggeredStep_ || startAbsoluteStep != lastTriggeredAbsoluteStep_) &&
        (!hasPendingStep_ || pendingAbsoluteStep_ != startAbsoluteStep)) {
        scheduleAbsoluteStep(startAbsoluteStep, blockStart, pool, buffers);
    }

    double nextBoundary = clock_.nextStepBoundary(blockStart);
    auto nextAbsoluteStep = startAbsoluteStep + 1u;

    for (std::size_t i = 0; i < numSamples; ++i) {
        const double absoluteSample = blockStart + static_cast<double>(i);

        if (hasPendingStep_ && absoluteSample >= pendingStepTimeSamples_) {
            triggerAbsoluteStep(
                pendingAbsoluteStep_, pendingStepTimeSamples_, pool, buffers);
        }

        while (absoluteSample >= nextBoundary) {
            scheduleAbsoluteStep(nextAbsoluteStep, absoluteSample, pool, buffers);
            ++nextAbsoluteStep;
            nextBoundary += clock_.samplesPerStep();
        }

        while (ratchetsRemaining_ > 0u &&
               absoluteSample >= nextRatchetTimeSamples_ &&
               absoluteSample < nextBoundary) {
            triggerStep(static_cast<std::size_t>(lastTriggeredAbsoluteStep_ % kStepCount),
                        pool, buffers);
            nextRatchetTimeSamples_ += ratchetIntervalSamples_;
            --ratchetsRemaining_;
        }

        const auto frame = player_.processSample(pool, buffers);
        outLeft[i] = frame.left;
        outRight[i] = frame.right;
    }
}

} // namespace phraseator
