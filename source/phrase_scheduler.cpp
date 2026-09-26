#include "phrase_scheduler.h"

#include <algorithm>

namespace phraseator {

void PhraseScheduler::reset() noexcept {
    player_.reset();
    clock_.reset();
    lastTriggeredAbsoluteStep_ = 0u;
    hasTriggeredStep_ = false;
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

void PhraseScheduler::triggerAbsoluteStep(
    std::uint64_t absoluteStep,
    const SourcePool& pool,
    const std::array<AudioBufferView, kMaxSources>& buffers) noexcept {

    triggerStep(static_cast<std::size_t>(absoluteStep % kStepCount), pool, buffers);
    lastTriggeredAbsoluteStep_ = absoluteStep;
    hasTriggeredStep_ = true;
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
        return;
    }

    const double blockStart = std::max(0.0, projectTimeSamples);
    const auto startAbsoluteStep = clock_.absoluteStepAt(blockStart);

    if (!hasTriggeredStep_ || startAbsoluteStep != lastTriggeredAbsoluteStep_) {
        triggerAbsoluteStep(startAbsoluteStep, pool, buffers);
    }

    double nextBoundary = clock_.nextStepBoundary(blockStart);
    auto nextAbsoluteStep = startAbsoluteStep + 1u;

    for (std::size_t i = 0; i < numSamples; ++i) {
        const double absoluteSample = blockStart + static_cast<double>(i);

        while (absoluteSample >= nextBoundary) {
            triggerAbsoluteStep(nextAbsoluteStep, pool, buffers);
            ++nextAbsoluteStep;
            nextBoundary += clock_.samplesPerStep();
        }

        const auto frame = player_.processSample(pool, buffers);
        outLeft[i] = frame.left;
        outRight[i] = frame.right;
    }
}

} // namespace phraseator
