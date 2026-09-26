#include "phrase_scheduler.h"

#include <array>
#include <cassert>
#include <cmath>
#include <vector>

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
    pattern[0].repeats = 2;

    pattern[1] = pattern[0];
    pattern[1].repeats = 1;

    PhraseScheduler scheduler;
    scheduler.setPattern(pattern);
    scheduler.prepare(48000.0, 120.0);

    const float centerGain = std::sqrt(0.5f);

    // 16th at 120 BPM / 48 kHz = 6000 samples.
    // repeats=2 must retrigger halfway, at sample 3000.
    std::vector<float> left(3001u);
    std::vector<float> right(3001u);
    scheduler.processBlock(pool, buffers, 0.0, true,
                           left.data(), right.data(), left.size());

    assert(std::fabs(left[0] - centerGain) < 1.0e-5f);
    assert(std::fabs(right[0] - centerGain) < 1.0e-5f);
    assert(std::fabs(left[3000] - centerGain) < 1.0e-5f);
    assert(std::fabs(right[3000] - centerGain) < 1.0e-5f);

    // Regression: a new host block beginning exactly on the next 16th boundary
    // must trigger that step even though playback never stopped.
    float boundaryLeft[8] {};
    float boundaryRight[8] {};
    scheduler.processBlock(pool, buffers, 6000.0, true,
                           boundaryLeft, boundaryRight, 8u);
    assert(std::fabs(boundaryLeft[0] - centerGain) < 1.0e-5f);
    assert(std::fabs(boundaryRight[0] - centerGain) < 1.0e-5f);

    scheduler.processBlock(pool, buffers, 7000.0, false,
                           boundaryLeft, boundaryRight, 8u);
    for (float x : boundaryLeft)
        assert(x == 0.0f);

    return 0;
}
