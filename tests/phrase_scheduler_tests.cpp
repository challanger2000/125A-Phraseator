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
    scheduler.prepare(48000.0);

    const float centerGain = std::sqrt(0.5f);

    // 16th at 120 BPM / 48 kHz = 6000 samples.
    // repeats=2 must retrigger halfway, at sample 3000.
    std::vector<float> left(3001u);
    std::vector<float> right(3001u);
    const bool firstBlockProduced = scheduler.processBlock(
        pool, buffers, 0.0, (1.0 / 6000.0), true, left.data(), right.data(), left.size());

    CHECK(firstBlockProduced);
    CHECK(std::fabs(left[0] - centerGain) < 1.0e-5f);
    CHECK(std::fabs(right[0] - centerGain) < 1.0e-5f);
    CHECK(std::fabs(left[3000] - centerGain) < 1.0e-5f);
    CHECK(std::fabs(right[3000] - centerGain) < 1.0e-5f);

    // Step 1 is delayed by 20% of one 16th = 1200 samples.
    std::vector<float> grooveLeft(1201u);
    std::vector<float> grooveRight(1201u);
    scheduler.processBlock(pool, buffers, 1.0, (1.0 / 6000.0), true,
                           grooveLeft.data(), grooveRight.data(), grooveLeft.size());

    CHECK(std::fabs(grooveLeft[0]) < 1.0e-8f);
    CHECK(std::fabs(grooveLeft[1199]) < 1.0e-8f);
    CHECK(std::fabs(grooveLeft[1200] - centerGain) < 1.0e-5f);

    const bool stoppedProduced = scheduler.processBlock(
        pool, buffers, (8000.0 / 6000.0), (1.0 / 6000.0), false, grooveLeft.data(), grooveRight.data(), 8u);
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
        seekScheduler.prepare(48000.0);

        float seekL[2] {};
        float seekR[2] {};
        CHECK(seekScheduler.processBlock(
            seekPool, seekBuffers, 0.0, (1.0 / 6000.0), true, seekL, seekR, 2u));
        CHECK(std::fabs(seekL[0] - centerGain) < 1.0e-5f);

        float jumpedL[1] {};
        float jumpedR[1] {};
        const bool jumpedProduced = seekScheduler.processBlock(
            seekPool, seekBuffers, 5.0, (1.0 / 6000.0), true,
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
        chokeScheduler.prepare(48000.0);

        std::vector<float> l(12120u);
        std::vector<float> r(12120u);
        CHECK(chokeScheduler.processBlock(
            chokePool, chokeBuffers, 0.0, (1.0 / 6000.0), true, l.data(), r.data(), l.size()));

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
        modeScheduler.prepare(48000.0);

        float continueL[1] {};
        float continueR[1] {};
        CHECK(modeScheduler.processBlock(
            modePool, modeBuffers, 1.0, (1.0 / 6000.0), true,
            continueL, continueR, 1u));
        CHECK(std::fabs(continueL[0] - 0.25f * centerGain) < 1.0e-5f);

        modeScheduler.reset();
        modeScheduler.prepare(48000.0);

        float retriggerL[1] {};
        float retriggerR[1] {};
        CHECK(modeScheduler.processBlock(
            modePool, modeBuffers, 0.0, (1.0 / 6000.0), true,
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
            sch.prepare(48000.0);

            std::vector<float> l(6000u);
            std::vector<float> rr(6000u);
            CHECK(sch.processBlock(
                ratchetPool, ratchetBuffers, 0.0, (1.0 / 6000.0), true,
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


    {
        // Invalid/stale fragment indices must never wrap to another source.
        SourcePool invalidPool;
        constexpr std::uint32_t frames = 8u;
        CHECK(invalidPool.setOneShot(0, 40u, frames, 48000.0, false));

        const float click[frames] {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
        std::array<AudioBufferView, kMaxSources> invalidBuffers {};
        invalidBuffers[0] = {click, nullptr, frames, false};

        Pattern invalidPattern {};
        invalidPattern[0].active = true;
        invalidPattern[0].fragment = 5u; // out of range; fragment count is 1
        invalidPattern[0].velocity = 1.0f;

        PhraseScheduler sch;
        sch.setPattern(invalidPattern);
        sch.prepare(48000.0);

        float l[16] {};
        float rr[16] {};
        const bool produced = sch.processBlock(
            invalidPool, invalidBuffers, 0.0, (1.0 / 6000.0), true, l, rr, 16u);

        CHECK(!produced);
        for (float x : l) CHECK(x == 0.0f);
        for (float x : rr) CHECK(x == 0.0f);
    }


    {
        // Tempo changes must not reinterpret historical sample time. The
        // scheduler runs in musical step units: after half a step at 120 BPM,
        // continuing at 60 BPM from the same musical position keeps the same
        // voice alive and reaches step 1 only after another half-step.
        SourcePool tempoPool;
        constexpr std::uint32_t frames = 20000u;
        CHECK(tempoPool.setOneShot(0, 50u, frames, 48000.0, false));
        CHECK(tempoPool.setOneShot(1, 51u, frames, 48000.0, false));

        static float first[frames];
        static float second[frames];
        for (std::uint32_t i = 0; i < frames; ++i) {
            first[i] = 0.5f;
            second[i] = 0.25f;
        }

        std::array<AudioBufferView, kMaxSources> tempoBuffers {};
        tempoBuffers[0] = {first, nullptr, frames, false};
        tempoBuffers[1] = {second, nullptr, frames, false};

        Pattern p {};
        p[0].active = true;
        p[0].fragment = 0u;
        p[0].velocity = 1.0f;
        p[1].active = true;
        p[1].fragment = 1u;
        p[1].velocity = 1.0f;

        PhraseScheduler sch;
        sch.setPattern(p);
        sch.prepare(48000.0);

        // 3000 samples @120 BPM = 0.5 sixteenth steps.
        std::vector<float> aL(3000u);
        std::vector<float> aR(3000u);
        CHECK(sch.processBlock(
            tempoPool, tempoBuffers,
            0.0, 1.0 / 6000.0, true,
            aL.data(), aR.data(), aL.size()));
        CHECK(std::fabs(aL.back() - 0.5f * centerGain) < 1.0e-4f);

        // At 60 BPM a full sixteenth is 12000 samples. Starting from musical
        // position 0.5, another 6000 samples reaches step 1 exactly.
        std::vector<float> bL(6001u);
        std::vector<float> bR(6001u);
        CHECK(sch.processBlock(
            tempoPool, tempoBuffers,
            0.5, 1.0 / 12000.0, true,
            bL.data(), bR.data(), bL.size()));

        CHECK(std::fabs(bL[0] - 0.5f * centerGain) < 1.0e-4f);
        CHECK(std::fabs(bL[5999] - 0.5f * centerGain) < 1.0e-4f);
        CHECK(bL[6000] > 0.25f * centerGain);
        CHECK(std::fabs(bL[6000] -
              (0.5f + 0.25f) * centerGain) < 1.0e-3f);
    }


    {
        // Musical scheduler timing remains sample-accurate at common host
        // rates. At 120 BPM one 16th is sampleRate/8 samples.
        for (const double rate : {44100.0, 96000.0}) {
            SourcePool ratePool;
            constexpr std::uint32_t frames = 4u;
            CHECK(ratePool.setOneShot(
                0, 60u, frames, rate, false));

            const float click[frames] {1.0f, 0.0f, 0.0f, 0.0f};
            std::array<AudioBufferView, kMaxSources> rateBuffers {};
            rateBuffers[0] = {click, nullptr, frames, false};

            Pattern p {};
            p[0].active = true;
            p[0].fragment = 0u;
            p[0].velocity = 1.0f;
            p[1] = p[0];

            PhraseScheduler sch;
            sch.setPattern(p);
            sch.prepare(rate);

            const auto samplesPerStep =
                static_cast<std::size_t>(std::llround(rate / 8.0));
            std::vector<float> l(samplesPerStep + 1u);
            std::vector<float> rr(samplesPerStep + 1u);
            CHECK(sch.processBlock(
                ratePool, rateBuffers,
                0.0, 8.0 / rate, true,
                l.data(), rr.data(), l.size()));

            CHECK(std::fabs(l[0] - centerGain) < 1.0e-5f);
            CHECK(std::fabs(l[samplesPerStep] - centerGain) < 1.0e-5f);
        }
    }


    {
        // Negative host musical time must run continuously through zero.
        // Absolute step -1 maps to pattern step 15; absolute step 0 maps to
        // pattern step 0. Clamping negative time to zero would retrigger step
        // 0 repeatedly during preroll instead of producing this sequence.
        SourcePool negativePool;
        constexpr std::uint32_t frames = 8u;
        CHECK(negativePool.setOneShot(0, 60u, frames, 48000.0, false));
        CHECK(negativePool.setOneShot(1, 61u, frames, 48000.0, false));

        const float a[frames] {0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
        const float b[frames] {0.25f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
        std::array<AudioBufferView, kMaxSources> negativeBuffers {};
        negativeBuffers[0] = {a, nullptr, frames, false};
        negativeBuffers[1] = {b, nullptr, frames, false};

        Pattern p {};
        p[15].active = true;
        p[15].fragment = 0u;
        p[15].velocity = 1.0f;
        p[0].active = true;
        p[0].fragment = 1u;
        p[0].velocity = 1.0f;

        PhraseScheduler sch;
        sch.setPattern(p);
        sch.prepare(48000.0);

        std::vector<float> l(6001u);
        std::vector<float> rr(6001u);
        CHECK(sch.processBlock(
            negativePool, negativeBuffers,
            -1.0, 1.0 / 6000.0, true,
            l.data(), rr.data(), l.size()));

        CHECK(std::fabs(l[0] - 0.5f * centerGain) < 1.0e-5f);
        CHECK(std::fabs(l[6000] - 0.25f * centerGain) < 1.0e-5f);
    }


    {
        // Malformed callers must not be able to force an effectively
        // unbounded boundary catch-up loop inside one audio sample.
        float l[4] {1.0f, 1.0f, 1.0f, 1.0f};
        float r[4] {1.0f, 1.0f, 1.0f, 1.0f};
        PhraseScheduler guarded;
        guarded.prepare(48000.0);
        CHECK(!guarded.processBlock(
            pool, buffers, 0.0, 1000000000.0, true, l, r, 4u));
        for (int i = 0; i < 4; ++i) {
            CHECK(l[i] == 0.0f);
            CHECK(r[i] == 0.0f);
        }
    }


    {
        // Stronger preroll continuity contract: a long voice started in
        // negative musical time must survive the -1 -> 0 boundary when the
        // step at zero is empty. A hidden scheduler reset at zero would turn
        // the second half silent even though no new active step occurred.
        SourcePool prerollPool;
        constexpr std::uint32_t frames = 12000u;
        CHECK(prerollPool.setOneShot(0, 70u, frames, 48000.0, false));

        static float tone[frames];
        for (auto& x : tone)
            x = 0.5f;

        std::array<AudioBufferView, kMaxSources> prerollBuffers {};
        prerollBuffers[0] = {tone, nullptr, frames, false};

        Pattern p {};
        p[15].active = true;
        p[15].fragment = 0u;
        p[15].velocity = 1.0f;
        p[0].active = false;

        PhraseScheduler sch;
        sch.setPattern(p);
        sch.prepare(48000.0);

        std::vector<float> l(6001u);
        std::vector<float> rr(6001u);
        CHECK(sch.processBlock(
            prerollPool, prerollBuffers,
            -0.5, 1.0 / 6000.0, true,
            l.data(), rr.data(), l.size()));

        CHECK(std::fabs(l[0] - 0.5f * centerGain) < 1.0e-4f);
        CHECK(std::fabs(l[2999] - 0.5f * centerGain) < 1.0e-4f);
        CHECK(std::fabs(l[3000] - 0.5f * centerGain) < 1.0e-4f);
        CHECK(std::fabs(l[6000] - 0.5f * centerGain) < 1.0e-4f);
    }

    return 0;
}
