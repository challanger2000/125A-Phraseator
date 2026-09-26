#pragma once

#include "audio_buffer.h"

#include <cstdint>
#include <vector>

namespace phraseator {

struct OwnedAudioSource {
    std::vector<float> left;
    std::vector<float> right;
    std::uint32_t sampleRate {0};
    bool stereo {false};

    bool valid() const noexcept {
        if (sampleRate == 0 || left.empty())
            return false;
        if (stereo && right.size() != left.size())
            return false;
        return true;
    }

    std::uint32_t frames() const noexcept {
        return static_cast<std::uint32_t>(left.size());
    }

    AudioBufferView view() const noexcept {
        if (!valid())
            return {};

        return {
            left.data(),
            stereo ? right.data() : nullptr,
            frames(),
            stereo
        };
    }

    void clear() noexcept {
        left.clear();
        right.clear();
        sampleRate = 0;
        stereo = false;
    }
};

} // namespace phraseator
