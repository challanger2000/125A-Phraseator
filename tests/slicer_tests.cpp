#include "slicer.h"

#include <cassert>

using namespace phraseator;

int main() {
    {
        const auto slices = Slicer::equalDivisions(16u, 4u);
        assert(slices.count == 4u);
        assert(slices.regions[0].startFrame == 0u);
        assert(slices.regions[0].endFrame == 4u);
        assert(slices.regions[3].startFrame == 12u);
        assert(slices.regions[3].endFrame == 16u);
    }

    {
        const auto slices = Slicer::equalDivisions(10u, 3u);
        assert(slices.count == 3u);
        assert(slices.regions[0].startFrame == 0u);
        assert(slices.regions[0].endFrame == 3u);
        assert(slices.regions[1].startFrame == 3u);
        assert(slices.regions[1].endFrame == 6u);
        assert(slices.regions[2].startFrame == 6u);
        assert(slices.regions[2].endFrame == 10u);
    }

    {
        const std::uint32_t boundaries[] {3u, 7u};
        const auto slices = Slicer::fromBoundaries(10u, boundaries, 2u);
        assert(slices.count == 3u);
        assert(slices.regions[0].startFrame == 0u);
        assert(slices.regions[0].endFrame == 3u);
        assert(slices.regions[1].startFrame == 3u);
        assert(slices.regions[1].endFrame == 7u);
        assert(slices.regions[2].startFrame == 7u);
        assert(slices.regions[2].endFrame == 10u);
    }

    {
        const std::uint32_t invalid[] {7u, 3u};
        const auto slices = Slicer::fromBoundaries(10u, invalid, 2u);
        assert(slices.count == 0u);
    }

    return 0;
}
