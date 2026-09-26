#include "phrase_engine.h"

#include <algorithm>
#include <cmath>

namespace phraseator {

PhraseEngine::PhraseEngine(std::uint32_t seed)
: seed_(seed), rng_(seed) {}

void PhraseEngine::setSeed(std::uint32_t seed) {
    seed_ = seed;
    rng_.seed(seed_);
}

float PhraseEngine::clamp01(float value) noexcept {
    return std::clamp(value, 0.0f, 1.0f);
}

float PhraseEngine::randomUnit() {
    return std::generate_canonical<float, 24>(rng_);
}

bool PhraseEngine::shouldActivate(std::size_t stepIndex, float density) {
    density = clamp01(density);

    // Strong 16th-note positions receive a musically useful bias.
    static constexpr std::array<float, kStepCount> weight {
        1.00f, 0.55f, 0.70f, 0.50f,
        0.90f, 0.55f, 0.72f, 0.50f,
        0.95f, 0.55f, 0.70f, 0.50f,
        0.88f, 0.55f, 0.74f, 0.52f
    };

    const float probability = clamp01(density * 1.35f * weight[stepIndex]);
    return randomUnit() < probability;
}

std::uint16_t PhraseEngine::chooseFragment(std::uint16_t fragmentCount) {
    fragmentCount = std::clamp<std::uint16_t>(fragmentCount, 1, static_cast<std::uint16_t>(kMaxFragments));
    std::uniform_int_distribution<std::uint16_t> distribution(0, static_cast<std::uint16_t>(fragmentCount - 1));
    return distribution(rng_);
}

Step PhraseEngine::makeStep(std::size_t stepIndex, const GenerationSettings& raw) {
    GenerationSettings settings = raw;
    settings.density = clamp01(settings.density);
    settings.repeat = clamp01(settings.repeat);
    settings.pitch = clamp01(settings.pitch);
    settings.pan = clamp01(settings.pan);
    settings.groove = clamp01(settings.groove);

    Step step;
    step.active = shouldActivate(stepIndex, settings.density);
    if (!step.active)
        return step;

    step.fragment = chooseFragment(settings.fragmentCount);
    step.velocity = 0.72f + randomUnit() * 0.28f;

    const float signedRandom = randomUnit() * 2.0f - 1.0f;
    step.pan = signedRandom * settings.pan;

    // Initial implementation deliberately stays conservative.
    const float pitchRange = 12.0f * settings.pitch;
    step.pitchSemitones = std::round((randomUnit() * 2.0f - 1.0f) * pitchRange);

    if (settings.repeat > 0.0f && randomUnit() < settings.repeat * 0.35f) {
        step.repeats = randomUnit() < 0.75f ? 2 : 3;
    }

    // Groove currently shifts weak 16ths only. Later versions can use swing templates.
    if ((stepIndex & 1u) != 0u) {
        step.timingOffset = settings.groove * 0.20f;
    }

    return step;
}

Pattern PhraseEngine::generate(const GenerationSettings& settings) {
    Pattern pattern {};
    for (std::size_t i = 0; i < pattern.size(); ++i)
        pattern[i] = makeStep(i, settings);

    // Keep the first beat anchored at useful densities.
    if (settings.density >= 0.15f && !pattern[0].active) {
        pattern[0] = makeStep(0, settings);
        pattern[0].active = true;
    }

    return pattern;
}

Pattern PhraseEngine::vary(const Pattern& input, const GenerationSettings& raw) {
    Pattern output = input;
    const float amount = clamp01(raw.variation);

    for (std::size_t i = 0; i < output.size(); ++i) {
        if (randomUnit() < amount)
            output[i] = makeStep(i, raw);
    }

    return output;
}

} // namespace phraseator
