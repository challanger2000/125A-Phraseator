#include "phrase_engine.h"

#include <cassert>
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
        assert(pa[i].active == pb[i].active);
        assert(pa[i].fragment == pb[i].fragment);
        assert(std::fabs(pa[i].pan - pb[i].pan) < 1.0e-6f);
        if (pa[i].active) {
            assert(pa[i].fragment < settings.fragmentCount);
            assert(pa[i].pan >= -settings.pan);
            assert(pa[i].pan <= settings.pan);
        }
    }

    GenerationSettings unchanged = settings;
    unchanged.variation = 0.0f;

    PhraseEngine c(99u);
    const auto pv = c.vary(pa, unchanged);
    for (std::size_t i = 0; i < kStepCount; ++i) {
        assert(pv[i].active == pa[i].active);
        assert(pv[i].fragment == pa[i].fragment);
        assert(pv[i].pitchSemitones == pa[i].pitchSemitones);
    }

    return 0;
}
