#include "fragment_player.h"
#include "source_pool.h"

#include <array>
#include <cassert>
#include <cmath>

using namespace phraseator;

int main() {
    SourcePool pool;
    assert(pool.setOneShot(0, 1u, 4u, 48000.0, false));

    const float mono[4] {1.0f, 0.5f, 0.0f, -0.5f};

    std::array<AudioBufferView, kMaxSources> buffers {};
    buffers[0] = {mono, nullptr, 4u, false};

    FragmentPlayer player;
    FragmentRef ref {0u, 0u};

    assert(player.trigger(pool, buffers, ref, 1.0f, 0.0f, 0.0f));
    assert(player.activeVoiceCount() == 1u);

    const auto a = player.processSample(pool, buffers);
    const float centerGain = std::sqrt(0.5f);
    assert(std::fabs(a.left - centerGain) < 1.0e-5f);
    assert(std::fabs(a.right - centerGain) < 1.0e-5f);

    const auto b = player.processSample(pool, buffers);
    assert(std::fabs(b.left - 0.5f * centerGain) < 1.0e-5f);

    player.reset();
    assert(player.activeVoiceCount() == 0u);

    // One octave up => 2x read increment.
    assert(player.trigger(pool, buffers, ref, 1.0f, -1.0f, 12.0f));
    const auto p0 = player.processSample(pool, buffers);
    const auto p1 = player.processSample(pool, buffers);
    assert(std::fabs(p0.left - 1.0f) < 1.0e-5f);
    assert(std::fabs(p1.left - 0.0f) < 1.0e-5f);

    return 0;
}
