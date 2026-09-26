#include "fragment_player.h"
#include "test_common.h"
#include "source_pool.h"

#include <array>
#include <cmath>

using namespace phraseator;

int main() {
    SourcePool pool;
    CHECK(pool.setOneShot(0, 1u, 4u, 48000.0, false));

    const float mono[4] {1.0f, 0.5f, 0.0f, -0.5f};

    std::array<AudioBufferView, kMaxSources> buffers {};
    buffers[0] = {mono, nullptr, 4u, false};

    FragmentPlayer player;
    player.prepare(48000.0);
    FragmentRef ref {0u, 0u};

    CHECK(player.trigger(pool, buffers, ref, 1.0f, 0.0f, 0.0f));
    CHECK(player.activeVoiceCount() == 1u);

    const auto a = player.processSample(pool, buffers);
    const float centerGain = std::sqrt(0.5f);
    CHECK(std::fabs(a.left - centerGain) < 1.0e-5f);
    CHECK(std::fabs(a.right - centerGain) < 1.0e-5f);

    const auto b = player.processSample(pool, buffers);
    CHECK(std::fabs(b.left - 0.5f * centerGain) < 1.0e-5f);

    player.reset();
    CHECK(player.activeVoiceCount() == 0u);

    // One octave up => 2x read increment.
    CHECK(player.trigger(pool, buffers, ref, 1.0f, -1.0f, 12.0f));
    const auto p0 = player.processSample(pool, buffers);
    const auto p1 = player.processSample(pool, buffers);
    CHECK(std::fabs(p0.left - 1.0f) < 1.0e-5f);
    CHECK(std::fabs(p1.left - 0.0f) < 1.0e-5f);

    {
        // A 24 kHz source in a 48 kHz host must advance by 0.5 source
        // frames per output sample, preserving pitch and duration.
        SourcePool resampledPool;
        CHECK(resampledPool.setOneShot(0, 2u, 4u, 24000.0, false));

        std::array<AudioBufferView, kMaxSources> resampledBuffers {};
        resampledBuffers[0] = {mono, nullptr, 4u, false};

        FragmentPlayer resampledPlayer;
        resampledPlayer.prepare(48000.0);

        CHECK(resampledPlayer.trigger(
            resampledPool, resampledBuffers, ref, 1.0f, -1.0f, 0.0f));

        const auto s0 = resampledPlayer.processSample(
            resampledPool, resampledBuffers);
        const auto s1 = resampledPlayer.processSample(
            resampledPool, resampledBuffers);

        CHECK(std::fabs(s0.left - 1.0f) < 1.0e-5f);
        CHECK(std::fabs(s1.left - 0.75f) < 1.0e-5f);
    }

    return 0;
}
