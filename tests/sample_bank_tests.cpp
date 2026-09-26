#include "sample_bank.h"

#include <cassert>

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

    assert(exchange.activeIndex() == 0);
    assert(exchange.activeBank().sourcePool().fragmentCount() == 0u);

    const int writeIndex = exchange.beginWrite();
    assert(writeIndex == 1);

    auto* bank = exchange.writableBank(writeIndex);
    assert(bank != nullptr);
    bank->clear();

    auto source = makeMono();
    assert(bank->setOneShot(0, 101u, std::move(source)));

    // Publish does not mutate the realtime-visible bank until the audio thread consumes it.
    assert(exchange.commitWrite(writeIndex));
    assert(exchange.activeBank().sourcePool().fragmentCount() == 0u);

    assert(exchange.consumePending());
    assert(exchange.activeIndex() == 1);
    assert(exchange.activeBank().sourcePool().fragmentCount() == 1u);
    assert(exchange.activeBank().buffers()[0].valid());
    assert(exchange.activeBank().buffers()[0].frames == 3u);

    // While no pending bank exists, the old active bank becomes the next safe write target.
    const int secondWrite = exchange.beginWrite();
    assert(secondWrite == 0);
    auto* secondBank = exchange.writableBank(secondWrite);
    assert(secondBank != nullptr);
    secondBank->clear();

    SliceRegion slices[2] {{0u, 1u}, {1u, 3u}};
    auto loop = makeMono();
    assert(secondBank->setLoop(0, 202u, std::move(loop), slices, 2u));
    assert(exchange.commitWrite(secondWrite));
    assert(exchange.consumePending());
    assert(exchange.activeBank().sourcePool().fragmentCount() == 2u);

    return 0;
}
