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
