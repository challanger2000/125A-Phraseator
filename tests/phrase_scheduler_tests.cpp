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
    const bool firstBlockProduced = scheduler.processBlock(
        pool, buffers, 0.0, true, left.data(), right.data(), left.size());

    CHECK(firstBlockProduced);
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

    const bool stoppedProduced = scheduler.processBlock(
        pool, buffers, 8000.0, false, grooveLeft.data(), grooveRight.data(), 8u);
    CHECK(!stoppedProduced);
    for (std::size_t i = 0; i < 8u; ++i)
        CHECK(grooveLeft[i] == 0.0f);

    {
        // Transport discontinuity: a long voice started at step 0 must not
        // bleed into an unrelated inactive step after a host seek.
        SourcePool seekPool;
        constexpr std::uint32_t seekFrames = 16u;
        CHECK(seekPool.setOneShot(0, 2u, seekFrames, 48000.0, false));

        const float longSample[seekFrames] {
            1.0f, 0.25f, 0.25f, 0.25f,
            0.25f, 0.25f, 0.25f, 0.25f,
            0.25f, 0.25f, 0.25f, 0.25f,
            0.25f, 0.25f, 0.25f, 0.25f
        };

        std::array<AudioBufferView, kMaxSources> seekBuffers {};
        seekBuffers[0] = {longSample, nullptr, seekFrames, false};

        Pattern seekPattern {};
        seekPattern[0].active = true;
        seekPattern[0].fragment = 0;
        seekPattern[0].velocity = 1.0f;
        seekPattern[0].pan = 0.0f;

        PhraseScheduler seekScheduler;
        seekScheduler.setPattern(seekPattern);
        seekScheduler.prepare(48000.0, 120.0);

        float seekL[2] {};
        float seekR[2] {};
        CHECK(seekScheduler.processBlock(
            seekPool, seekBuffers, 0.0, true, seekL, seekR, 2u));
        CHECK(std::fabs(seekL[0] - centerGain) < 1.0e-5f);

        float jumpedL[1] {};
        float jumpedR[1] {};
        const bool jumpedProduced = seekScheduler.processBlock(
            seekPool, seekBuffers, 30000.0, true,
            jumpedL, jumpedR, 1u);

        CHECK(!jumpedProduced);
        CHECK(std::fabs(jumpedL[0]) < 1.0e-8f);
        CHECK(std::fabs(jumpedR[0]) < 1.0e-8f);
    }


    {
        // Long one-shots are phrase-monophonic: an empty step lets the current
        // voice continue, but the next active step from any source chokes it.
        SourcePool chokePool;
        constexpr std::uint32_t frames = 20000u;
        CHECK(chokePool.setOneShot(0, 10u, frames, 48000.0, false));
        CHECK(chokePool.setOneShot(1, 11u, frames, 48000.0, false));

        static float a[frames];
        static float b[frames];
        for (std::uint32_t i = 0; i < frames; ++i) {
            a[i] = 0.5f;
            b[i] = 0.25f;
        }

        std::array<AudioBufferView, kMaxSources> chokeBuffers {};
        chokeBuffers[0] = {a, nullptr, frames, false};
        chokeBuffers[1] = {b, nullptr, frames, false};

        Pattern chokePattern {};
        chokePattern[0].active = true;
        chokePattern[0].fragment = 0;
        chokePattern[0].velocity = 1.0f;
        chokePattern[0].pan = 0.0f;
        // Step 1 is empty: source 0 must keep sounding.
        chokePattern[2].active = true;
        chokePattern[2].fragment = 1;
        chokePattern[2].velocity = 1.0f;
        chokePattern[2].pan = 0.0f;

        PhraseScheduler chokeScheduler;
        chokeScheduler.setPattern(chokePattern);
        chokeScheduler.prepare(48000.0, 120.0);

        std::vector<float> l(12120u);
        std::vector<float> r(12120u);
        CHECK(chokeScheduler.processBlock(
            chokePool, chokeBuffers, 0.0, true, l.data(), r.data(), l.size()));

        CHECK(std::fabs(l[6000] - 0.5f * centerGain) < 1.0e-4f);
        CHECK(std::fabs(l[11999] - 0.5f * centerGain) < 1.0e-4f);
        // Step 2 begins at sample 12000. Source 0 would still be sounding
        // here without the choke, so this proves the new step ends it.
        CHECK(l[12000] > 0.25f * centerGain);
        CHECK(std::fabs(l[12110] - 0.25f * centerGain) < 1.0e-4f);
    }


    {
        // Phrase Mode contract:
        // CONTINUE feeds absolute host time into the scheduler, so a note
        // starting on the second 16th must continue at pattern step 1.
        // RETRIGGER resets and feeds phrase-local time 0, so the same note
        // must start from pattern step 0.
        SourcePool modePool;
        constexpr std::uint32_t modeFrames = 32u;
        CHECK(modePool.setOneShot(0, 20u, modeFrames, 48000.0, false));
        CHECK(modePool.setOneShot(1, 21u, modeFrames, 48000.0, false));

        float first[modeFrames] {};
        float second[modeFrames] {};
        for (auto& x : first) x = 1.0f;
        for (auto& x : second) x = 0.25f;

        std::array<AudioBufferView, kMaxSources> modeBuffers {};
        modeBuffers[0] = {first, nullptr, modeFrames, false};
        modeBuffers[1] = {second, nullptr, modeFrames, false};

        Pattern modePattern {};
        modePattern[0].active = true;
        modePattern[0].fragment = 0;
        modePattern[0].velocity = 1.0f;
        modePattern[1].active = true;
        modePattern[1].fragment = 1;
        modePattern[1].velocity = 1.0f;

        PhraseScheduler modeScheduler;
        modeScheduler.setPattern(modePattern);
        modeScheduler.prepare(48000.0, 120.0);

        float continueL[1] {};
        float continueR[1] {};
        CHECK(modeScheduler.processBlock(
            modePool, modeBuffers, 6000.0, true,
            continueL, continueR, 1u));
        CHECK(std::fabs(continueL[0] - 0.25f * centerGain) < 1.0e-5f);

        modeScheduler.reset();
        modeScheduler.prepare(48000.0, 120.0);

        float retriggerL[1] {};
        float retriggerR[1] {};
        CHECK(modeScheduler.processBlock(
            modePool, modeBuffers, 0.0, true,
            retriggerL, retriggerR, 1u));
        CHECK(std::fabs(retriggerL[0] - centerGain) < 1.0e-5f);
    }


    {
        // Exact ratchet timing at 120 BPM / 48 kHz: one 16th is 6000 samples.
        SourcePool ratchetPool;
        constexpr std::uint32_t frames = 4u;
        CHECK(ratchetPool.setOneShot(0, 30u, frames, 48000.0, false));

        const float click[frames] {1.0f, 0.0f, 0.0f, 0.0f};
        std::array<AudioBufferView, kMaxSources> ratchetBuffers {};
        ratchetBuffers[0] = {click, nullptr, frames, false};

        for (int hits = 1; hits <= 4; ++hits) {
            Pattern p {};
            p[0].active = true;
            p[0].fragment = 0u;
            p[0].velocity = 1.0f;
            p[0].pan = 0.0f;
            p[0].repeats = static_cast<std::uint8_t>(hits);

            PhraseScheduler sch;
            sch.setPattern(p);
            sch.prepare(48000.0, 120.0);

            std::vector<float> l(6000u);
            std::vector<float> rr(6000u);
            CHECK(sch.processBlock(
                ratchetPool, ratchetBuffers, 0.0, true,
                l.data(), rr.data(), l.size()));

            int impulses = 0;
            for (std::size_t i = 0; i < l.size(); ++i) {
                if (std::fabs(l[i] - centerGain) < 1.0e-5f)
                    ++impulses;
            }
            CHECK(impulses == hits);

            for (int n = 0; n < hits; ++n) {
                const auto expected = static_cast<std::size_t>(
                    std::llround(static_cast<double>(n) * 6000.0 /
                                 static_cast<double>(hits)));
                CHECK(std::fabs(l[expected] - centerGain) < 1.0e-5f);
            }
        }
    }

    return 0;
}
