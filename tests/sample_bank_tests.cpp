#include "sample_bank.h"
#include "test_common.h"

#include <cmath>


using namespace phraseator;

namespace {

OwnedAudioSource makeMono() {
    OwnedAudioSource source;
    source.sampleRate = 48000u;
    source.stereo = false;
    source.left = {1.0f, 0.0f, -1.0f};
    return source;
}

}

int main() {
    SampleBankExchange exchange;

    CHECK(exchange.activeIndex() == 0);
    CHECK(exchange.activeBank().sourcePool().fragmentCount() == 0u);

    const int writeIndex = exchange.beginWrite();
    CHECK(writeIndex == 1);

    auto* bank = exchange.writableBank(writeIndex);
    CHECK(bank != nullptr);
    bank->clear();

    auto source = makeMono();
    CHECK(bank->setOneShot(0, 101u, std::move(source)));

    // Publish does not mutate the realtime-visible bank until the audio thread consumes it.
    CHECK(exchange.commitWrite(writeIndex, 10101u));
    CHECK(exchange.activeBank().sourcePool().fragmentCount() == 0u);
    CHECK(exchange.pendingPublishTag() == 10101u);

    std::uint64_t firstTag = 0u;
    CHECK(exchange.consumePending(&firstTag));
    CHECK(firstTag == 10101u);
    CHECK(exchange.activePublishTag() == 10101u);
    CHECK(exchange.pendingPublishTag() == 0u);
    CHECK(exchange.activeIndex() == 1);
    CHECK(exchange.activeBank().sourcePool().fragmentCount() == 1u);
    CHECK(exchange.activeBank().buffers()[0].valid());
    CHECK(exchange.activeBank().buffers()[0].frames == 3u);

    // While no pending bank exists, the old active bank becomes the next safe write target.
    const int secondWrite = exchange.beginWrite();
    CHECK(secondWrite == 0);
    auto* secondBank = exchange.writableBank(secondWrite);
    CHECK(secondBank != nullptr);
    secondBank->clear();

    SliceRegion slices[2] {{0u, 1u}, {1u, 3u}};
    auto loop = makeMono();
    CHECK(secondBank->setLoop(0, 202u, std::move(loop), slices, 2u));
    CHECK(exchange.commitWrite(secondWrite, 20202u));
    std::uint64_t secondTag = 0u;
    CHECK(exchange.consumePending(&secondTag));
    CHECK(secondTag == 20202u);
    CHECK(exchange.activePublishTag() == 20202u);
    CHECK(exchange.activeBank().sourcePool().fragmentCount() == 2u);

    {
        // Copying a bank must rebind AudioBufferView pointers to the copied
        // OwnedAudioSource storage. Otherwise loading source 2 can leave
        // source 1's view pointing at the previous bank.
        SampleBank original;
        auto first = makeMono();
        CHECK(original.setOneShot(0, 301u, std::move(first)));

        SampleBank copied = original;
        CHECK(copied.sourcePool().fragmentCount() == 1u);
        CHECK(copied.buffers()[0].valid());
        CHECK(copied.buffers()[0].left != original.buffers()[0].left);
        CHECK(copied.buffers()[0].left[0] > 0.0f);

        auto second = makeMono();
        CHECK(copied.setOneShot(1, 302u, std::move(second)));
        CHECK(copied.sourcePool().fragmentCount() == 2u);
        CHECK(copied.buffers()[0].valid());
        CHECK(copied.buffers()[1].valid());
        CHECK(copied.buffers()[0].left[0] > 0.0f);
        CHECK(copied.buffers()[1].left[0] > 0.0f);
        CHECK(std::fabs(copied.buffers()[0].left[0] - copied.buffers()[1].left[0]) < 1.0e-6f);
    }


    {
        // Conservative auto level should bring the same one-shot recorded at
        // very different amplitudes to a comparable playback level.
        SampleBank levelBank;
        auto loud = makeMono();
        auto quiet = makeMono();
        for (auto& x : quiet.left)
            x *= 0.1f;

        CHECK(levelBank.setOneShot(0, 401u, std::move(loud)));
        CHECK(levelBank.setOneShot(1, 402u, std::move(quiet)));
        CHECK(levelBank.buffers()[0].valid());
        CHECK(levelBank.buffers()[1].valid());

        const float loudPeak = std::fabs(levelBank.buffers()[0].left[0]);
        const float quietPeak = std::fabs(levelBank.buffers()[1].left[0]);
        CHECK(std::fabs(loudPeak - quietPeak) < 1.0e-4f);
        CHECK(loudPeak <= 0.89f + 1.0e-6f);
    }


    {
        // Auto-level must remain finite and bounded for stereo, extreme low
        // level and silence-like material.
        SampleBank levelBank;

        OwnedAudioSource stereo;
        stereo.sampleRate = 48000u;
        stereo.stereo = true;
        stereo.left = {0.8f, -0.8f, 0.4f, -0.4f};
        stereo.right = {0.4f, -0.4f, 0.2f, -0.2f};
        CHECK(levelBank.setOneShot(0, 501u, std::move(stereo)));
        const auto stereoView = levelBank.buffers()[0];
        CHECK(stereoView.valid());
        for (std::uint32_t i = 0; i < stereoView.frames; ++i) {
            CHECK(std::isfinite(stereoView.left[i]));
            CHECK(std::isfinite(stereoView.right[i]));
            CHECK(std::fabs(stereoView.left[i]) <= 0.89f + 1.0e-6f);
            CHECK(std::fabs(stereoView.right[i]) <= 0.89f + 1.0e-6f);
        }

        OwnedAudioSource veryQuiet;
        veryQuiet.sampleRate = 48000u;
        veryQuiet.stereo = false;
        veryQuiet.left = {0.001f, -0.001f, 0.0005f, -0.0005f};
        CHECK(levelBank.setOneShot(1, 502u, std::move(veryQuiet)));
        const auto quietView = levelBank.buffers()[1];
        CHECK(quietView.valid());
        CHECK(std::fabs(quietView.left[0]) <= 0.004001f); // +12 dB cap (4x)

        OwnedAudioSource hot;
        hot.sampleRate = 48000u;
        hot.stereo = false;
        hot.left = {4.0f, -4.0f, 2.0f, -2.0f};
        CHECK(levelBank.setOneShot(2, 503u, std::move(hot)));
        const auto hotView = levelBank.buffers()[2];
        CHECK(hotView.valid());
        for (std::uint32_t i = 0; i < hotView.frames; ++i) {
            CHECK(std::isfinite(hotView.left[i]));
            CHECK(std::fabs(hotView.left[i]) <= 0.89f + 1.0e-6f);
        }

        OwnedAudioSource silence;
        silence.sampleRate = 48000u;
        silence.stereo = false;
        silence.left = {0.0f, 0.0f, 0.0f, 0.0f};
        CHECK(levelBank.setOneShot(3, 504u, std::move(silence)));
        const auto silenceView = levelBank.buffers()[3];
        CHECK(silenceView.valid());
        for (std::uint32_t i = 0; i < silenceView.frames; ++i)
            CHECK(silenceView.left[i] == 0.0f);
    }

    return 0;
}
