#include "slicer.h"
#include "test_common.h"


using namespace phraseator;

int main() {
    {
        const auto slices = Slicer::equalDivisions(16u, 4u);
        CHECK(slices.count == 4u);
        CHECK(slices.regions[0].startFrame == 0u);
        CHECK(slices.regions[0].endFrame == 4u);
        CHECK(slices.regions[3].startFrame == 12u);
        CHECK(slices.regions[3].endFrame == 16u);
    }

    {
        const auto slices = Slicer::equalDivisions(10u, 3u);
        CHECK(slices.count == 3u);
        CHECK(slices.regions[0].startFrame == 0u);
        CHECK(slices.regions[0].endFrame == 3u);
        CHECK(slices.regions[1].startFrame == 3u);
        CHECK(slices.regions[1].endFrame == 6u);
        CHECK(slices.regions[2].startFrame == 6u);
        CHECK(slices.regions[2].endFrame == 10u);
    }

    {
        const std::uint32_t boundaries[] {3u, 7u};
        const auto slices = Slicer::fromBoundaries(10u, boundaries, 2u);
        CHECK(slices.count == 3u);
        CHECK(slices.regions[0].startFrame == 0u);
        CHECK(slices.regions[0].endFrame == 3u);
        CHECK(slices.regions[1].startFrame == 3u);
        CHECK(slices.regions[1].endFrame == 7u);
        CHECK(slices.regions[2].startFrame == 7u);
        CHECK(slices.regions[2].endFrame == 10u);
    }

    {
        const std::uint32_t invalid[] {7u, 3u};
        const auto slices = Slicer::fromBoundaries(10u, invalid, 2u);
        CHECK(slices.count == 0u);
    }

    return 0;
}
