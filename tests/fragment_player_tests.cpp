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


    {
        // PAN is a true live macro on an already-running voice. The voice
        // stores its pan tendency and the global amount can move it without
        // retriggering, but automation is de-clicked instead of jumping.
        SourcePool panPool;
        constexpr std::uint32_t frames = 4096u;
        CHECK(panPool.setOneShot(0, 12u, frames, 48000.0, false));

        static float constant[frames] {};
        for (auto& x : constant) x = 1.0f;
        std::array<AudioBufferView, kMaxSources> panBuffers {};
        panBuffers[0] = {constant, nullptr, frames, false};

        FragmentRef panRef {0u, 0u};
        FragmentPlayer player;
        player.prepare(48000.0);
        player.setPanAmount(0.0f);
        CHECK(player.trigger(panPool, panBuffers, panRef, 1.0f, 1.0f, 0.0f));

        const auto centered = player.processSample(panPool, panBuffers);
        CHECK(std::fabs(centered.left - centered.right) < 1.0e-6f);

        player.setPanAmount(1.0f);
        const auto firstMove = player.processSample(panPool, panBuffers);
        CHECK(std::fabs(firstMove.left - centered.left) < 0.05f);
        CHECK(std::fabs(firstMove.right - centered.right) < 0.05f);

        StereoFrame right {};
        for (int i = 0; i < 1600; ++i)
            right = player.processSample(panPool, panBuffers);
        CHECK(right.left < 0.01f);
        CHECK(right.right > 0.99f);

        player.setPanAmount(0.0f);
        const auto firstReturn = player.processSample(panPool, panBuffers);
        CHECK(std::fabs(firstReturn.left - right.left) < 0.05f);
        CHECK(std::fabs(firstReturn.right - right.right) < 0.05f);

        StereoFrame centeredAgain {};
        for (int i = 0; i < 1600; ++i)
            centeredAgain = player.processSample(panPool, panBuffers);
        CHECK(std::fabs(centeredAgain.left - centeredAgain.right) < 0.01f);
    }


    {
        // Source-specific mute choke must release only voices from that source.
        SourcePool mutePool;
        constexpr std::uint32_t frames = 256u;
        CHECK(mutePool.setOneShot(0, 21u, frames, 48000.0, false));
        CHECK(mutePool.setOneShot(1, 22u, frames, 48000.0, false));

        float a[frames] {};
        float b[frames] {};
        for (auto& x : a) x = 1.0f;
        for (auto& x : b) x = 0.5f;

        std::array<AudioBufferView, kMaxSources> muteBuffers {};
        muteBuffers[0] = {a, nullptr, frames, false};
        muteBuffers[1] = {b, nullptr, frames, false};

        FragmentPlayer player;
        player.prepare(48000.0);
        CHECK(player.trigger(mutePool, muteBuffers, {0u, 0u}, 1.0f, 0.0f, 0.0f));
        CHECK(player.trigger(mutePool, muteBuffers, {1u, 0u}, 1.0f, 0.0f, 0.0f));
        CHECK(player.activeVoiceCount() == 2u);

        player.chokeSource(0u);
        for (int i = 0; i < 96; ++i)
            player.processSample(mutePool, muteBuffers);

        CHECK(player.activeVoiceCount() == 1u);
    }


    {
        // Fractional interpolation at a slice boundary must not leak samples
        // from the next slice.
        SourcePool slicedPool;
        constexpr std::uint32_t frames = 8u;
        const SliceRegion slices[2] {{0u, 4u}, {4u, 8u}};
        CHECK(slicedPool.setLoopSlices(
            0, 31u, frames, 48000.0, false, slices, 2u));

        const float data[frames] {
            1.0f, 1.0f, 1.0f, 1.0f,
            -1.0f, -1.0f, -1.0f, -1.0f
        };
        std::array<AudioBufferView, kMaxSources> slicedBuffers {};
        slicedBuffers[0] = {data, nullptr, frames, false};

        FragmentPlayer player;
        player.prepare(48000.0);
        CHECK(player.trigger(
            slicedPool, slicedBuffers, {0u, 0u}, 1.0f, -1.0f, -12.0f));

        // -12 semitones advances by 0.5 frames. Every sample from slice 0
        // must remain non-negative; interpolation must never see slice 1.
        while (player.activeVoiceCount() > 0u) {
            const auto frame = player.processSample(slicedPool, slicedBuffers);
            CHECK(frame.left >= -1.0e-6f);
        }
    }

    {
        // Slice fade and choke release multiply; the choke must audibly decay
        // rather than remain flat until its final forced stop.
        SourcePool loopPool;
        constexpr std::uint32_t frames = 512u;
        const SliceRegion region {0u, frames};
        CHECK(loopPool.setLoopSlices(
            0, 32u, frames, 48000.0, false, &region, 1u));

        float constant[frames] {};
        for (auto& x : constant) x = 1.0f;
        std::array<AudioBufferView, kMaxSources> buffers {};
        buffers[0] = {constant, nullptr, frames, false};

        FragmentPlayer player;
        player.prepare(48000.0);
        CHECK(player.trigger(
            loopPool, buffers, {0u, 0u}, 1.0f, -1.0f, 0.0f));

        // Move past the slice fade-in.
        for (int i = 0; i < 32; ++i)
            player.processSample(loopPool, buffers);

        player.chokeAll();
        const auto releaseStart = player.processSample(loopPool, buffers);
        StereoFrame releaseLate {};
        for (int i = 0; i < 80; ++i)
            releaseLate = player.processSample(loopPool, buffers);

        CHECK(releaseStart.left > releaseLate.left);
        CHECK(releaseLate.left >= 0.0f);
    }

    {
        // Boundary contract: Processor::setupProcessing accepts exactly
        // 1000 Hz, so the player must not silently fall back to 48 kHz.
        SourcePool boundaryPool;
        CHECK(boundaryPool.setOneShot(0, 99u, 4u, 1000.0, false));
        const float data[4] {1.0f, 0.5f, 0.0f, -0.5f};
        std::array<AudioBufferView, kMaxSources> boundaryBuffers {};
        boundaryBuffers[0] = {data, nullptr, 4u, false};

        FragmentPlayer boundaryPlayer;
        boundaryPlayer.prepare(1000.0);
        CHECK(boundaryPlayer.trigger(
            boundaryPool, boundaryBuffers, {0u, 0u}, 1.0f, -1.0f, 0.0f));
        const auto first = boundaryPlayer.processSample(boundaryPool, boundaryBuffers);
        const auto second = boundaryPlayer.processSample(boundaryPool, boundaryBuffers);
        CHECK(std::fabs(first.left - 1.0f) < 1.0e-5f);
        CHECK(std::fabs(second.left - 0.5f) < 1.0e-5f);
    }

    {
        // Valid low-rate WAV/source metadata must preserve source-rate
        // conversion instead of being silently treated as host-rate audio.
        SourcePool lowRatePool;
        CHECK(lowRatePool.setOneShot(0, 100u, 4u, 500.0, false));
        const float data[4] {1.0f, 0.5f, 0.0f, -0.5f};
        std::array<AudioBufferView, kMaxSources> lowRateBuffers {};
        lowRateBuffers[0] = {data, nullptr, 4u, false};

        FragmentPlayer lowRatePlayer;
        lowRatePlayer.prepare(1000.0);
        CHECK(lowRatePlayer.trigger(
            lowRatePool, lowRateBuffers, {0u, 0u}, 1.0f, -1.0f, 0.0f));
        const auto first = lowRatePlayer.processSample(lowRatePool, lowRateBuffers);
        const auto second = lowRatePlayer.processSample(lowRatePool, lowRateBuffers);
        CHECK(std::fabs(first.left - 1.0f) < 1.0e-5f);
        CHECK(std::fabs(second.left - 0.75f) < 1.0e-5f);
    }

    {
        // Live PAN automation on a running voice must not create a one-sample
        // gain jump. The dormant state may snap, but an active voice ramps.
        SourcePool panPool;
        constexpr std::uint32_t frames = 1024u;
        CHECK(panPool.setOneShot(0, 101u, frames, 48000.0, false));

        static float tone[frames];
        for (auto& x : tone)
            x = 1.0f;

        std::array<AudioBufferView, kMaxSources> panBuffers {};
        panBuffers[0] = {tone, nullptr, frames, false};

        FragmentPlayer p;
        p.prepare(48000.0);
        p.setPanAmount(0.0f);
        CHECK(p.trigger(
            panPool, panBuffers, {0u, 0u}, 1.0f, 1.0f, 0.0f));

        const auto before = p.processSample(panPool, panBuffers);
        p.setPanAmount(1.0f);
        const auto after = p.processSample(panPool, panBuffers);

        CHECK(std::fabs(after.left - before.left) < 0.05f);
        CHECK(std::fabs(after.right - before.right) < 0.05f);

        StereoFrame settled {};
        for (int i = 0; i < 800; ++i)
            settled = p.processSample(panPool, panBuffers);
        CHECK(settled.right > settled.left);
    }

    return 0;
}
