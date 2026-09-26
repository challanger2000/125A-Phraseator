#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace phraseator {

constexpr std::size_t kMaxAudioFramesPerSource = 48000u * 30u; // 30 s @ 48 kHz baseline

struct AudioBufferView {
    const float* left {nullptr};
    const float* right {nullptr};
    std::uint32_t frames {0};
    bool stereo {false};

    bool valid() const noexcept {
        if (left == nullptr || frames == 0)
            return false;
        if (stereo && right == nullptr)
            return false;
        return true;
    }
};

} // namespace phraseator
