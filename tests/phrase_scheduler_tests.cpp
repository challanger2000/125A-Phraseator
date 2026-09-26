#include "phrase_scheduler.h"

#include <array>
#include <cassert>
#include <cmath>

using namespace phraseator;

int main() {
    SourcePool pool;
    assert(pool.setOneShot(0, 1u, 2u, 48000.0, false));

    const float sample[2] {1.0f, 0.0f};
    std::array<AudioBufferView, kMaxSources> buffers {};
    buffers[0] = {sample, nullptr, 2u, false};

    Pattern pattern {};
    pattern[0].active = true;
    pattern[0].fragment = 0;
    pattern[0].velocity = 1.0f;
    pattern[0].pan = 0.0f;
    pattern[0].pitchSemitones = 0.0f;

    PhraseScheduler scheduler;
    scheduler.setPattern(pattern);
    scheduler.prepare(48000.0, 120.0);

    float left[8] {};
    float right[8] {};
    scheduler.processBlock(pool, buffers, 0.0, true, left, right, 8u);

    const float centerGain = std::sqrt(0.5f);
    assert(std::fabs(left[0] - centerGain) < 1.0e-5f);
    assert(std::fabs(right[0] - centerGain) < 1.0e-5f);

    scheduler.processBlock(pool, buffers, 1000.0, false, left, right, 8u);
    for (float x : left)
        assert(x == 0.0f);

    return 0;
}
