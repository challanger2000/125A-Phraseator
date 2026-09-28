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
        int structuralChanges = 0;

        PhraseEngine variationEngine(0xB4531u);
        for (int n = 0; n < 256; ++n) {
            const auto varied = variationEngine.vary(base, moderate);

            for (std::size_t i = 0; i < kStepCount; ++i) {
                if (varied[i].active != base[i].active)
                    ++structuralChanges;

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
        CHECK(structuralChanges == 0);
    }

    {
        // Every non-zero VARIATE action on a non-empty phrase must produce at
        // least one audible/detail change, even at the default 25% setting.
        GenerationSettings settings;
        settings.density = 0.65f;
        settings.variation = 0.25f;
        settings.fragmentCount = 2u;

        PhraseEngine sourceEngine(0xA11D1u);
        const auto base = sourceEngine.generate(settings);

        PhraseEngine variator(0xA11D2u);
        for (int n = 0; n < 256; ++n) {
            const auto varied = variator.vary(base, settings);
            bool anyDifference = false;
            for (std::size_t i = 0; i < kStepCount; ++i) {
                const auto& a = base[i];
                const auto& b = varied[i];
                anyDifference = anyDifference ||
                    a.active != b.active ||
                    a.fragment != b.fragment ||
                    std::fabs(a.velocity - b.velocity) > 1.0e-6f ||
                    std::fabs(a.pitchSemitones - b.pitchSemitones) > 1.0e-6f ||
                    std::fabs(a.pan - b.pan) > 1.0e-6f ||
                    a.repeats != b.repeats ||
                    std::fabs(a.timingOffset - b.timingOffset) > 1.0e-6f;
            }
            CHECK(anyDifference);
        }
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
        // Source-balanced selection: one one-shot and one 16-slice loop should
        // receive comparable source-level usage despite very different slice
        // counts. Flat fragment weighting would heavily favor the loop.
        GenerationSettings balanced;
        balanced.density = 0.85f;
        balanced.fragmentCount = 17u;
        balanced.sourceSpanCount = 2u;
        balanced.sourceSpans[0] = {0u, 1u};
        balanced.sourceSpans[1] = {1u, 16u};

        PhraseEngine balancedEngine(0x50A2CEu);
        int oneShotUses = 0;
        int loopUses = 0;
        std::array<bool, 16> loopSlicesSeen {};

        for (int n = 0; n < 512; ++n) {
            const auto pattern = balancedEngine.generate(balanced);
            for (const auto& step : pattern) {
                if (!step.active)
                    continue;

                if (step.fragment == 0u) {
                    ++oneShotUses;
                } else if (step.fragment <= 16u) {
                    ++loopUses;
                    loopSlicesSeen[step.fragment - 1u] = true;
                }
            }
        }

        CHECK(oneShotUses > 0);
        CHECK(loopUses > 0);

        const double oneShotRatio =
            static_cast<double>(oneShotUses) /
            static_cast<double>(oneShotUses + loopUses);

        CHECK(oneShotRatio > 0.35);
        CHECK(oneShotRatio < 0.65);

        int distinctLoopSlices = 0;
        for (const bool seen : loopSlicesSeen)
            distinctLoopSlices += seen ? 1 : 0;
        CHECK(distinctLoopSlices >= 12);
    }

    {
        // PITCH follows the 125A macro philosophy: the normal musical range
        // must avoid arbitrary chromatic semitone scatter, while the top
        // creative range is explicitly allowed to reach it.
        GenerationSettings musicalPitch;
        musicalPitch.density = 1.0f;
        musicalPitch.fragmentCount = 4u;
        musicalPitch.pitch = 0.50f;

        PhraseEngine pitchEngine(0x917C4u);
        bool sawNonZero = false;

        for (int n = 0; n < 256; ++n) {
            const auto pattern = pitchEngine.generate(musicalPitch);
            for (const auto& step : pattern) {
                if (!step.active)
                    continue;

                const int p = static_cast<int>(step.pitchSemitones);
                sawNonZero = sawNonZero || p != 0;

                const int absP = std::abs(p);
                CHECK(absP == 0 || absP == 2 || absP == 3 ||
                      absP == 5 || absP == 7);
            }
        }

        CHECK(sawNonZero);

        GenerationSettings creativePitch = musicalPitch;
        creativePitch.pitch = 1.0f;
        PhraseEngine creativeEngine(0xC2EA7u);
        bool sawChromatic = false;

        for (int n = 0; n < 256; ++n) {
            const auto pattern = creativeEngine.generate(creativePitch);
            for (const auto& step : pattern) {
                if (!step.active)
                    continue;

                const int absP = std::abs(static_cast<int>(step.pitchSemitones));
                if (absP == 1 || absP == 4 || absP == 6 ||
                    absP == 8 || absP == 9 || absP == 10 || absP == 11) {
                    sawChromatic = true;
                }
            }
        }

        CHECK(sawChromatic);
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

        // With 12 equiprobable fragments, zero reuse should stay near
        // the natural 1/12 coincidence rate. Full reuse must be materially
        // above that baseline.
        CHECK(lowRatio < 0.16);
        CHECK(highRatio > 0.35);
        CHECK(highRatio > lowRatio * 2.5);
    }

    {
        // Measured macro effect: a macro change followed by GENERATE must
        // produce a materially different deterministic pattern population.
        GenerationSettings low;
        low.density = 0.20f;
        low.fragmentCount = 2u;
        low.pitch = 0.0f;
        low.pan = 0.0f;
        low.groove = 0.0f;

        GenerationSettings high = low;
        high.density = 0.80f;
        high.pitch = 0.75f;
        high.pan = 0.75f;
        high.groove = 0.75f;

        PhraseEngine lowEngine(0x4D414352u);
        PhraseEngine highEngine(0x4D414352u);

        int lowActive = 0;
        int highActive = 0;
        int highNonZeroPitch = 0;
        int highNonZeroPan = 0;
        int highNonZeroTiming = 0;

        for (int n = 0; n < 256; ++n) {
            const auto a = lowEngine.generate(low);
            const auto b = highEngine.generate(high);
            for (std::size_t i = 0; i < kStepCount; ++i) {
                lowActive += a[i].active ? 1 : 0;
                highActive += b[i].active ? 1 : 0;
                if (!b[i].active)
                    continue;
                highNonZeroPitch += std::fabs(b[i].pitchSemitones) > 1.0e-6f ? 1 : 0;
                highNonZeroPan += std::fabs(b[i].pan) > 1.0e-6f ? 1 : 0;
                highNonZeroTiming += std::fabs(b[i].timingOffset) > 1.0e-6f ? 1 : 0;
            }
        }

        CHECK(highActive > lowActive * 2);
        CHECK(highNonZeroPitch > 0);
        CHECK(highNonZeroPan > 0);
        CHECK(highNonZeroTiming > 0);
    }


    {
        // VELOCITY is a depth macro: zero must be uniform, while 100% must
        // introduce measurable hit-level variation without exceeding unity.
        GenerationSettings flat;
        flat.density = 1.0f;
        flat.fragmentCount = 4u;
        flat.velocity = 0.0f;

        PhraseEngine flatEngine(0x7E11u);
        const auto flatPattern = flatEngine.generate(flat);
        for (const auto& step : flatPattern) {
            if (step.active)
                CHECK(std::fabs(step.velocity - 1.0f) < 1.0e-6f);
        }

        GenerationSettings dynamic = flat;
        dynamic.velocity = 1.0f;
        PhraseEngine dynamicEngine(0x7E10u);
        bool sawBelowUnity = false;
        for (int n = 0; n < 64; ++n) {
            const auto pattern = dynamicEngine.generate(dynamic);
            for (const auto& step : pattern) {
                if (!step.active)
                    continue;
                CHECK(step.velocity <= 1.0f);
                CHECK(step.velocity >= 0.72f);
                sawBelowUnity = sawBelowUnity || step.velocity < 0.99f;
            }
        }
        CHECK(sawBelowUnity);
    }

    {
        // OCTAVE selector is discrete: +1/-1 are deterministic register
        // shifts, +/-1 chooses only those two octave offsets per active hit.
        GenerationSettings up;
        up.density = 1.0f;
        up.fragmentCount = 2u;
        up.pitch = 0.0f;
        up.octaveMode = 1;
        PhraseEngine upEngine(0x0C71u);
        const auto upPattern = upEngine.generate(up);
        for (const auto& step : upPattern)
            if (step.active) CHECK(step.pitchSemitones == 12.0f);

        GenerationSettings down = up;
        down.octaveMode = 2;
        PhraseEngine downEngine(0x0C72u);
        const auto downPattern = downEngine.generate(down);
        for (const auto& step : downPattern)
            if (step.active) CHECK(step.pitchSemitones == -12.0f);

        GenerationSettings both = up;
        both.octaveMode = 3;
        PhraseEngine bothEngine(0x0C73u);
        bool sawUp = false;
        bool sawDown = false;
        for (int n = 0; n < 64; ++n) {
            const auto pattern = bothEngine.generate(both);
            for (const auto& step : pattern) {
                if (!step.active) continue;
                CHECK(step.pitchSemitones == 12.0f || step.pitchSemitones == -12.0f);
                sawUp = sawUp || step.pitchSemitones > 0.0f;
                sawDown = sawDown || step.pitchSemitones < 0.0f;
            }
        }
        CHECK(sawUp && sawDown);
    }


    {
        // VARIATE DEPTH must mutate more than the first step and must keep
        // forced fragment changes inside the configured audible source spans.
        Pattern base {};
        for (std::size_t i = 0; i < base.size(); ++i) {
            base[i].active = true;
            base[i].fragment = 0u;
            base[i].repeats = 1u;
        }

        GenerationSettings settings;
        settings.variation = 0.25f;
        settings.fragmentCount = 4u;
        settings.sourceSpanCount = 2u;
        settings.sourceSpans[0] = {0u, 1u};
        settings.sourceSpans[1] = {3u, 1u}; // fragment 1-2 emulate muted sources

        PhraseEngine variator(0x125A991u);
        const auto varied = variator.vary(base, settings);

        int changed = 0;
        bool changedBeyondFirst = false;
        for (std::size_t i = 0; i < varied.size(); ++i) {
            const bool differs =
                varied[i].fragment != base[i].fragment ||
                varied[i].repeats != base[i].repeats;
            if (!differs)
                continue;
            ++changed;
            changedBeyondFirst = changedBeyondFirst || i > 0u;
            CHECK(varied[i].fragment == 0u || varied[i].fragment == 3u);
        }

        CHECK(changed == 4); // ceil(0.25 * 16)
        CHECK(changedBeyondFirst);
    }


    {
        // Runtime source spans are authoritative. With every source muted,
        // GENERATE must produce silence and VARIATE must leave an existing
        // pattern untouched rather than falling back to flat fragment choice.
        GenerationSettings muted;
        muted.density = 1.0f;
        muted.variation = 1.0f;
        muted.fragmentCount = 4u;
        muted.sourceSpansAuthoritative = true;
        muted.sourceSpanCount = 0u;

        PhraseEngine engine(0xA1100u);
        const auto generated = engine.generate(muted);
        for (const auto& step : generated)
            CHECK(!step.active);

        Pattern base {};
        base[0].active = true;
        base[0].fragment = 2u;
        base[4].active = true;
        base[4].fragment = 3u;

        const auto varied = engine.vary(base, muted);
        for (std::size_t i = 0; i < base.size(); ++i) {
            CHECK(varied[i].active == base[i].active);
            CHECK(varied[i].fragment == base[i].fragment);
            CHECK(varied[i].repeats == base[i].repeats);
        }
    }

    {
        // Continuity/preservation paths in VARIATE must not select fragments
        // outside authoritative spans. Fragment 0 is excluded; only 3 is live.
        Pattern base {};
        for (std::size_t i = 0; i < base.size(); ++i) {
            base[i].active = true;
            base[i].fragment = (i & 1u) ? 0u : 3u;
        }

        GenerationSettings settings;
        settings.variation = 1.0f;
        settings.fragmentCount = 4u;
        settings.sourceSpansAuthoritative = true;
        settings.sourceSpanCount = 1u;
        settings.sourceSpans[0] = {3u, 1u};

        PhraseEngine engine(0xA11C0DEu);
        const auto varied = engine.vary(base, settings);
        for (const auto& step : varied) {
            if (!step.active)
                continue;
            CHECK(step.fragment == 3u);
        }
    }


    {
        // VARIATE must never alter manual ratchet settings.
        Pattern base {};
        for (std::size_t i = 0; i < base.size(); ++i) {
            base[i].active = true;
            base[i].fragment = static_cast<std::uint16_t>(i & 1u);
            base[i].repeats = static_cast<std::uint8_t>((i % 4u) + 1u);
        }

        GenerationSettings settings;
        settings.variation = 1.0f;
        settings.fragmentCount = 2u;

        PhraseEngine engine(0xA11CE55u);
        const auto varied = engine.vary(base, settings);
        for (std::size_t i = 0; i < base.size(); ++i)
            CHECK(varied[i].repeats == base[i].repeats);
    }

    return 0;
}
