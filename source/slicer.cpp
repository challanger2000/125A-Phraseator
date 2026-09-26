#include "slicer.h"

#include <algorithm>

namespace phraseator {

SliceSet Slicer::equalDivisions(std::uint32_t totalFrames,
                                std::size_t divisions) noexcept {
    SliceSet result {};

    if (totalFrames == 0 || divisions == 0 || divisions > kMaxSlicesPerSource)
        return result;

    const std::size_t safeDivisions =
        std::min<std::size_t>(divisions, static_cast<std::size_t>(totalFrames));

    for (std::size_t i = 0; i < safeDivisions; ++i) {
        const auto start = static_cast<std::uint32_t>(
            (static_cast<std::uint64_t>(totalFrames) * i) / safeDivisions);
        const auto end = static_cast<std::uint32_t>(
            (static_cast<std::uint64_t>(totalFrames) * (i + 1u)) / safeDivisions);

        if (end > start) {
            result.regions[result.count++] = {start, end};
        }
    }

    return result;
}

SliceSet Slicer::fromBoundaries(std::uint32_t totalFrames,
                                const std::uint32_t* boundaries,
                                std::size_t boundaryCount) noexcept {
    SliceSet result {};

    if (totalFrames == 0 || boundaries == nullptr ||
        boundaryCount == 0 || boundaryCount > (kMaxSlicesPerSource - 1u))
        return result;

    std::uint32_t previous = 0u;

    for (std::size_t i = 0; i < boundaryCount; ++i) {
        const auto boundary = boundaries[i];

        if (boundary <= previous || boundary >= totalFrames)
            return {};

        result.regions[result.count++] = {previous, boundary};
        previous = boundary;
    }

    if (previous < totalFrames)
        result.regions[result.count++] = {previous, totalFrames};

    return result;
}

} // namespace phraseator
