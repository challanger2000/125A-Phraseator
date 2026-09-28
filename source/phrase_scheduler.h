#pragma once

#include "fragment_player.h"
#include "phrase_engine.h"
#include "source_pool.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace phraseator {

class PhraseScheduler {
public:
    void reset() noexcept;
    void setPattern(const Pattern& pattern) noexcept { pattern_ = pattern; }

    void prepare(double sampleRate) noexcept;
    void setPanAmount(float amount) noexcept { player_.setPanAmount(amount); }
    void chokeSource(std::uint16_t sourceIndex) noexcept { player_.chokeSource(sourceIndex); }

    // Time is expressed in absolute 16th-note step units, not historical
    // audio samples. One full step is 1.0. This keeps phrase phase stable
    // across tempo changes; stepsPerSample controls only the current playback
    // rate through the musical timeline.
    bool processBlock(const SourcePool& pool,
                      const std::array<AudioBufferView, kMaxSources>& buffers,
                      double startStepPosition,
                      double stepsPerSample,
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
                             double triggerStepPosition,
                             const SourcePool& pool,
                             const std::array<AudioBufferView, kMaxSources>& buffers) noexcept;

    void scheduleAbsoluteStep(std::uint64_t absoluteStep,
                              double currentStepPosition,
                              const SourcePool& pool,
                              const std::array<AudioBufferView, kMaxSources>& buffers) noexcept;

    void scheduleRatchets(std::uint64_t absoluteStep,
                          double actualTriggerStepPosition,
                          double currentStepPosition) noexcept;

    double stepTriggerPosition(std::uint64_t absoluteStep) const noexcept;

    Pattern pattern_ {};
    FragmentPlayer player_ {};
    std::uint64_t lastTriggeredAbsoluteStep_ {0u};
    std::uint64_t pendingAbsoluteStep_ {0u};
    double pendingStepPosition_ {0.0};
    double nextRatchetStepPosition_ {0.0};
    double ratchetIntervalSteps_ {0.0};
    std::uint8_t ratchetsRemaining_ {0u};
    bool hasTriggeredStep_ {false};
    bool hasPendingStep_ {false};
    bool timelineValid_ {false};
    double expectedNextStepPosition_ {0.0};
};

} // namespace phraseator
