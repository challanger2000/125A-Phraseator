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

float PhraseEngine::choosePitchSemitones(float amount) {
    amount = clamp01(amount);
    if (amount <= 0.0f)
        return 0.0f;

    // EMPIRICALLY TUNED 125A pitch mapping:
    // low/mid settings prioritize intervals that tend to remain musically
    // useful without requiring a known key. The upper creative range opens
    // into chromatic movement deliberately.
    static constexpr std::array<int, 5> gentle {0, 2, -2, 3, -3};
    static constexpr std::array<int, 9> musical {0, 2, -2, 3, -3, 5, -5, 7, -7};
    static constexpr std::array<int, 11> wide {0, 2, -2, 3, -3, 5, -5, 7, -7, 12, -12};

    if (amount < 0.35f) {
        std::uniform_int_distribution<std::size_t> d(0u, gentle.size() - 1u);
        return static_cast<float>(gentle[d(rng_)]);
    }

    if (amount < 0.60f) {
        std::uniform_int_distribution<std::size_t> d(0u, musical.size() - 1u);
        return static_cast<float>(musical[d(rng_)]);
    }

    if (amount < 0.80f) {
        std::uniform_int_distribution<std::size_t> d(0u, wide.size() - 1u);
        return static_cast<float>(wide[d(rng_)]);
    }

    // 80-100% is intentionally the creative range. Blend wide musical
    // intervals with a growing chance of chromatic semitone movement.
    const float chromaticChance = (amount - 0.80f) / 0.20f;
    if (randomUnit() < chromaticChance) {
        std::uniform_int_distribution<int> d(-12, 12);
        return static_cast<float>(d(rng_));
    }

    std::uniform_int_distribution<std::size_t> d(0u, wide.size() - 1u);
    return static_cast<float>(wide[d(rng_)]);
}

std::uint16_t PhraseEngine::chooseFragment(const GenerationSettings& settings) {
    const auto totalFragments = std::clamp<std::uint16_t>(
        settings.fragmentCount, 1, static_cast<std::uint16_t>(kMaxFragments));

    const auto spanCount = std::min<std::size_t>(
        settings.sourceSpanCount, settings.sourceSpans.size());

    if (spanCount > 0u) {
        std::uniform_int_distribution<std::size_t> sourceDistribution(
            0u, spanCount - 1u);

        // Try each configured source at most once. Invalid spans should not
        // poison generation; they simply fall back to flat fragment choice.
        const auto firstSource = sourceDistribution(rng_);
        for (std::size_t attempt = 0; attempt < spanCount; ++attempt) {
            const auto spanIndex = (firstSource + attempt) % spanCount;
            const auto& span = settings.sourceSpans[spanIndex];

            if (span.fragmentCount == 0u ||
                span.firstFragment >= totalFragments) {
                continue;
            }

            const auto available = static_cast<std::uint16_t>(
                std::min<std::size_t>(
                    span.fragmentCount,
                    static_cast<std::size_t>(totalFragments - span.firstFragment)));

            if (available == 0u)
                continue;

            std::uniform_int_distribution<std::uint16_t> sliceDistribution(
                0u, static_cast<std::uint16_t>(available - 1u));
            return static_cast<std::uint16_t>(
                span.firstFragment + sliceDistribution(rng_));
        }
    }

    std::uniform_int_distribution<std::uint16_t> distribution(
        0u, static_cast<std::uint16_t>(totalFragments - 1u));
    return distribution(rng_);
}

