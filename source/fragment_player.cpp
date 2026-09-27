#include "fragment_player.h"

#include <algorithm>
#include <cmath>

namespace phraseator {

void FragmentPlayer::prepare(double outputSampleRate) noexcept {
    outputSampleRate_ =
        (std::isfinite(outputSampleRate) && outputSampleRate > 1000.0)
        ? outputSampleRate
        : 48000.0;
}

void FragmentPlayer::reset() noexcept {
    voices_ = {};
}

float FragmentPlayer::clamp(float v, float lo, float hi) noexcept {
    return std::clamp(v, lo, hi);
}

float FragmentPlayer::sampleLinear(const float* data, std::uint32_t frames, double position) noexcept {
    if (data == nullptr || frames == 0)
        return 0.0f;

    if (position <= 0.0)
        return data[0];

    const double last = static_cast<double>(frames - 1u);
    if (position >= last)
        return data[frames - 1u];

    const auto i0 = static_cast<std::uint32_t>(position);
    const auto i1 = i0 + 1u;
    const float frac = static_cast<float>(position - static_cast<double>(i0));
    return data[i0] + (data[i1] - data[i0]) * frac;
}

VoiceState* FragmentPlayer::acquireVoice() noexcept {
    for (auto& v : voices_) {
        if (!v.active)
            return &v;
    }

    // Deterministic voice stealing: replace voice 0 when all voices are active.
    return &voices_[0];
}

bool FragmentPlayer::trigger(const SourcePool& pool,
                             const std::array<AudioBufferView, kMaxSources>& buffers,
                             const FragmentRef& fragment,
                             float gain,
                             float pan,
                             float pitchSemitones) noexcept {
    const auto* source = pool.source(fragment.sourceIndex);
    if (source == nullptr || fragment.sliceIndex >= source->sliceCount)
        return false;

    const auto& buffer = buffers[fragment.sourceIndex];
    if (!buffer.valid() || buffer.frames < source->totalFrames)
        return false;

    const auto region = source->slices[fragment.sliceIndex];
    if (!region.valid() || region.endFrame > buffer.frames)
        return false;

    auto* voice = acquireVoice();
    voice->active = true;
    voice->fragment = fragment;
    voice->position = static_cast<double>(region.startFrame);
    voice->startFrame = region.startFrame;

    // Preserve original sample pitch/duration across host sample rates.
    // Source-rate conversion and creative pitch transpose are multiplicative.
    const double sourceRate =
        (std::isfinite(source->sampleRate) && source->sampleRate > 1000.0)
        ? source->sampleRate
        : outputSampleRate_;
    const double rateRatio = sourceRate / outputSampleRate_;
    const double pitchRatio =
        std::pow(2.0, static_cast<double>(pitchSemitones) / 12.0);
    voice->increment = rateRatio * pitchRatio;
    voice->gain = std::max(0.0f, gain);
    voice->pan = clamp(pan, -1.0f, 1.0f);
    voice->endFrame = region.endFrame;

    if (source->type == SourceType::Loop) {
        // EMPIRICALLY TUNED de-click baseline: 0.25 ms at the source rate,
        // bounded to at most half the slice length.
        const double requestedFade =
            std::max(1.0, std::round(sourceRate * 0.00025));
        const double maxFade =
            std::max(0.0, static_cast<double>(region.lengthFrames()) * 0.5);
        voice->fadeFrames = std::min(requestedFade, maxFade);
    } else {
        voice->fadeFrames = 0.0;
    }

    return true;
}

StereoFrame FragmentPlayer::processSample(const SourcePool& pool,
                                          const std::array<AudioBufferView, kMaxSources>& buffers) noexcept {
    StereoFrame out {};

    for (auto& voice : voices_) {
        if (!voice.active)
            continue;

        const auto* source = pool.source(voice.fragment.sourceIndex);
        if (source == nullptr) {
            voice.active = false;
            continue;
        }

        const auto& buffer = buffers[voice.fragment.sourceIndex];
        if (!buffer.valid() || voice.position >= static_cast<double>(voice.endFrame)) {
            voice.active = false;
            continue;
        }

        const float l = sampleLinear(buffer.left, buffer.frames, voice.position);
        const float r = buffer.stereo
            ? sampleLinear(buffer.right, buffer.frames, voice.position)
            : l;

        // Phraseator PAN is a placement macro, not a stereo-balance control.
        // At PAN=0 every source -- including stereo WAVs -- is true mono/center.
        // Non-zero values pan that mono-compatible signal with a constant-power law.
        const float mono = buffer.stereo ? 0.5f * (l + r) : l;
        const float pan01 = (voice.pan + 1.0f) * 0.5f;
        const float angle = pan01 * 1.57079632679f;
        const float gL = std::cos(angle) * voice.gain;
        const float gR = std::sin(angle) * voice.gain;

        float envelope = 1.0f;
        if (voice.fadeFrames > 0.0) {
            const double fromStart =
                voice.position - static_cast<double>(voice.startFrame);
            const double lastPlayable =
                static_cast<double>(voice.endFrame > 0u ? voice.endFrame - 1u : 0u);
            const double toEnd = lastPlayable - voice.position;

            const double fadeIn =
                std::clamp(fromStart / voice.fadeFrames, 0.0, 1.0);
            const double fadeOut =
                std::clamp(toEnd / voice.fadeFrames, 0.0, 1.0);
            envelope = static_cast<float>(std::min(fadeIn, fadeOut));
        }

        out.left += mono * gL * envelope;
        out.right += mono * gR * envelope;

        voice.position += voice.increment;
        if (voice.position >= static_cast<double>(voice.endFrame))
            voice.active = false;
    }

    return out;
}

std::size_t FragmentPlayer::activeVoiceCount() const noexcept {
    std::size_t count = 0;
    for (const auto& v : voices_) {
        if (v.active)
            ++count;
    }
    return count;
}

} // namespace phraseator
