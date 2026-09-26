#include "slicer.h"
#include "test_common.h"

#include <array>

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

    {
        constexpr std::uint32_t frames = 48000u;
        std::array<float, frames> pulse {};
        const std::uint32_t hits[] {0u, 12000u, 24000u, 36000u};

        for (const auto hit : hits) {
            for (std::uint32_t i = 0; i < 240u && hit + i < frames; ++i)
                pulse[hit + i] = 1.0f - static_cast<float>(i) / 240.0f;
        }

        AudioBufferView audio {pulse.data(), nullptr, frames, false};
        const auto slices = Slicer::transientDivisions(audio, 48000.0, 16u);

        CHECK(slices.count == 4u);
        CHECK(slices.regions[0].startFrame == 0u);
        CHECK(std::abs(static_cast<int>(slices.regions[0].endFrame) - 12000) <= 240);
        CHECK(std::abs(static_cast<int>(slices.regions[1].endFrame) - 24000) <= 240);
        CHECK(std::abs(static_cast<int>(slices.regions[2].endFrame) - 36000) <= 240);
        CHECK(slices.regions[3].endFrame == frames);
    }

    {
        std::array<float, 4096> flat {};
        flat.fill(0.25f);
        AudioBufferView audio {flat.data(), nullptr,
                               static_cast<std::uint32_t>(flat.size()), false};
        const auto slices = Slicer::transientDivisions(audio, 48000.0, 16u);
        CHECK(slices.count == 0u);
    }

    return 0;
}
