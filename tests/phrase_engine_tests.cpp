#include "phrase_engine.h"
#include "test_common.h"

#include <cmath>

using namespace phraseator;

int main() {
    GenerationSettings settings;
    settings.density = 0.65f;
    settings.fragmentCount = 10;
    settings.pan = 0.4f;

    PhraseEngine a(12345u);
    PhraseEngine b(12345u);

    const auto pa = a.generate(settings);
    const auto pb = b.generate(settings);

    for (std::size_t i = 0; i < kStepCount; ++i) {
        CHECK(pa[i].active == pb[i].active);
        CHECK(pa[i].fragment == pb[i].fragment);
        CHECK(std::fabs(pa[i].pan - pb[i].pan) < 1.0e-6f);
        if (pa[i].active) {
            CHECK(pa[i].fragment < settings.fragmentCount);
            CHECK(pa[i].pan >= -settings.pan);
            CHECK(pa[i].pan <= settings.pan);
        }
    }

    GenerationSettings unchanged = settings;
    unchanged.variation = 0.0f;

    PhraseEngine c(99u);
    const auto pv = c.vary(pa, unchanged);
    for (std::size_t i = 0; i < kStepCount; ++i) {
        CHECK(pv[i].active == pa[i].active);
        CHECK(pv[i].fragment == pa[i].fragment);
        CHECK(pv[i].pitchSemitones == pa[i].pitchSemitones);
    }


    {
        // 125A neutral-at-zero contract for the generation macros.
        GenerationSettings neutral;
        neutral.density = 0.0f;
        neutral.fragmentCount = 8;
        neutral.repeat = 0.0f;
        neutral.pitch = 0.0f;
        neutral.pan = 0.0f;
        neutral.groove = 0.0f;

        PhraseEngine neutralEngine(0x0A125u);
        for (int n = 0; n < 128; ++n) {
            const auto pattern = neutralEngine.generate(neutral);
            for (const auto& step : pattern)
                CHECK(!step.active);
        }

        neutral.density = 0.50f;
        const auto activePattern = neutralEngine.generate(neutral);
        for (const auto& step : activePattern) {
            if (!step.active)
                continue;
            CHECK(step.pitchSemitones == 0.0f);
            CHECK(step.pan == 0.0f);
            CHECK(step.timingOffset == 0.0f);
            CHECK(step.repeats == 1u);
        }
    }

    {
        // DENSITY should be monotonic in aggregate: a high-density setting
        // must produce clearly more active steps than a low-density setting
        // across a deterministic fixture set.
        GenerationSettings sparse;
        sparse.density = 0.20f;
        sparse.fragmentCount = 8;

        GenerationSettings dense = sparse;
        dense.density = 0.80f;

        PhraseEngine sparseEngine(0xD3517u);
        PhraseEngine denseEngine(0xD3517u);

        int sparseActive = 0;
        int denseActive = 0;

        for (int n = 0; n < 512; ++n) {
            const auto aPattern = sparseEngine.generate(sparse);
            const auto bPattern = denseEngine.generate(dense);

            for (std::size_t i = 0; i < kStepCount; ++i) {
                sparseActive += aPattern[i].active ? 1 : 0;
                denseActive += bPattern[i].active ? 1 : 0;
            }
        }

        CHECK(denseActive > sparseActive);
        CHECK(denseActive > sparseActive * 2);
    }

    {
        // VARIATION should behave as mutation, not replacement. Zero must
        // preserve the phrase exactly; a moderate amount must change some
        // steps while leaving a substantial part of the phrase intact.
        GenerationSettings baseSettings;
        baseSettings.density = 0.72f;
        baseSettings.fragmentCount = 10;
        baseSettings.repeat = 0.25f;

        PhraseEngine baseEngine(0xB453u);
        const auto base = baseEngine.generate(baseSettings);

        GenerationSettings moderate = baseSettings;
        moderate.variation = 0.35f;

        int changedSteps = 0;
        int preservedSteps = 0;

        PhraseEngine variationEngine(0xB4531u);
        for (int n = 0; n < 256; ++n) {
            const auto varied = variationEngine.vary(base, moderate);

            for (std::size_t i = 0; i < kStepCount; ++i) {
                const bool same =
                    varied[i].active == base[i].active &&
                    varied[i].fragment == base[i].fragment &&
                    std::fabs(varied[i].velocity - base[i].velocity) < 1.0e-6f &&
                    std::fabs(varied[i].pitchSemitones - base[i].pitchSemitones) < 1.0e-6f &&
                    std::fabs(varied[i].pan - base[i].pan) < 1.0e-6f &&
                    varied[i].repeats == base[i].repeats;

                if (same)
                    ++preservedSteps;
                else
                    ++changedSteps;
            }
        }

        CHECK(changedSteps > 0);
        CHECK(preservedSteps > changedSteps);
    }

    {
        // At useful densities the first beat remains anchored instead of
        // allowing an accidental fully floating phrase start.
        GenerationSettings anchored;
        anchored.density = 0.30f;
        anchored.fragmentCount = 6;

        PhraseEngine anchorEngine(0xA11C0u);
        for (int n = 0; n < 256; ++n) {
            const auto pattern = anchorEngine.generate(anchored);
            CHECK(pattern[0].active);
        }
    }

    {
        GenerationSettings lowRepeat;
        lowRepeat.density = 0.85f;
        lowRepeat.fragmentCount = 12;
        lowRepeat.repeat = 0.0f;

        GenerationSettings highRepeat = lowRepeat;
        highRepeat.repeat = 1.0f;

        PhraseEngine lowEngine(0xA11CEu);
        PhraseEngine highEngine(0xA11CEu);

        int lowAdjacentReuse = 0;
        int highAdjacentReuse = 0;
        int lowPairs = 0;
        int highPairs = 0;

        for (int n = 0; n < 256; ++n) {
            const auto low = lowEngine.generate(lowRepeat);
            const auto high = highEngine.generate(highRepeat);

            for (std::size_t i = 1; i < kStepCount; ++i) {
                if (low[i - 1].active && low[i].active) {
                    ++lowPairs;
                    if (low[i - 1].fragment == low[i].fragment)
                        ++lowAdjacentReuse;
                }
                if (high[i - 1].active && high[i].active) {
                    ++highPairs;
                    if (high[i - 1].fragment == high[i].fragment)
                        ++highAdjacentReuse;
                }
            }
        }

        CHECK(lowPairs > 0);
        CHECK(highPairs > 0);

        const double lowRatio =
            static_cast<double>(lowAdjacentReuse) / static_cast<double>(lowPairs);
        const double highRatio =
            static_cast<double>(highAdjacentReuse) / static_cast<double>(highPairs);

        CHECK(highRatio > lowRatio);
    }

    return 0;
}
