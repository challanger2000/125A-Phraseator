#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace phraseator {

class PhraseFx {
public:
    void prepare(double sampleRate);
    void reset() noexcept;

    void setDelayAmount(float amount) noexcept;
    void setFilterAmount(float amount) noexcept;

    bool processBlock(float* left,
                      float* right,
                      std::size_t numSamples,
                      double tempoBpm) noexcept;

private:
    static float clamp01(float value) noexcept;
    float readDelay(const std::vector<float>& buffer,
                    const std::vector<std::uint64_t>& generations,
                    double readPosition) const noexcept;

    double sampleRate_ {48000.0};

    std::vector<float> delayLeft_;
    std::vector<float> delayRight_;
    std::vector<std::uint64_t> delayGenerations_;
    std::uint64_t delayGeneration_ {1u};
    std::size_t writeIndex_ {0};

    float delayTarget_ {0.0f};
    float delayCurrent_ {0.0f};
    double delaySamplesCurrent_ {12000.0};

    float filterTarget_ {0.0f};
    float filterCurrent_ {0.0f};
    float filterStateL_ {0.0f};
    float filterStateR_ {0.0f};
};

} // namespace phraseator
