#include "pattern_fragment_remap.h"
#include "test_common.h"

using namespace phraseator;

int main() {
    SourcePool oldPool;
    CHECK(oldPool.setOneShot(0u, 10u, 100u, 48000.0, false));

    SliceRegion loopSlices[3] {
        {0u, 100u},
        {100u, 200u},
        {200u, 300u}
    };
    CHECK(oldPool.setLoopSlices(
        1u, 20u, 300u, 48000.0, false, loopSlices, 3u));

    Pattern pattern {};
    pattern[0].active = true;
    pattern[0].fragment = 0u; // source 0, slice 0
    pattern[1].active = true;
    pattern[1].fragment = 2u; // source 1, slice 1

    const auto snapshot = snapshotPatternFragments(pattern, oldPool);
    CHECK(snapshot[0].valid);
    CHECK(snapshot[0].fragment.sourceIndex == 0u);
    CHECK(snapshot[1].valid);
    CHECK(snapshot[1].fragment.sourceIndex == 1u);
    CHECK(snapshot[1].fragment.sliceIndex == 1u);

    // Insert a new two-slice source before source slot 1. The flat index for
    // the old source-1/slice-1 identity must move from 2 to 4, not silently
    // keep pointing at whatever now occupies flat index 2.
    SourcePool insertedPool;
    SliceRegion insertedSlices[2] {
        {0u, 50u},
        {50u, 100u}
    };
    CHECK(insertedPool.setOneShot(0u, 11u, 100u, 48000.0, false));
    CHECK(insertedPool.setLoopSlices(
        1u, 21u, 100u, 48000.0, false, insertedSlices, 2u));
    CHECK(insertedPool.setLoopSlices(
        2u, 20u, 300u, 48000.0, false, loopSlices, 3u));

    // Snapshot is slot-based by design: replacing a slot means the pattern
    // follows the replacement in that slot. A shift caused by other slots is
    // what must never retarget the pattern accidentally.
    Pattern shifted {};
    shifted[0].active = true;
    shifted[0].fragment = 0u;
    shifted[1].active = true;
    shifted[1].fragment = 2u;

    // More representative layout shift: source 0 is cleared, so source 1
    // fragments compact from flat indices 1..3 down to 0..2.
    SourcePool clearedPool;
    CHECK(clearedPool.setLoopSlices(
        1u, 20u, 300u, 48000.0, false, loopSlices, 3u));

    Pattern compacted = pattern;
    CHECK(restorePatternFragments(compacted, snapshot, clearedPool));
    CHECK(!compacted[0].active); // source 0 disappeared
    CHECK(compacted[1].active);
    CHECK(compacted[1].fragment == 1u); // source 1 / slice 1 preserved

    // Project recall can reconstruct the same identity snapshot from stored
    // per-source slice counts even when the realtime pool starts empty.
    std::array<std::uint16_t, kMaxSources> savedCounts {};
    savedCounts[0] = 1u;
    savedCounts[1] = 3u;

    const auto recallSnapshot =
        snapshotPatternFragmentsFromSourceCounts(pattern, savedCounts);

    CHECK(recallSnapshot[0].valid);
    CHECK(recallSnapshot[0].fragment.sourceIndex == 0u);
    CHECK(recallSnapshot[0].fragment.sliceIndex == 0u);
    CHECK(recallSnapshot[1].valid);
    CHECK(recallSnapshot[1].fragment.sourceIndex == 1u);
    CHECK(recallSnapshot[1].fragment.sliceIndex == 1u);

    Pattern partialRecall = pattern;
    CHECK(restorePatternFragments(partialRecall, recallSnapshot, clearedPool));
    CHECK(!partialRecall[0].active);
    CHECK(partialRecall[1].active);
    CHECK(partialRecall[1].fragment == 1u);

    // Replacing the same slot with fewer slices deactivates only steps whose
    // specific slice no longer exists.
    SourcePool shorterPool;
    SliceRegion shortSlices[1] {{0u, 300u}};
    CHECK(shorterPool.setOneShot(0u, 10u, 100u, 48000.0, false));
    CHECK(shorterPool.setLoopSlices(
        1u, 22u, 300u, 48000.0, false, shortSlices, 1u));

    Pattern shortened = pattern;
    CHECK(restorePatternFragments(shortened, snapshot, shorterPool));
    CHECK(shortened[0].active);
    CHECK(!shortened[1].active);

    return 0;
}