Step PhraseEngine::makeStep(std::size_t stepIndex, const GenerationSettings& raw) {
    GenerationSettings settings = raw;
    settings.density = clamp01(settings.density);
    settings.repeat = clamp01(settings.repeat);
    settings.pitch = clamp01(settings.pitch);
    settings.pan = clamp01(settings.pan);
    settings.groove = clamp01(settings.groove);
    settings.velocity = clamp01(settings.velocity);
    settings.octaveMode = std::clamp(settings.octaveMode, 0, 3);

    Step step;
    step.active = shouldActivate(stepIndex, settings.density);
    if (!step.active)
        return step;

    // VELOCITY is a depth control: 0% keeps every generated hit at unity,
    // higher values progressively introduce musical level variation.
    step.velocity = 1.0f - randomUnit() * (0.28f * settings.velocity);

    const float signedRandom = randomUnit() * 2.0f - 1.0f;
    step.pan = signedRandom * settings.pan;

    step.pitchSemitones = choosePitchSemitones(settings.pitch);

    // OCTAVE is deliberately discrete and predictable. +1/-1 transpose the
    // generated phrase by one octave; +/-1 chooses the octave direction per hit.
    if (settings.octaveMode == 1) {
        step.pitchSemitones += 12.0f;
    } else if (settings.octaveMode == 2) {
        step.pitchSemitones -= 12.0f;
    } else if (settings.octaveMode == 3) {
        step.pitchSemitones += randomUnit() < 0.5f ? -12.0f : 12.0f;
    }

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

    // EMPIRICALLY TUNED musical-coherence rule:
    // each quarter-note group gets a motif anchor. Offbeats usually reuse
    // either the recent fragment or the local anchor instead of selecting
    // every fragment independently.
    std::array<std::uint16_t, 4> quarterAnchor {};
    for (auto& anchor : quarterAnchor)
        anchor = chooseFragment(raw);

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
                step.fragment = chooseFragment(raw);
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

    return chooseFragment(raw);
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
    bool changed = false;

    const auto differs = [](const Step& a, const Step& b) noexcept {
        return a.active != b.active ||
               a.fragment != b.fragment ||
               std::fabs(a.velocity - b.velocity) > 1.0e-6f ||
               std::fabs(a.pitchSemitones - b.pitchSemitones) > 1.0e-6f ||
               std::fabs(a.pan - b.pan) > 1.0e-6f ||
               std::fabs(a.gate - b.gate) > 1.0e-6f ||
               a.repeats != b.repeats ||
               std::fabs(a.timingOffset - b.timingOffset) > 1.0e-6f;
    };

    for (std::size_t i = 0; i < output.size(); ++i) {
        if (randomUnit() >= amount)
            continue;

        if (input[i].active) {
            auto candidate = makeStep(i, raw);

            // VARIATE preserves phrase identity in the normal range: keep the
            // rhythmic gate active and mutate musical/detail parameters.
            candidate.active = true;
            candidate.fragment = chooseVariedFragment(input, output, i, raw);
            output[i] = candidate;

            // Only the top creative quarter may remove existing hits.
            if (amount > 0.75f) {
                const float structuralChance = (amount - 0.75f) * 1.6f;
                if (randomUnit() < structuralChance)
                    output[i].active = false;
            }

            changed = changed || differs(output[i], input[i]);
        } else if (amount > 0.75f) {
            // Likewise, only strong variation may introduce new hits.
            const float structuralChance = (amount - 0.75f) * 1.6f;
            if (randomUnit() < structuralChance) {
                auto candidate = makeStep(i, raw);
                candidate.active = true;
                candidate.fragment = chooseVariedFragment(input, output, i, raw);
                output[i] = candidate;
                changed = true;
            }
        }
    }

    // A VARIATE click must always do something audible on a non-empty phrase.
    // Prefer a fragment/source change; if only one fragment exists, make a
    // deterministic velocity change while preserving the rhythm structure.
    if (!changed && amount > 0.0f) {
        for (std::size_t i = 0; i < output.size(); ++i) {
            if (!input[i].active)
                continue;

            if (raw.fragmentCount > 1u) {
                output[i].fragment = static_cast<std::uint16_t>(
                    (input[i].fragment + 1u) %
                    std::max<std::uint16_t>(raw.fragmentCount, 1u));
            }

            if (output[i].fragment == input[i].fragment) {
                const float delta = input[i].velocity > 0.86f ? -0.10f : 0.10f;
                output[i].velocity =
                    std::clamp(input[i].velocity + delta, 0.50f, 1.0f);
            }

            changed = differs(output[i], input[i]);
            if (changed)
                break;
        }
    }

    return output;
}

} // namespace phraseator
