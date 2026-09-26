#pragma once

#include "source_pool.h"
#include "audio_buffer.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace phraseator {

struct SliceSet {
    std::array<SliceRegion, kMaxSlicesPerSource> regions {};
    std::size_t count {0};
};

class Slicer {
public:
    static SliceSet equalDivisions(std::uint32_t totalFrames,
                                   std::size_t divisions) noexcept;

    static SliceSet fromBoundaries(std::uint32_t totalFrames,
                                   const std::uint32_t* boundaries,
                                   std::size_t boundaryCount) noexcept;

    static SliceSet transientDivisions(const AudioBufferView& audio,
                                       double sampleRate,
                                       std::size_t maxSlices);
};

} // namespace phraseator
