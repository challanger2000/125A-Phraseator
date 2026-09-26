#include "phrase_scheduler.h"
#include "test_common.h"

#include <array>
#include <cmath>
#include <vector>

using namespace phraseator;

int main() {
    SourcePool pool;
    CHECK(pool.setOneShot(0, 1u, 2u, 48000.0, false));

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
    pattern[1].timingOffset = 0.20f;

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

    CHECK(std::fabs(left[0] - centerGain) < 1.0e-5f);
    CHECK(std::fabs(right[0] - centerGain) < 1.0e-5f);
    CHECK(std::fabs(left[3000] - centerGain) < 1.0e-5f);
    CHECK(std::fabs(right[3000] - centerGain) < 1.0e-5f);

    // Step 1 is delayed by 20% of one 16th = 1200 samples.
    std::vector<float> grooveLeft(1201u);
    std::vector<float> grooveRight(1201u);
    scheduler.processBlock(pool, buffers, 6000.0, true,
                           grooveLeft.data(), grooveRight.data(), grooveLeft.size());

    CHECK(std::fabs(grooveLeft[0]) < 1.0e-8f);
    CHECK(std::fabs(grooveLeft[1199]) < 1.0e-8f);
    CHECK(std::fabs(grooveLeft[1200] - centerGain) < 1.0e-5f);

    scheduler.processBlock(pool, buffers, 8000.0, false,
                           grooveLeft.data(), grooveRight.data(), 8u);
    for (std::size_t i = 0; i < 8u; ++i)
        CHECK(grooveLeft[i] == 0.0f);

    return 0;
}
