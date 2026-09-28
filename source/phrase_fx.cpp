#include "phrase_fx.h"

#include <algorithm>
#include <cmath>

namespace phraseator {

namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr float kDenormalFloor = 1.0e-20f;
constexpr double kMinTempoBpm = 20.0;
constexpr double kMaxDelayQuarterMultiplier = 1.0;

float killDenormal(float value) noexcept {
    return std::fabs(value) < kDenormalFloor ? 0.0f : value;
}

double delayQuarterMultiplier(std::int32_t division) noexcept {
    switch (std::clamp(division, 0, 7)) {
        case 0: return 0.0;        // OFF
        case 1: return 1.0;        // 1/4
        case 2: return 0.5;        // 1/8
        case 3: return 0.75;       // 1/8 dotted
        case 4: return 1.0 / 3.0;  // 1/8 triplet
        case 5: return 0.25;       // 1/16
        case 6: return 0.375;      // 1/16 dotted
        case 7: return 1.0 / 6.0;  // 1/16 triplet
        default: return 0.5;
    }
}

double musicalFilterCutoff(double amount, double sampleRate, std::int32_t mode) noexcept {
    amount = std::clamp(amount, 0.0, 1.0);
    const double maxCutoff = std::min(20000.0, sampleRate * 0.45);

    // 125A macro scaling:
    // 0% bypass
    // 25% ~14 kHz: subtle
    // 50% ~8 kHz: musical
    // 75% ~3.5 kHz: obvious
    // 100% ~1.2 kHz: strong creative darkening
    if (mode == 0) {
        constexpr double minCutoff = 1200.0;
        return maxCutoff * std::pow(minCutoff / maxCutoff, amount);
    }

    constexpr double minCutoff = 20.0;
    constexpr double maxHighPass = 4000.0;
    return minCutoff * std::pow(maxHighPass / minCutoff, amount);
}

} // namespace

float PhraseFx::clamp01(float value) noexcept {
    if (!std::isfinite(value))
        return 0.0f;
    return std::clamp(value, 0.0f, 1.0f);
}

void PhraseFx::prepare(double sampleRate) {
    sampleRate_ = (std::isfinite(sampleRate) && sampleRate >= 1000.0)
        ? sampleRate
        : 48000.0;

    // Longest supported division is 1/4. Size the buffer from the same
    // minimum tempo accepted by processBlock so tempo sync never silently
    // clamps at slow project tempos.
    const double maxDelaySeconds =
        (60.0 / kMinTempoBpm) * kMaxDelayQuarterMultiplier;
    const auto maxDelaySamples = static_cast<std::size_t>(
        std::ceil(sampleRate_ * maxDelaySeconds));

    delayLeft_.assign(maxDelaySamples + 2u, 0.0f);
    delayRight_.assign(maxDelaySamples + 2u, 0.0f);
    delayGenerations_.assign(maxDelaySamples + 2u, 0u);

    reset();
}

void PhraseFx::reset() noexcept {
    std::fill(delayLeft_.begin(), delayLeft_.end(), 0.0f);
    std::fill(delayRight_.begin(), delayRight_.end(), 0.0f);
    std::fill(delayGenerations_.begin(), delayGenerations_.end(), 0u);
    delayGeneration_ = 1u;

    writeIndex_ = 0u;
    delayCurrent_ = delayTarget_;
    delaySamplesCurrent_ = sampleRate_ * 0.25;

    filterCurrent_ = filterTarget_;
    filterStateL_ = 0.0f;
    filterStateR_ = 0.0f;
}

void PhraseFx::setDelayAmount(float amount) noexcept {
    const float next = clamp01(amount);

    if (next == 0.0f && delayTarget_ > 0.0f) {
        // Realtime-safe logical clear. Reset the smoothed mix as well: a later
        // re-enable must ramp up from dry rather than start 100% wet against
        // an intentionally empty history buffer.
        ++delayGeneration_;
        if (delayGeneration_ == 0u)
            delayGeneration_ = 1u;
        writeIndex_ = 0u;
        delayCurrent_ = 0.0f;
    }

    delayTarget_ = next;
}

void PhraseFx::setDelayDivision(std::int32_t division) noexcept {
    const auto next = std::clamp<std::int32_t>(division, 0, 7);
    if (next == 0 && delayDivision_ != 0) {
        ++delayGeneration_;
        if (delayGeneration_ == 0u)
            delayGeneration_ = 1u;
        writeIndex_ = 0u;
        delayCurrent_ = 0.0f;
    }
    delayDivision_ = next;
}

void PhraseFx::setFilterAmount(float amount) noexcept {
    filterTarget_ = clamp01(amount);
}

void PhraseFx::setFilterMode(std::int32_t mode) noexcept {
    filterMode_ = mode == 0 ? 0 : 1;
}

