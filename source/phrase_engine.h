#pragma once

#include <array>
#include <cstdint>
#include <random>

namespace phraseator {

constexpr std::size_t kStepCount = 16;
constexpr std::size_t kMaxFragments = 128;

struct Step {
    bool active {false};
    std::uint16_t fragment {0};
    float velocity {1.0f};
    float pitchSemitones {0.0f};
    float pan {0.0f};
    float gate {1.0f};
    std::uint8_t repeats {1};
    float timingOffset {0.0f};
};

using Pattern = std::array<Step, kStepCount>;

struct GenerationSettings {
    float density {0.50f};
    float variation {0.25f};
    float repeat {0.0f};
    float pitch {0.0f};
    float pan {0.0f};
    float groove {0.0f};
    std::uint16_t fragmentCount {1};
};

class PhraseEngine {
public:
    explicit PhraseEngine(std::uint32_t seed = 0x125A0001u);

    void setSeed(std::uint32_t seed);
    std::uint32_t seed() const noexcept { return seed_; }

    Pattern generate(const GenerationSettings& settings);
    Pattern vary(const Pattern& input, const GenerationSettings& settings);

private:
    static float clamp01(float value) noexcept;
    Step makeStep(std::size_t stepIndex, const GenerationSettings& settings);
    bool shouldActivate(std::size_t stepIndex, float density);
    std::uint16_t chooseFragment(std::uint16_t fragmentCount);
    float randomUnit();

    std::uint32_t seed_;
    std::mt19937 rng_;
};

} // namespace phraseator
