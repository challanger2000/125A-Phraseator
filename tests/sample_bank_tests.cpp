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
    CHECK(exchange.commitWrite(writeIndex));
    CHECK(exchange.activeBank().sourcePool().fragmentCount() == 0u);

    CHECK(exchange.consumePending());
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
    CHECK(exchange.commitWrite(secondWrite));
    CHECK(exchange.consumePending());
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

    return 0;
}
