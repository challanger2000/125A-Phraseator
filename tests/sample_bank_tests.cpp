#include "sample_bank.h"
#include "test_common.h"


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

    return 0;
}
