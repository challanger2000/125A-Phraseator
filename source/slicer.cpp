#include "slicer.h"

#include <algorithm>
#include <cmath>
#include <vector>

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

SliceSet Slicer::transientDivisions(const AudioBufferView& audio,
                                    double sampleRate,
                                    std::size_t maxSlices) {
    SliceSet empty {};

    if (!audio.valid() || !std::isfinite(sampleRate) || sampleRate <= 1000.0 ||
        maxSlices < 2u) {
        return empty;
    }

    maxSlices = std::min<std::size_t>(maxSlices, kMaxSlicesPerSource);

    // EMPIRICALLY TUNED onset-analysis baseline:
    // 5 ms analysis hops and 40 ms minimum slice spacing.
    const std::size_t hop = std::clamp<std::size_t>(
        static_cast<std::size_t>(std::llround(sampleRate * 0.005)),
        16u, 2048u);
    const std::uint32_t minSpacing = static_cast<std::uint32_t>(
        std::max(1.0, std::round(sampleRate * 0.040)));

    const std::size_t blocks =
        (static_cast<std::size_t>(audio.frames) + hop - 1u) / hop;
    if (blocks < 3u)
        return empty;

    std::vector<double> envelope(blocks, 0.0);
    for (std::size_t block = 0; block < blocks; ++block) {
        const std::size_t start = block * hop;
        const std::size_t end = std::min<std::size_t>(
            start + hop, static_cast<std::size_t>(audio.frames));

        double sum = 0.0;
        for (std::size_t i = start; i < end; ++i) {
            const double l = std::fabs(static_cast<double>(audio.left[i]));
            const double r = audio.stereo
                ? std::fabs(static_cast<double>(audio.right[i]))
                : l;
            sum += 0.5 * (l + r);
        }

        const auto count = end > start ? end - start : 1u;
        envelope[block] = sum / static_cast<double>(count);
    }

    std::vector<double> flux(blocks, 0.0);
    double maxFlux = 0.0;
    std::vector<double> nonZeroFlux;
    nonZeroFlux.reserve(blocks);

    for (std::size_t i = 1; i < blocks; ++i) {
        const double value = std::max(0.0, envelope[i] - envelope[i - 1u]);
        flux[i] = value;
        maxFlux = std::max(maxFlux, value);
        if (value > 0.0)
            nonZeroFlux.push_back(value);
    }

    if (maxFlux < 1.0e-5 || nonZeroFlux.empty())
        return empty;

    std::sort(nonZeroFlux.begin(), nonZeroFlux.end());
    const double median = nonZeroFlux[nonZeroFlux.size() / 2u];
    const double threshold = std::max(median * 3.0, maxFlux * 0.18);

    struct Peak {
        std::uint32_t frame {0};
        double score {0.0};
    };
    std::vector<Peak> peaks;

    for (std::size_t i = 1; i + 1u < blocks; ++i) {
        if (flux[i] < threshold ||
            flux[i] < flux[i - 1u] ||
            flux[i] < flux[i + 1u]) {
            continue;
        }

        const auto frame = static_cast<std::uint32_t>(
            std::min<std::size_t>(
                i * hop,
                static_cast<std::size_t>(audio.frames - 1u)));

        if (frame < minSpacing ||
            frame + minSpacing >= audio.frames) {
            continue;
        }

        if (!peaks.empty() &&
            frame - peaks.back().frame < minSpacing) {
            if (flux[i] > peaks.back().score)
                peaks.back() = {frame, flux[i]};
            continue;
        }

        peaks.push_back({frame, flux[i]});
    }

    if (peaks.empty())
        return empty;

    const std::size_t maxBoundaries = maxSlices - 1u;
    if (peaks.size() > maxBoundaries) {
        std::sort(peaks.begin(), peaks.end(),
                  [](const Peak& a, const Peak& b) {
                      return a.score > b.score;
                  });
        peaks.resize(maxBoundaries);
        std::sort(peaks.begin(), peaks.end(),
                  [](const Peak& a, const Peak& b) {
                      return a.frame < b.frame;
                  });
    }

    std::array<std::uint32_t, kMaxSlicesPerSource - 1u> boundaries {};
    for (std::size_t i = 0; i < peaks.size(); ++i)
        boundaries[i] = peaks[i].frame;

    return fromBoundaries(
        audio.frames, boundaries.data(), peaks.size());
}

} // namespace phraseator