float PhraseFx::readDelay(const std::vector<float>& buffer,
                          const std::vector<std::uint64_t>& generations,
                          double readPosition) const noexcept {
    if (buffer.empty())
        return 0.0f;

    const double size = static_cast<double>(buffer.size());

    while (readPosition < 0.0)
        readPosition += size;
    while (readPosition >= size)
        readPosition -= size;

    const auto i0 = static_cast<std::size_t>(readPosition);
    const auto i1 = (i0 + 1u) % buffer.size();
    const float frac = static_cast<float>(readPosition - static_cast<double>(i0));

    const float s0 =
        i0 < generations.size() && generations[i0] == delayGeneration_
            ? buffer[i0] : 0.0f;
    const float s1 =
        i1 < generations.size() && generations[i1] == delayGeneration_
            ? buffer[i1] : 0.0f;

    return s0 + (s1 - s0) * frac;
}

bool PhraseFx::processBlock(float* left,
                            float* right,
                            std::size_t numSamples,
                            double tempoBpm) noexcept {
    if (!left || !right || numSamples == 0u)
        return false;

    const double tempo = (std::isfinite(tempoBpm) && tempoBpm > 1.0)
        ? std::clamp(tempoBpm, kMinTempoBpm, 400.0)
        : 120.0;

    const double quarterSamples = sampleRate_ * 60.0 / tempo;
    const bool delayEnabled = delayDivision_ != 0;
    const double delaySamplesTarget = delayEnabled
        ? std::clamp(
            quarterSamples * delayQuarterMultiplier(delayDivision_),
            1.0,
            static_cast<double>(delayLeft_.empty() ? 1u : delayLeft_.size() - 2u))
        : delaySamplesCurrent_;

    const double amountCoeff = 1.0 - std::exp(-1.0 / (sampleRate_ * 0.020));
    const double timeCoeff = 1.0 - std::exp(-1.0 / (sampleRate_ * 0.050));

    bool producedAudio = false;

    for (std::size_t i = 0; i < numSamples; ++i) {
        const float effectiveDelayTarget =
            delayEnabled ? delayTarget_ : 0.0f;
        delayCurrent_ += static_cast<float>(
            (static_cast<double>(effectiveDelayTarget) - delayCurrent_) * amountCoeff);

        filterCurrent_ += static_cast<float>(
            (static_cast<double>(filterTarget_) - filterCurrent_) * amountCoeff);

        delaySamplesCurrent_ +=
            (delaySamplesTarget - delaySamplesCurrent_) * timeCoeff;

        float inL = left[i];
        float inR = right[i];

        if (!delayLeft_.empty()) {
            const bool delayActive =
                delayEnabled &&
                (delayTarget_ > 0.0f || delayCurrent_ > 0.000001f);

            if (delayActive) {
                const double readPos =
                    static_cast<double>(writeIndex_) - delaySamplesCurrent_;

                const float delayedL = readDelay(delayLeft_, delayGenerations_, readPos);
                const float delayedR = readDelay(delayRight_, delayGenerations_, readPos);

                constexpr float feedback = 0.34f;
                delayLeft_[writeIndex_] =
                    killDenormal(inL + delayedR * feedback);
                delayRight_[writeIndex_] =
                    killDenormal(inR + delayedL * feedback);
                delayGenerations_[writeIndex_] = delayGeneration_;

                const float mix = std::clamp(delayCurrent_, 0.0f, 1.0f);
                const float dryGain = 1.0f - mix;
                const float wetGain = mix;
                inL = inL * dryGain + delayedL * wetGain;
                inR = inR * dryGain + delayedR * wetGain;

                writeIndex_ = (writeIndex_ + 1u) % delayLeft_.size();
            }
        }

        if (filterCurrent_ > 0.000001f) {
            const double cutoff = musicalFilterCutoff(
                static_cast<double>(filterCurrent_), sampleRate_, filterMode_);

            const double a = std::exp(-2.0 * kPi * cutoff / sampleRate_);
            const float oneMinusA = static_cast<float>(1.0 - a);
            const float af = static_cast<float>(a);

            filterStateL_ = killDenormal(
                oneMinusA * inL + af * filterStateL_);
            filterStateR_ = killDenormal(
                oneMinusA * inR + af * filterStateR_);

            if (filterMode_ == 0) {
                inL = filterStateL_;
                inR = filterStateR_;
            } else {
                inL -= filterStateL_;
                inR -= filterStateR_;
            }
        } else {
            filterStateL_ = inL;
            filterStateR_ = inR;
        }

        inL = killDenormal(inL);
        inR = killDenormal(inR);
        left[i] = inL;
        right[i] = inR;
        producedAudio = producedAudio || inL != 0.0f || inR != 0.0f;
    }

    return producedAudio;
}

} // namespace phraseator
