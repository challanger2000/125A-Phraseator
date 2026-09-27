#pragma once

#include "audio_buffer.h"
#include "source_pool.h"

#include <array>
#include <cstddef>
#include <cstdint>

namespace phraseator {

struct VoiceState {
    bool active {false};
    FragmentRef fragment {};
    double position {0.0};
    double increment {1.0};
    float gain {1.0f};
    float pan {0.0f};
    std::uint32_t startFrame {0};
    std::uint32_t endFrame {0};
    double fadeFrames {0.0};
    std::uint32_t releaseSamplesRemaining {0};
    std::uint32_t releaseSamplesTotal {0};
};

struct StereoFrame {
    float left {0.0f};
    float right {0.0f};
};

class FragmentPlayer {
public:
    static constexpr std::size_t kMaxVoices = 16;

    void prepare(double outputSampleRate) noexcept;
    void reset() noexcept;

    bool trigger(const SourcePool& pool,
                 const std::array<AudioBufferView, kMaxSources>& buffers,
                 const FragmentRef& fragment,
                 float gain,
                 float pan,
                 float pitchSemitones) noexcept;

    StereoFrame processSample(const SourcePool& pool,
                              const std::array<AudioBufferView, kMaxSources>& buffers) noexcept;

    std::size_t activeVoiceCount() const noexcept;
    void chokeAll() noexcept;

private:
    static float clamp(float v, float lo, float hi) noexcept;
    static float sampleLinear(const float* data, std::uint32_t frames, double position) noexcept;
    VoiceState* acquireVoice() noexcept;

    std::array<VoiceState, kMaxVoices> voices_ {};
    double outputSampleRate_ {48000.0};
};

} // namespace phraseator
