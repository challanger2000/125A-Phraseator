#include "pattern_fragment_remap.h"

namespace phraseator {

PatternFragmentSnapshot snapshotPatternFragments(
    const Pattern& pattern,
    const SourcePool& pool) noexcept {

    PatternFragmentSnapshot snapshot {};

    for (std::size_t i = 0; i < pattern.size(); ++i) {
        if (!pattern[i].active)
            continue;

        FragmentRef ref {};
        if (pool.fragmentAt(pattern[i].fragment, ref)) {
            snapshot[i].valid = true;
            snapshot[i].fragment = ref;
        }
    }

    return snapshot;
}

PatternFragmentSnapshot snapshotPatternFragmentsFromSourceCounts(
    const Pattern& pattern,
    const std::array<std::uint16_t, kMaxSources>& sourceSliceCounts) noexcept {

    PatternFragmentSnapshot snapshot {};

    for (std::size_t stepIndex = 0; stepIndex < pattern.size(); ++stepIndex) {
        if (!pattern[stepIndex].active)
            continue;

        const auto flatIndex = static_cast<std::size_t>(pattern[stepIndex].fragment);
        std::size_t cursor = 0;

        for (std::size_t sourceIndex = 0;
             sourceIndex < sourceSliceCounts.size();
             ++sourceIndex) {
            const auto count = static_cast<std::size_t>(sourceSliceCounts[sourceIndex]);
            if (count == 0u)
                continue;

            if (flatIndex < cursor + count) {
                snapshot[stepIndex].valid = true;
                snapshot[stepIndex].fragment.sourceIndex =
                    static_cast<std::uint16_t>(sourceIndex);
                snapshot[stepIndex].fragment.sliceIndex =
                    static_cast<std::uint16_t>(flatIndex - cursor);
                break;
            }

            cursor += count;
        }
    }

    return snapshot;
}

bool restorePatternFragments(
    Pattern& pattern,
    const PatternFragmentSnapshot& snapshot,
    const SourcePool& pool) noexcept {

    bool changed = false;

    for (std::size_t i = 0; i < pattern.size(); ++i) {
        auto& step = pattern[i];
        if (!step.active)
            continue;

        if (!snapshot[i].valid) {
            step.active = false;
            changed = true;
            continue;
        }

        std::size_t flatIndex = 0;
        if (!pool.flatIndexOf(snapshot[i].fragment, flatIndex)) {
            step.active = false;
            changed = true;
            continue;
        }

        const auto mapped = static_cast<std::uint16_t>(flatIndex);
        if (step.fragment != mapped) {
            step.fragment = mapped;
            changed = true;
        }
    }

    return changed;
}

} // namespace phraseator
