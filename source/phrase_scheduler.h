#pragma once

#include "fragment_player.h"
#include "phrase_engine.h"
#include "source_pool.h"
#include "step_clock.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace phraseator {

class PhraseScheduler {
public:
    void reset() noexcept;
    void setPattern(const Pattern& pattern) noexcept { pattern_ = pattern; }

    void prepare(double sampleRate, double tempoBpm) noexcept;
    void setPanAmount(float amount) noexcept { player_.setPanAmount(amount); }

    bool processBlock(const SourcePool& pool,
                      const std::array<AudioBufferView, kMaxSources>& buffers,
                      double projectTimeSamples,
                      bool playing,
                      float* outLeft,
                      float* outRight,
                      std::size_t numSamples) noexcept;

private:
    void resetPlaybackState() noexcept;

    void triggerStep(std::size_t stepIndex,
                     const SourcePool& pool,
                     const std::array<AudioBufferView, kMaxSources>& buffers) noexcept;

    void triggerAbsoluteStep(std::uint64_t absoluteStep,
                             double triggerTimeSamples,
                             const SourcePool& pool,
                             const std::array<AudioBufferView, kMaxSources>& buffers) noexcept;

    void scheduleAbsoluteStep(std::uint64_t absoluteStep,
                              double blockTimeSamples,
                              const SourcePool& pool,
                              const std::array<AudioBufferView, kMaxSources>& buffers) noexcept;

    void scheduleRatchets(std::uint64_t absoluteStep,
                          double actualStepTriggerTimeSamples,
                          double currentTimeSamples) noexcept;

    double stepTriggerTime(std::uint64_t absoluteStep) const noexcept;

    Pattern pattern_ {};
    StepClock clock_ {};
    FragmentPlayer player_ {};
    std::uint64_t lastTriggeredAbsoluteStep_ {0u};
    std::uint64_t pendingAbsoluteStep_ {0u};
    double pendingStepTimeSamples_ {0.0};
    double nextRatchetTimeSamples_ {0.0};
    double ratchetIntervalSamples_ {0.0};
    std::uint8_t ratchetsRemaining_ {0u};
    bool hasTriggeredStep_ {false};
    bool hasPendingStep_ {false};
    bool timelineValid_ {false};
    double expectedNextProjectTimeSamples_ {0.0};
};

} // namespace phraseator
