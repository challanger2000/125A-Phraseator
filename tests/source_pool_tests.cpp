#include "source_pool.h"
#include "test_common.h"

#include <limits>


using namespace phraseator;

int main() {
    SourcePool pool;

    CHECK(pool.fragmentCount() == 0);

    CHECK(pool.setOneShot(0, 1001u, 4800u, 48000.0, false));
    CHECK(pool.fragmentCount() == 1);

    SliceRegion loopSlices[4] {
        {0u, 12000u},
        {12000u, 24000u},
        {24000u, 36000u},
        {36000u, 48000u}
    };

    CHECK(pool.setLoopSlices(1, 2001u, 48000u, 48000.0, true, loopSlices, 4));
    CHECK(pool.fragmentCount() == 5);

    FragmentRef ref {};
    CHECK(pool.fragmentAt(0, ref));
    CHECK(ref.sourceIndex == 0 && ref.sliceIndex == 0);

    CHECK(pool.fragmentAt(1, ref));
    CHECK(ref.sourceIndex == 1 && ref.sliceIndex == 0);

    CHECK(pool.fragmentAt(4, ref));
    CHECK(ref.sourceIndex == 1 && ref.sliceIndex == 3);

    CHECK(!pool.fragmentAt(5, ref));

    SliceRegion invalid[2] {
        {0u, 30000u},
        {20000u, 40000u}
    };

    CHECK(!pool.setLoopSlices(2, 3001u, 48000u, 48000.0, false, invalid, 2));
    CHECK(pool.fragmentCount() == 5);

    pool.clear();
    CHECK(pool.fragmentCount() == 0);

    {
        // Source selection used by the manual pattern editor must map to the
        // first fragment of that source independent of how many slices earlier
        // sources contain.
        SourcePool mapped;
        SliceRegion loopSlices[3] {{0u, 10u}, {10u, 20u}, {20u, 30u}};
        CHECK(mapped.setLoopSlices(0, 10u, 30u, 48000.0, false,
                                   loopSlices, 3u));
        CHECK(mapped.setOneShot(1, 11u, 20u, 48000.0, false));

        std::size_t flat = 999u;
        CHECK(mapped.flatIndexOf({0u, 0u}, flat));
        CHECK(flat == 0u);

        CHECK(mapped.flatIndexOf({1u, 0u}, flat));
        CHECK(flat == 3u);

        FragmentRef back {};
        CHECK(mapped.fragmentAt(flat, back));
        CHECK(back.sourceIndex == 1u);
        CHECK(back.sliceIndex == 0u);
    }


    {
        SourcePool numeric;
        CHECK(!numeric.setOneShot(
            0u, 1u, 100u,
            std::numeric_limits<double>::quiet_NaN(), false));

        CHECK(numeric.setOneShot(
            0u, 2u, 100u, 48000.0, false,
            true, std::numeric_limits<float>::quiet_NaN()));
        const auto* source = numeric.source(0u);
        CHECK(source != nullptr);
        CHECK(!source->tonal);
        CHECK(source->detectedRootMidi < 0.0f);
    }

    return 0;
}
