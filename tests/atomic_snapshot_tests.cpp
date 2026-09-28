#include "../vst3/AtomicSnapshot.h"
#include "test_common.h"

#include <atomic>
#include <cstdint>
#include <thread>

using phraseator::vst3::AtomicSnapshot;

namespace {
struct SnapshotPayload {
    std::uint64_t generation {0u};
    std::uint64_t inverse {~0u};
    std::uint64_t mirror {0u};
    std::uint64_t checksum {0u};
};

SnapshotPayload makePayload(std::uint64_t generation) {
    SnapshotPayload p;
    p.generation = generation;
    p.inverse = ~generation;
    p.mirror = generation ^ 0xA55AA55AA55AA55Aull;
    p.checksum = p.generation ^ p.inverse ^ p.mirror;
    return p;
}

bool coherent(const SnapshotPayload& p) {
    return p.inverse == ~p.generation &&
           p.mirror == (p.generation ^ 0xA55AA55AA55AA55Aull) &&
           p.checksum == (p.generation ^ p.inverse ^ p.mirror);
}
}

int main() {
    AtomicSnapshot<SnapshotPayload> snapshot;
    snapshot.store(makePayload(0u));

    std::atomic<bool> done {false};
    std::atomic<bool> failed {false};

    std::thread writer([&] {
        for (std::uint64_t i = 1u; i <= 200000u; ++i)
            snapshot.store(makePayload(i));
        done.store(true, std::memory_order_release);
    });

    std::uint64_t lastSeen = 0u;
    while (!done.load(std::memory_order_acquire)) {
        SnapshotPayload p {};
        std::uint64_t seq = 0u;
        if (!snapshot.tryLoad(p, seq))
            continue;
        if (!coherent(p) || p.generation < lastSeen) {
            failed.store(true, std::memory_order_release);
            break;
        }
        lastSeen = p.generation;
    }

    writer.join();
    CHECK(!failed.load(std::memory_order_acquire));

    SnapshotPayload final {};
    std::uint64_t seq = 0u;
    CHECK(snapshot.tryLoad(final, seq));
    CHECK(coherent(final));
    CHECK(final.generation == 200000u);
    CHECK((seq & 1u) == 0u);
    return 0;
}
