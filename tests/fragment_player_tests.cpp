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

    {
        SourcePool stereoPool;
        CHECK(stereoPool.setOneShot(0, 3u, 2u, 48000.0, true));

        const float leftStereo[2] {1.0f, 0.0f};
        const float rightStereo[2] {0.5f, 0.0f};

        std::array<AudioBufferView, kMaxSources> stereoBuffers {};
        stereoBuffers[0] = {leftStereo, rightStereo, 2u, true};

        FragmentPlayer stereoPlayer;
        stereoPlayer.prepare(48000.0);

        CHECK(stereoPlayer.trigger(
            stereoPool, stereoBuffers, ref, 1.0f, 0.0f, 0.0f));

        const auto centered = stereoPlayer.processSample(
            stereoPool, stereoBuffers);

        const float centeredMono = 0.75f * std::sqrt(0.5f);
        CHECK(std::fabs(centered.left - centeredMono) < 1.0e-5f);
        CHECK(std::fabs(centered.right - centeredMono) < 1.0e-5f);

        stereoPlayer.reset();
        CHECK(stereoPlayer.trigger(
            stereoPool, stereoBuffers, ref, 1.0f, 1.0f, 0.0f));

        const auto hardRight = stereoPlayer.processSample(
            stereoPool, stereoBuffers);

        CHECK(std::fabs(hardRight.left) < 1.0e-5f);
        CHECK(std::fabs(hardRight.right - 0.75f) < 1.0e-5f);
    }

    {
        // Loop slices get a short de-click envelope; one-shots do not.
        constexpr std::uint32_t loopFrames = 32u;
        SourcePool loopPool;
        const SliceRegion loopRegion {0u, loopFrames};
        CHECK(loopPool.setLoopSlices(
            0, 4u, loopFrames, 48000.0, false, &loopRegion, 1u));

        float constant[loopFrames] {};
        for (auto& x : constant)
            x = 1.0f;

        std::array<AudioBufferView, kMaxSources> loopBuffers {};
        loopBuffers[0] = {constant, nullptr, loopFrames, false};

        FragmentPlayer loopPlayer;
        loopPlayer.prepare(48000.0);
        CHECK(loopPlayer.trigger(
            loopPool, loopBuffers, ref, 1.0f, -1.0f, 0.0f));

        const auto first = loopPlayer.processSample(loopPool, loopBuffers);
        CHECK(std::fabs(first.left) < 1.0e-8f);

        StereoFrame middle {};
        for (int i = 0; i < 16; ++i)
            middle = loopPlayer.processSample(loopPool, loopBuffers);
        CHECK(middle.left > 0.95f);
    }


    {
        // Choke release is time-based: ~2 ms at both 48 and 96 kHz.
        SourcePool chokePool;
        constexpr std::uint32_t frames = 512u;
        CHECK(chokePool.setOneShot(0, 9u, frames, 48000.0, false));

        float constant[frames] {};
        for (auto& x : constant) x = 1.0f;
        std::array<AudioBufferView, kMaxSources> chokeBuffers {};
        chokeBuffers[0] = {constant, nullptr, frames, false};

        FragmentPlayer p48;
        p48.prepare(48000.0);
        CHECK(p48.trigger(chokePool, chokeBuffers, ref, 1.0f, 0.0f, 0.0f));
        p48.chokeAll();
        for (int i = 0; i < 95; ++i)
            p48.processSample(chokePool, chokeBuffers);
        CHECK(p48.activeVoiceCount() == 1u);
        p48.processSample(chokePool, chokeBuffers);
        CHECK(p48.activeVoiceCount() == 0u);

        FragmentPlayer p96;
        p96.prepare(96000.0);
        CHECK(p96.trigger(chokePool, chokeBuffers, ref, 1.0f, 0.0f, 0.0f));
        p96.chokeAll();
        for (int i = 0; i < 191; ++i)
            p96.processSample(chokePool, chokeBuffers);
        CHECK(p96.activeVoiceCount() == 1u);
        p96.processSample(chokePool, chokeBuffers);
        CHECK(p96.activeVoiceCount() == 0u);
    }

    return 0;
}
