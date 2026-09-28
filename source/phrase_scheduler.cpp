#include "phrase_scheduler.h"

#include <algorithm>
#include <cmath>

namespace phraseator {

void PhraseScheduler::resetPlaybackState() noexcept {
    player_.reset();
    lastTriggeredAbsoluteStep_ = 0u;
    pendingAbsoluteStep_ = 0u;
    pendingStepPosition_ = 0.0;
    nextRatchetStepPosition_ = 0.0;
    ratchetIntervalSteps_ = 0.0;
    ratchetsRemaining_ = 0u;
    hasTriggeredStep_ = false;
    hasPendingStep_ = false;
}

void PhraseScheduler::reset() noexcept {
    resetPlaybackState();
    timelineValid_ = false;
    expectedNextStepPosition_ = 0.0;
}

void PhraseScheduler::prepare(double sampleRate) noexcept {
    player_.prepare(sampleRate);
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
    if (!pool.fragmentAt(static_cast<std::size_t>(step.fragment), ref))
        return;

    // Monophonic phrase articulation: every new active step (and ratchet)
    // ends the previous phrase voice with a short de-click release.
    player_.chokeAll();
    player_.trigger(pool, buffers, ref, step.velocity, step.pan, step.pitchSemitones);
}

double PhraseScheduler::stepTriggerPosition(
    std::uint64_t absoluteStep) const noexcept {

    const auto stepIndex = static_cast<std::size_t>(absoluteStep % kStepCount);
    const auto& step = pattern_[stepIndex];
    const double offset =
        std::clamp(static_cast<double>(step.timingOffset), 0.0, 0.49);
    return static_cast<double>(absoluteStep) + offset;
}

void PhraseScheduler::scheduleRatchets(
    std::uint64_t absoluteStep,
    double actualTriggerStepPosition,
    double currentStepPosition) noexcept {

    const auto stepIndex = static_cast<std::size_t>(absoluteStep % kStepCount);
    const auto& step = pattern_[stepIndex];

    const auto repeats = static_cast<std::uint8_t>(
        std::clamp<int>(step.repeats, 1, kMaxRatchetHits));
    if (!step.active || repeats <= 1u) {
        ratchetsRemaining_ = 0u;
        ratchetIntervalSteps_ = 0.0;
        nextRatchetStepPosition_ = 0.0;
        return;
    }

    ratchetIntervalSteps_ = 1.0 / static_cast<double>(repeats);
    nextRatchetStepPosition_ =
        actualTriggerStepPosition + ratchetIntervalSteps_;
    ratchetsRemaining_ = static_cast<std::uint8_t>(repeats - 1u);

    while (ratchetsRemaining_ > 0u &&
           nextRatchetStepPosition_ < currentStepPosition) {
        nextRatchetStepPosition_ += ratchetIntervalSteps_;
        --ratchetsRemaining_;
    }
}

void PhraseScheduler::triggerAbsoluteStep(
    std::uint64_t absoluteStep,
    double triggerStepPosition,
    const SourcePool& pool,
    const std::array<AudioBufferView, kMaxSources>& buffers) noexcept {

    triggerStep(
        static_cast<std::size_t>(absoluteStep % kStepCount), pool, buffers);
    lastTriggeredAbsoluteStep_ = absoluteStep;
    hasTriggeredStep_ = true;
    hasPendingStep_ = false;
    scheduleRatchets(
        absoluteStep, triggerStepPosition, triggerStepPosition);
}

void PhraseScheduler::scheduleAbsoluteStep(
    std::uint64_t absoluteStep,
    double currentStepPosition,
    const SourcePool& pool,
    const std::array<AudioBufferView, kMaxSources>& buffers) noexcept {

    const double triggerPosition = stepTriggerPosition(absoluteStep);

    if (triggerPosition <= currentStepPosition) {
        triggerAbsoluteStep(
            absoluteStep, currentStepPosition, pool, buffers);
        return;
    }

    pendingAbsoluteStep_ = absoluteStep;
    pendingStepPosition_ = triggerPosition;
    hasPendingStep_ = true;
}

bool PhraseScheduler::processBlock(
    const SourcePool& pool,
    const std::array<AudioBufferView, kMaxSources>& buffers,
    double startStepPosition,
    double stepsPerSample,
    bool playing,
    float* outLeft,
    float* outRight,
    std::size_t numSamples) noexcept {

    if (outLeft == nullptr || outRight == nullptr || numSamples == 0u)
        return false;

    std::fill(outLeft, outLeft + numSamples, 0.0f);
    std::fill(outRight, outRight + numSamples, 0.0f);

    if (!playing) {
        resetPlaybackState();
        timelineValid_ = false;
        expectedNextStepPosition_ = 0.0;
        return false;
    }

    if (!std::isfinite(startStepPosition) ||
        !std::isfinite(stepsPerSample) ||
        stepsPerSample <= 0.0) {
        resetPlaybackState();
        timelineValid_ = false;
        expectedNextStepPosition_ = 0.0;
        return false;
    }

    bool producedAudio = false;
    const double blockStart = std::max(0.0, startStepPosition);

    // Host seek/loop wrap is detected in musical time. A tempo change alone
    // does not create a discontinuity because the accumulated step position
    // remains continuous.
    const double continuityTolerance =
        std::max(1.0e-8, stepsPerSample * 1.5);
    if (timelineValid_ &&
        std::fabs(blockStart - expectedNextStepPosition_) >
            continuityTolerance) {
        resetPlaybackState();
    }

    const auto startAbsoluteStep = static_cast<std::uint64_t>(
        std::floor(blockStart));

    if ((!hasTriggeredStep_ ||
         startAbsoluteStep != lastTriggeredAbsoluteStep_) &&
        (!hasPendingStep_ ||
         pendingAbsoluteStep_ != startAbsoluteStep)) {
        scheduleAbsoluteStep(startAbsoluteStep, blockStart, pool, buffers);
    }

    double nextBoundary = static_cast<double>(startAbsoluteStep + 1u);
    auto nextAbsoluteStep = startAbsoluteStep + 1u;

    for (std::size_t i = 0; i < numSamples; ++i) {
        const double stepPosition =
            blockStart + static_cast<double>(i) * stepsPerSample;

        if (hasPendingStep_ &&
            stepPosition + 1.0e-12 >= pendingStepPosition_) {
            triggerAbsoluteStep(
                pendingAbsoluteStep_,
                pendingStepPosition_,
                pool,
                buffers);
        }

        while (stepPosition + 1.0e-12 >= nextBoundary) {
            scheduleAbsoluteStep(
                nextAbsoluteStep, stepPosition, pool, buffers);
            ++nextAbsoluteStep;
            nextBoundary += 1.0;
        }

        while (ratchetsRemaining_ > 0u &&
               stepPosition + 1.0e-12 >= nextRatchetStepPosition_ &&
               stepPosition < nextBoundary) {
            triggerStep(
                static_cast<std::size_t>(
                    lastTriggeredAbsoluteStep_ % kStepCount),
                pool,
                buffers);
            nextRatchetStepPosition_ += ratchetIntervalSteps_;
            --ratchetsRemaining_;
        }

        const auto frame = player_.processSample(pool, buffers);
        outLeft[i] = frame.left;
        outRight[i] = frame.right;
        producedAudio =
            producedAudio || frame.left != 0.0f || frame.right != 0.0f;
    }

    expectedNextStepPosition_ =
        blockStart + static_cast<double>(numSamples) * stepsPerSample;
    timelineValid_ = true;

    return producedAudio;
}

} // namespace phraseator
