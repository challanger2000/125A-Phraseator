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


void PhraseEngine::assignMusicalFragments(
    Pattern& pattern,
    const GenerationSettings& raw) {

    const auto fragmentCount = std::clamp<std::uint16_t>(
        raw.fragmentCount, 1, static_cast<std::uint16_t>(kMaxFragments));

    // EMPIRICALLY TUNED musical-coherence rule:
    // each quarter-note group gets a motif anchor. Offbeats usually reuse
    // either the recent fragment or the local anchor instead of selecting
    // every fragment independently.
    std::array<std::uint16_t, 4> quarterAnchor {};
    for (auto& anchor : quarterAnchor)
        anchor = chooseFragment(fragmentCount);

    bool havePrevious = false;
    std::uint16_t previous = 0;
    const float repeatAmount = clamp01(raw.repeat);
    const float previousProbability = 0.18f + 0.42f * repeatAmount;

    for (std::size_t i = 0; i < pattern.size(); ++i) {
        auto& step = pattern[i];
        if (!step.active)
            continue;

        const auto anchor = quarterAnchor[i / 4u];

        if ((i % 4u) == 0u) {
            step.fragment = anchor;
        } else {
            const float draw = randomUnit();

            if (havePrevious && draw < previousProbability) {
                step.fragment = previous;
            } else if (draw < previousProbability + 0.52f) {
                step.fragment = anchor;
            } else {
                step.fragment = chooseFragment(fragmentCount);
            }
        }

        previous = step.fragment;
        havePrevious = true;
    }
}

std::uint16_t PhraseEngine::chooseVariedFragment(
    const Pattern& input,
    const Pattern& output,
    std::size_t stepIndex,
    const GenerationSettings& raw) {

    const auto fragmentCount = std::clamp<std::uint16_t>(
        raw.fragmentCount, 1, static_cast<std::uint16_t>(kMaxFragments));

    // Preserve phrase identity when mutating an already active step.
    if (input[stepIndex].active && randomUnit() < 0.55f)
        return static_cast<std::uint16_t>(input[stepIndex].fragment % fragmentCount);

    // Prefer local continuity over a completely unrelated fragment.
    for (std::size_t back = stepIndex; back > 0; --back) {
        const auto& previous = output[back - 1u];
        if (previous.active) {
            const float continuity = 0.25f + 0.35f * clamp01(raw.repeat);
            if (randomUnit() < continuity)
                return static_cast<std::uint16_t>(previous.fragment % fragmentCount);
            break;
        }
    }

    return chooseFragment(fragmentCount);
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

    assignMusicalFragments(pattern, settings);
    return pattern;
}

Pattern PhraseEngine::vary(const Pattern& input, const GenerationSettings& raw) {
    Pattern output = input;
    const float amount = clamp01(raw.variation);

    for (std::size_t i = 0; i < output.size(); ++i) {
        if (randomUnit() < amount) {
            output[i] = makeStep(i, raw);
            if (output[i].active)
                output[i].fragment = chooseVariedFragment(input, output, i, raw);
        }
    }

    return output;
}

} // namespace phraseator
