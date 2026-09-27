#pragma once

#include "phrase_engine.h"
#include "source_pool.h"

#include <array>
#include <cstddef>

namespace phraseator {

struct PatternFragmentIdentity {
    bool valid {false};
    FragmentRef fragment {};
};

using PatternFragmentSnapshot =
    std::array<PatternFragmentIdentity, kStepCount>;

PatternFragmentSnapshot snapshotPatternFragments(
    const Pattern& pattern,
    const SourcePool& pool) noexcept;

bool restorePatternFragments(
    Pattern& pattern,
    const PatternFragmentSnapshot& snapshot,
    const SourcePool& pool) noexcept;

} // namespace phraseator
