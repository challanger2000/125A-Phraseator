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

    return 0;
}
