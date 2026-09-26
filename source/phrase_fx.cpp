#include "phrase_fx.h"

#include <algorithm>
#include <cmath>

namespace phraseator {

namespace {
constexpr double kPi = 3.14159265358979323846;
}

float PhraseFx::clamp01(float value) noexcept {
    if (!std::isfinite(value))
        return 0.0f;
    return std::clamp(value, 0.0f, 1.0f);
}

void PhraseFx::prepare(double sampleRate) {
    sampleRate_ = (std::isfinite(sampleRate) && sampleRate > 1000.0)
        ? sampleRate
        : 48000.0;

    const auto maxDelaySamples = static_cast<std::size_t>(
        std::ceil(sampleRate_ * 2.1));

    delayLeft_.assign(maxDelaySamples + 2u, 0.0f);
    delayRight_.assign(maxDelaySamples + 2u, 0.0f);

    reset();
}

void PhraseFx::reset() noexcept {
    std::fill(delayLeft_.begin(), delayLeft_.end(), 0.0f);
    std::fill(delayRight_.begin(), delayRight_.end(), 0.0f);

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
        std::fill(delayLeft_.begin(), delayLeft_.end(), 0.0f);
        std::fill(delayRight_.begin(), delayRight_.end(), 0.0f);
        writeIndex_ = 0u;
    }

    delayTarget_ = next;
}

void PhraseFx::setFilterAmount(float amount) noexcept {
    filterTarget_ = clamp01(amount);
}

float PhraseFx::readDelay(const std::vector<float>& buffer,
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

    return buffer[i0] + (buffer[i1] - buffer[i0]) * frac;
}

bool PhraseFx::processBlock(float* left,
                            float* right,
                            std::size_t numSamples,
                            double tempoBpm) noexcept {
    if (!left || !right || numSamples == 0u)
        return false;

    const double tempo = (std::isfinite(tempoBpm) && tempoBpm > 1.0)
        ? std::clamp(tempoBpm, 20.0, 400.0)
        : 120.0;

    const double quarterSamples = sampleRate_ * 60.0 / tempo;
    const double delaySamplesTarget = std::clamp(
        quarterSamples * 0.5,
        1.0,
        static_cast<double>(delayLeft_.empty() ? 1u : delayLeft_.size() - 2u));

    const double amountCoeff = 1.0 - std::exp(-1.0 / (sampleRate_ * 0.020));
    const double timeCoeff = 1.0 - std::exp(-1.0 / (sampleRate_ * 0.050));

    bool producedAudio = false;

    for (std::size_t i = 0; i < numSamples; ++i) {
        delayCurrent_ += static_cast<float>(
            (static_cast<double>(delayTarget_) - delayCurrent_) * amountCoeff);

        filterCurrent_ += static_cast<float>(
            (static_cast<double>(filterTarget_) - filterCurrent_) * amountCoeff);

        delaySamplesCurrent_ +=
            (delaySamplesTarget - delaySamplesCurrent_) * timeCoeff;

        float inL = left[i];
        float inR = right[i];

        if (!delayLeft_.empty()) {
            const double readPos =
                static_cast<double>(writeIndex_) - delaySamplesCurrent_;

            const float delayedL = readDelay(delayLeft_, readPos);
            const float delayedR = readDelay(delayRight_, readPos);

            constexpr float feedback = 0.34f;
            delayLeft_[writeIndex_] = inL + delayedR * feedback;
            delayRight_[writeIndex_] = inR + delayedL * feedback;

            const float wet = delayCurrent_ * 0.70f;
            inL += delayedL * wet;
            inR += delayedR * wet;

            writeIndex_ = (writeIndex_ + 1u) % delayLeft_.size();
        }

        if (filterCurrent_ > 0.000001f) {
            const double minCutoff = 800.0;
            const double maxCutoff = std::min(20000.0, sampleRate_ * 0.45);
            const double amount = static_cast<double>(filterCurrent_);
            const double cutoff = maxCutoff *
                std::pow(minCutoff / maxCutoff, amount);

            const double a = std::exp(-2.0 * kPi * cutoff / sampleRate_);
            const float oneMinusA = static_cast<float>(1.0 - a);
            const float af = static_cast<float>(a);

            filterStateL_ = oneMinusA * inL + af * filterStateL_;
            filterStateR_ = oneMinusA * inR + af * filterStateR_;

            inL = filterStateL_;
            inR = filterStateR_;
        } else {
            filterStateL_ = inL;
            filterStateR_ = inR;
        }

        left[i] = inL;
        right[i] = inR;
        producedAudio = producedAudio || inL != 0.0f || inR != 0.0f;
    }

    return producedAudio;
}

} // namespace phraseator
