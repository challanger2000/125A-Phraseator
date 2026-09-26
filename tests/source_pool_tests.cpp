#include "source_pool.h"
#include "test_common.h"


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

    return 0;
}
