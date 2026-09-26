#include "source_pool.h"

#include <algorithm>

namespace phraseator {

void SourcePool::clear() noexcept {
    sources_ = {};
}

bool SourcePool::setOneShot(std::size_t sourceIndex,
                            std::uint32_t sourceId,
                            std::uint32_t totalFrames,
                            double sampleRate,
                            bool stereo,
                            bool tonal,
                            float detectedRootMidi) noexcept {
    if (sourceIndex >= kMaxSources || totalFrames == 0 || sampleRate <= 0.0)
        return false;

    auto& s = sources_[sourceIndex];
    s = {};
    s.type = SourceType::OneShot;
    s.sourceId = sourceId;
    s.totalFrames = totalFrames;
    s.sampleRate = sampleRate;
    s.stereo = stereo;
    s.tonal = tonal;
    s.detectedRootMidi = detectedRootMidi;
    s.slices[0] = {0u, totalFrames};
    s.sliceCount = 1;
    return true;
}

bool SourcePool::setLoopSlices(std::size_t sourceIndex,
                               std::uint32_t sourceId,
                               std::uint32_t totalFrames,
                               double sampleRate,
                               bool stereo,
                               const SliceRegion* slices,
                               std::size_t sliceCount,
                               bool tonal,
                               float detectedRootMidi) noexcept {
    if (sourceIndex >= kMaxSources || totalFrames == 0 || sampleRate <= 0.0 ||
        slices == nullptr || sliceCount == 0 || sliceCount > kMaxSlicesPerSource)
        return false;

    SourceDescriptor candidate {};
    candidate.type = SourceType::Loop;
    candidate.sourceId = sourceId;
    candidate.totalFrames = totalFrames;
    candidate.sampleRate = sampleRate;
    candidate.stereo = stereo;
    candidate.tonal = tonal;
    candidate.detectedRootMidi = detectedRootMidi;

    std::uint32_t previousEnd = 0;
    for (std::size_t i = 0; i < sliceCount; ++i) {
        const auto region = slices[i];
        if (!region.valid() || region.endFrame > totalFrames)
            return false;
        if (i > 0 && region.startFrame < previousEnd)
            return false;

        candidate.slices[i] = region;
        previousEnd = region.endFrame;
    }

    candidate.sliceCount = static_cast<std::uint16_t>(sliceCount);
    sources_[sourceIndex] = candidate;
    return true;
}

const SourceDescriptor* SourcePool::source(std::size_t sourceIndex) const noexcept {
    if (sourceIndex >= kMaxSources)
        return nullptr;

    const auto& s = sources_[sourceIndex];
    return s.type == SourceType::Empty ? nullptr : &s;
}

std::size_t SourcePool::fragmentCount() const noexcept {
    std::size_t count = 0;
    for (const auto& s : sources_)
        count += s.sliceCount;
    return count;
}

bool SourcePool::fragmentAt(std::size_t flatIndex, FragmentRef& out) const noexcept {
    std::size_t cursor = 0;

    for (std::size_t sourceIndex = 0; sourceIndex < sources_.size(); ++sourceIndex) {
        const auto& s = sources_[sourceIndex];
        const auto next = cursor + s.sliceCount;

        if (flatIndex < next) {
            out.sourceIndex = static_cast<std::uint16_t>(sourceIndex);
            out.sliceIndex = static_cast<std::uint16_t>(flatIndex - cursor);
            return true;
        }

        cursor = next;
    }

    return false;
}

} // namespace phraseator
