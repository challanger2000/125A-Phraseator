#include "fragment_player.h"

#include <algorithm>
#include <cmath>

namespace phraseator {

void FragmentPlayer::prepare(double outputSampleRate) noexcept {
    outputSampleRate_ =
        (std::isfinite(outputSampleRate) && outputSampleRate >= 1000.0)
        ? outputSampleRate
        : 48000.0;
    panSmoothingCoeff_ = static_cast<float>(
        1.0 - std::exp(-1.0 / (outputSampleRate_ * 0.005)));
}

void FragmentPlayer::reset() noexcept {
    voices_ = {};
    panAmountCurrent_ = panAmountTarget_;
}

void FragmentPlayer::chokeAll() noexcept {
    // 2 ms de-click release, constant in time across host sample rates.
    const auto safeRelease = static_cast<std::uint32_t>(
        std::max(1.0, std::round(outputSampleRate_ * 0.002)));
    for (auto& voice : voices_) {
        if (!voice.active)
            continue;
        voice.releaseSamplesRemaining = safeRelease;
        voice.releaseSamplesTotal = safeRelease;
    }
}

void FragmentPlayer::chokeSource(std::uint16_t sourceIndex) noexcept {
    const auto safeRelease = static_cast<std::uint32_t>(
        std::max(1.0, std::round(outputSampleRate_ * 0.002)));
    for (auto& voice : voices_) {
        if (!voice.active || voice.fragment.sourceIndex != sourceIndex)
            continue;
        voice.releaseSamplesRemaining = safeRelease;
        voice.releaseSamplesTotal = safeRelease;
    }
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
        (std::isfinite(source->sampleRate) && source->sampleRate > 0.0)
        ? source->sampleRate
        : outputSampleRate_;
    const double rateRatio = sourceRate / outputSampleRate_;
    const double pitchRatio =
        std::pow(2.0, static_cast<double>(pitchSemitones) / 12.0);
    voice->increment = rateRatio * pitchRatio;
    voice->gain = std::max(0.0f, gain);
    voice->panShape = clamp(pan, -1.0f, 1.0f);
    voice->endFrame = region.endFrame;
    voice->releaseSamplesRemaining = 0u;
    voice->releaseSamplesTotal = 0u;

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

    // Smooth live PAN automation once per output sample. Doing this inside the
    // voice loop would make the ramp speed depend on temporary voice overlap.
    panAmountCurrent_ +=
        (panAmountTarget_ - panAmountCurrent_) * panSmoothingCoeff_;

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

        // Clamp interpolation to the current fragment region. Passing the
        // region end as the effective frame count prevents the final
        // fractional sample of a slice from reading into the next slice.
        const float l = sampleLinear(buffer.left, voice.endFrame, voice.position);
        const float r = buffer.stereo
            ? sampleLinear(buffer.right, voice.endFrame, voice.position)
            : l;

        // Preserve native stereo at PAN=0. Stereo material is never summed
        // to mono merely to place it: the PAN macro behaves as a balance
        // control for stereo sources, attenuating only the opposite side.
        // Mono sources keep the existing constant-power pan law.
        const float livePan =
            clamp(voice.panShape * panAmountCurrent_, -1.0f, 1.0f);

        float sourceL = l;
        float sourceR = r;
        float gL = voice.gain;
        float gR = voice.gain;

        if (buffer.stereo) {
            const float magnitude = std::fabs(livePan);
            const float oppositeGain =
                std::cos(magnitude * 1.57079632679f);

            if (livePan > 0.0f)
                gL *= oppositeGain;
            else if (livePan < 0.0f)
                gR *= oppositeGain;
        } else {
            const float pan01 = (livePan + 1.0f) * 0.5f;
            const float angle = pan01 * 1.57079632679f;
            sourceR = sourceL;
            gL *= std::cos(angle);
            gR *= std::sin(angle);
        }

        float envelope = 1.0f;
        if (voice.releaseSamplesRemaining > 0u &&
            voice.releaseSamplesTotal > 0u) {
            envelope *= static_cast<float>(voice.releaseSamplesRemaining) /
                        static_cast<float>(voice.releaseSamplesTotal);
        }

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
            envelope *= static_cast<float>(std::min(fadeIn, fadeOut));
        }

        out.left += sourceL * gL * envelope;
        out.right += sourceR * gR * envelope;

        voice.position += voice.increment;
        if (voice.releaseSamplesRemaining > 0u) {
            --voice.releaseSamplesRemaining;
            if (voice.releaseSamplesRemaining == 0u)
                voice.active = false;
        }
        if (voice.position >= static_cast<double>(voice.endFrame))
            voice.active = false;
    }

    return out;
}

void FragmentPlayer::setPanAmount(float amount) noexcept {
    panAmountTarget_ = clamp(amount, 0.0f, 1.0f);

    // With no sounding voice there is nothing to de-click. Snap the dormant
    // state so the next trigger begins exactly at the requested placement.
    if (activeVoiceCount() == 0u)
        panAmountCurrent_ = panAmountTarget_;
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
