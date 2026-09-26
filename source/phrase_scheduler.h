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

    void processBlock(const SourcePool& pool,
                      const std::array<AudioBufferView, kMaxSources>& buffers,
                      double projectTimeSamples,
                      bool playing,
                      float* outLeft,
                      float* outRight,
                      std::size_t numSamples) noexcept;

private:
    void triggerStep(std::size_t stepIndex,
                     const SourcePool& pool,
                     const std::array<AudioBufferView, kMaxSources>& buffers) noexcept;

    Pattern pattern_ {};
    StepClock clock_ {};
    FragmentPlayer player_ {};
    bool wasPlaying_ {false};
};

} // namespace phraseator
