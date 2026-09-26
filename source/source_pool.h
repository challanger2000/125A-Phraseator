#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace phraseator {

constexpr std::size_t kMaxSources = 16;
constexpr std::size_t kMaxSlicesPerSource = 64;
constexpr std::size_t kMaxFragmentsTotal = kMaxSources * kMaxSlicesPerSource;

enum class SourceType : std::uint8_t {
    Empty = 0,
    OneShot,
    Loop
};

struct SliceRegion {
    std::uint32_t startFrame {0};
    std::uint32_t endFrame {0};

    bool valid() const noexcept {
        return endFrame > startFrame;
    }

    std::uint32_t lengthFrames() const noexcept {
        return valid() ? (endFrame - startFrame) : 0u;
    }
};

struct SourceDescriptor {
    SourceType type {SourceType::Empty};
    std::uint32_t sourceId {0};
    std::uint32_t totalFrames {0};
    double sampleRate {0.0};
    bool stereo {false};
    bool tonal {false};
    float detectedRootMidi {-1.0f};

    std::array<SliceRegion, kMaxSlicesPerSource> slices {};
    std::uint16_t sliceCount {0};
};

struct FragmentRef {
    std::uint16_t sourceIndex {0};
    std::uint16_t sliceIndex {0};
};

class SourcePool {
public:
    void clear() noexcept;

    bool setOneShot(std::size_t sourceIndex,
                    std::uint32_t sourceId,
                    std::uint32_t totalFrames,
                    double sampleRate,
                    bool stereo,
                    bool tonal = false,
                    float detectedRootMidi = -1.0f) noexcept;

    bool setLoopSlices(std::size_t sourceIndex,
                       std::uint32_t sourceId,
                       std::uint32_t totalFrames,
                       double sampleRate,
                       bool stereo,
                       const SliceRegion* slices,
                       std::size_t sliceCount,
                       bool tonal = false,
                       float detectedRootMidi = -1.0f) noexcept;

    const SourceDescriptor* source(std::size_t sourceIndex) const noexcept;

    std::size_t fragmentCount() const noexcept;
    bool fragmentAt(std::size_t flatIndex, FragmentRef& out) const noexcept;

private:
    std::array<SourceDescriptor, kMaxSources> sources_ {};
};

} // namespace phraseator
