#include "source_pool.h"

#include <cassert>

using namespace phraseator;

int main() {
    SourcePool pool;

    assert(pool.fragmentCount() == 0);

    assert(pool.setOneShot(0, 1001u, 4800u, 48000.0, false));
    assert(pool.fragmentCount() == 1);

    SliceRegion loopSlices[4] {
        {0u, 12000u},
        {12000u, 24000u},
        {24000u, 36000u},
        {36000u, 48000u}
    };

    assert(pool.setLoopSlices(1, 2001u, 48000u, 48000.0, true, loopSlices, 4));
    assert(pool.fragmentCount() == 5);

    FragmentRef ref {};
    assert(pool.fragmentAt(0, ref));
    assert(ref.sourceIndex == 0 && ref.sliceIndex == 0);

    assert(pool.fragmentAt(1, ref));
    assert(ref.sourceIndex == 1 && ref.sliceIndex == 0);

    assert(pool.fragmentAt(4, ref));
    assert(ref.sourceIndex == 1 && ref.sliceIndex == 3);

    assert(!pool.fragmentAt(5, ref));

    SliceRegion invalid[2] {
        {0u, 30000u},
        {20000u, 40000u}
    };

    assert(!pool.setLoopSlices(2, 3001u, 48000u, 48000.0, false, invalid, 2));
    assert(pool.fragmentCount() == 5);

    pool.clear();
    assert(pool.fragmentCount() == 0);

    return 0;
}
