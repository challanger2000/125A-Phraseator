#include "phrase_scheduler.h"

#include <algorithm>
#include <cmath>

namespace phraseator {

void PhraseScheduler::reset() noexcept {
    player_.reset();
    clock_.reset();
    wasPlaying_ = false;
}

void PhraseScheduler::prepare(double sampleRate, double tempoBpm) noexcept {
    clock_.configure(sampleRate, tempoBpm);
}

void PhraseScheduler::triggerStep(std::size_t stepIndex,
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

void PhraseScheduler::processBlock(const SourcePool& pool,
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
        wasPlaying_ = false;
        return;
    }

    const double blockStart = std::max(0.0, projectTimeSamples);
    double nextBoundary = clock_.nextStepBoundary(blockStart);

    if (!wasPlaying_) {
        triggerStep(clock_.stepIndexAt(blockStart), pool, buffers);
        wasPlaying_ = true;
    }

    for (std::size_t i = 0; i < numSamples; ++i) {
        const double absoluteSample = blockStart + static_cast<double>(i);

        while (absoluteSample >= nextBoundary) {
            triggerStep(clock_.stepIndexAt(nextBoundary), pool, buffers);
            nextBoundary += clock_.samplesPerStep();
        }

        const auto frame = player_.processSample(pool, buffers);
        outLeft[i] = frame.left;
        outRight[i] = frame.right;
    }
}

} // namespace phraseator
