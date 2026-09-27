#include "sample_load_worker.h"
#include "test_common.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <cmath>
#include <vector>

using namespace phraseator;

namespace {

void appendU16(std::vector<std::uint8_t>& out, std::uint16_t v) {
    out.push_back(static_cast<std::uint8_t>(v & 0xFFu));
    out.push_back(static_cast<std::uint8_t>((v >> 8u) & 0xFFu));
}

void appendU32(std::vector<std::uint8_t>& out, std::uint32_t v) {
    out.push_back(static_cast<std::uint8_t>(v & 0xFFu));
    out.push_back(static_cast<std::uint8_t>((v >> 8u) & 0xFFu));
    out.push_back(static_cast<std::uint8_t>((v >> 16u) & 0xFFu));
    out.push_back(static_cast<std::uint8_t>((v >> 24u) & 0xFFu));
}

void appendId(std::vector<std::uint8_t>& out, const char id[5]) {
    for (int i = 0; i < 4; ++i)
        out.push_back(static_cast<std::uint8_t>(id[i]));
}

std::vector<std::uint8_t> makePulseMono16(std::uint32_t frames) {
    std::vector<std::int16_t> samples(frames, 0);
    const std::uint32_t hits[] {0u, 12000u, 24000u, 36000u};

    for (const auto hit : hits) {
        for (std::uint32_t i = 0; i < 240u && hit + i < frames; ++i) {
            const double gain = 1.0 - static_cast<double>(i) / 240.0;
            samples[hit + i] = static_cast<std::int16_t>(
                std::lround(gain * 28000.0));
        }
    }

    std::vector<std::uint8_t> out;
    const std::uint32_t dataBytes = frames * 2u;

    appendId(out, "RIFF");
    appendU32(out, 36u + dataBytes);
    appendId(out, "WAVE");
    appendId(out, "fmt ");
    appendU32(out, 16u);
    appendU16(out, 1u);
    appendU16(out, 1u);
    appendU32(out, 48000u);
    appendU32(out, 96000u);
    appendU16(out, 2u);
    appendU16(out, 16u);
    appendId(out, "data");
    appendU32(out, dataBytes);

    for (const auto sample : samples)
        appendU16(out, static_cast<std::uint16_t>(sample));

    return out;
}

std::vector<std::uint8_t> makePluckWithReverbMono16(std::uint32_t frames) {
    std::vector<double> samples(frames, 0.0);
    constexpr double twoPi = 6.28318530717958647692;

    const auto addPluck = [&](std::uint32_t start, double amplitude) {
        for (std::uint32_t i = 0; start + i < frames; ++i) {
            const double t = static_cast<double>(i) / 48000.0;
            const double env = std::exp(-t * 7.0);
            if (env < 1.0e-4)
                break;
            samples[start + i] +=
                amplitude * env * std::sin(twoPi * 440.0 * t);
        }
    };

    addPluck(0u, 0.85);
    addPluck(9000u, 0.22);
    addPluck(19000u, 0.11);
    addPluck(30000u, 0.055);

    std::vector<std::uint8_t> out;
    const std::uint32_t dataBytes = frames * 2u;

    appendId(out, "RIFF");
    appendU32(out, 36u + dataBytes);
    appendId(out, "WAVE");
    appendId(out, "fmt ");
    appendU32(out, 16u);
    appendU16(out, 1u);
    appendU16(out, 1u);
    appendU32(out, 48000u);
    appendU32(out, 96000u);
    appendU16(out, 2u);
    appendU16(out, 16u);
    appendId(out, "data");
    appendU32(out, dataBytes);

    for (const auto sample : samples) {
        const auto clamped = std::clamp(sample, -0.999, 0.999);
        const auto pcm = static_cast<std::int16_t>(
            std::lround(clamped * 32767.0));
        appendU16(out, static_cast<std::uint16_t>(pcm));
    }

    return out;
}

std::vector<std::uint8_t> makeMono16(std::uint32_t frames) {
    std::vector<std::uint8_t> out;
    const std::uint32_t dataBytes = frames * 2u;

    appendId(out, "RIFF");
    appendU32(out, 36u + dataBytes);
    appendId(out, "WAVE");

    appendId(out, "fmt ");
    appendU32(out, 16u);
    appendU16(out, 1u);
    appendU16(out, 1u);
    appendU32(out, 48000u);
    appendU32(out, 96000u);
    appendU16(out, 2u);
    appendU16(out, 16u);

    appendId(out, "data");
    appendU32(out, dataBytes);

    for (std::uint32_t i = 0; i < frames; ++i)
        appendU16(out, static_cast<std::uint16_t>((i & 1u) ? 0xC000u : 0x4000u));

    return out;
}

bool writeBytes(const std::filesystem::path& path,
                const std::vector<std::uint8_t>& bytes) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file)
        return false;

    file.write(reinterpret_cast<const char*>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
    return static_cast<bool>(file);
}

}

int main() {
    const auto path = std::filesystem::temp_directory_path() /
                      "125A_Phraseator_SampleLoadWorker_Test.wav";

    std::error_code ec;
    std::filesystem::remove(path, ec);
    CHECK(writeBytes(path, makeMono16(16u)));

    SampleBankExchange exchange;
    SampleLoadWorker worker(exchange);

    SampleLoadRequest oneShot;
    oneShot.sourceIndex = 0u;
    oneShot.sourceId = 100u;
    oneShot.path = path;
    oneShot.mode = SampleLoadMode::OneShot;

    std::atomic<bool> completionCalled {false};
    std::atomic<bool> completionOk {false};

    const auto oneShotId = worker.requestLoad(
        oneShot,
        true,
        [&](const SampleLoadWorkerResult& completed,
            const std::vector<SampleLoadRequest>& resolved) {
            completionOk.store(
                completed.ok() && resolved.size() == 1u,
                std::memory_order_release);
            completionCalled.store(true, std::memory_order_release);
        });
    CHECK(oneShotId != 0u);

    SampleLoadWorkerResult result;
    CHECK(worker.waitForResult(oneShotId, result, std::chrono::seconds(2)));
    CHECK(result.ok());
    CHECK(completionCalled.load(std::memory_order_acquire));
    CHECK(completionOk.load(std::memory_order_acquire));

    CHECK(exchange.consumePending());
    const auto* source0 = exchange.activeBank().sourcePool().source(0u);
    CHECK(source0 != nullptr);
    CHECK(source0->sliceCount == 1u);

    // Batch-load two changes and publish them in one bank swap. This must work
    // without requiring the audio side to consume an intermediate bank.
    SampleLoadRequest replace0;
    replace0.sourceIndex = 0u;
    replace0.sourceId = 101u;
    replace0.path = path;
    replace0.mode = SampleLoadMode::EqualSlices;
    replace0.equalDivisions = 2u;

    SampleLoadRequest loop1;
    loop1.sourceIndex = 1u;
    loop1.sourceId = 200u;
    loop1.path = path;
    loop1.mode = SampleLoadMode::EqualSlices;
    loop1.equalDivisions = 4u;

    std::vector<SampleLoadRequest> batch;
    batch.push_back(replace0);
    batch.push_back(loop1);

    const auto batchId = worker.requestBatch(std::move(batch), true);
    CHECK(batchId != 0u);
    CHECK(worker.waitForResult(batchId, result, std::chrono::seconds(2)));
    CHECK(result.ok());

    CHECK(exchange.consumePending());

    source0 = exchange.activeBank().sourcePool().source(0u);
    const auto* source1 = exchange.activeBank().sourcePool().source(1u);

    CHECK(source0 != nullptr);
    CHECK(source0->sourceId == 101u);
    CHECK(source0->sliceCount == 2u);

    CHECK(source1 != nullptr);
    CHECK(source1->sourceId == 200u);
    CHECK(source1->sliceCount == 4u);

    CHECK(exchange.activeBank().sourcePool().fragmentCount() == 6u);

    const auto transientPath = std::filesystem::temp_directory_path() /
                               "125A_Phraseator_TransientLoad_Test.wav";
    std::filesystem::remove(transientPath, ec);
    ec.clear();
    CHECK(writeBytes(transientPath, makePulseMono16(48000u)));

    SampleLoadRequest autoLoop;
    autoLoop.sourceIndex = 2u;
    autoLoop.sourceId = 300u;
    autoLoop.path = transientPath;
    autoLoop.mode = SampleLoadMode::EqualSlices;
    autoLoop.equalDivisions = 16u;
    autoLoop.preferTransient = true;

    std::atomic<std::uint16_t> resolvedSlices {0u};
    const auto transientId = worker.requestLoad(
        autoLoop,
        true,
        [&](const SampleLoadWorkerResult& completed,
            const std::vector<SampleLoadRequest>& resolved) {
            if (completed.ok() && resolved.size() == 1u)
                resolvedSlices.store(
                    resolved.front().resolvedSliceCount,
                    std::memory_order_release);
        });

    CHECK(transientId != 0u);
    CHECK(worker.waitForResult(transientId, result, std::chrono::seconds(2)));
    CHECK(result.ok());
    CHECK(exchange.consumePending());

    const auto* source2 = exchange.activeBank().sourcePool().source(2u);
    CHECK(source2 != nullptr);
    CHECK(source2->sliceCount == 4u);
    CHECK(resolvedSlices.load(std::memory_order_acquire) == 4u);

    // Exact recall must be able to bypass transient analysis and reuse stored
    // boundaries verbatim, even when transient preference is also enabled.
    SampleLoadRequest recalledLoop;
    recalledLoop.sourceIndex = 3u;
    recalledLoop.sourceId = 400u;
    recalledLoop.path = transientPath;
    recalledLoop.mode = SampleLoadMode::EqualSlices;
    recalledLoop.equalDivisions = 16u;
    recalledLoop.preferTransient = true;
    recalledLoop.useStoredSlices = true;
    recalledLoop.resolvedSliceCount = 2u;
    recalledLoop.resolvedSlices[0] = {0u, 10000u};
    recalledLoop.resolvedSlices[1] = {10000u, 48000u};

    const auto recalledId = worker.requestLoad(recalledLoop, true);
    CHECK(recalledId != 0u);
    CHECK(worker.waitForResult(recalledId, result, std::chrono::seconds(2)));
    CHECK(result.ok());
    CHECK(exchange.consumePending());

    const auto* source3 = exchange.activeBank().sourcePool().source(3u);
    CHECK(source3 != nullptr);
    CHECK(source3->sliceCount == 2u);
    CHECK(source3->slices[0].startFrame == 0u);
    CHECK(source3->slices[0].endFrame == 10000u);
    CHECK(source3->slices[1].startFrame == 10000u);
    CHECK(source3->slices[1].endFrame == 48000u);

    // AUTO mode must keep ordinary short material as a one-shot.
    SampleLoadRequest autoDropOne;
    autoDropOne.sourceIndex = 6u;
    autoDropOne.sourceId = 700u;
    autoDropOne.path = path;
    autoDropOne.mode = SampleLoadMode::Auto;
    autoDropOne.equalDivisions = 16u;

    std::atomic<int> autoOneMode {-1};
    const auto autoOneId = worker.requestLoad(
        autoDropOne,
        true,
        [&](const SampleLoadWorkerResult& completed,
            const std::vector<SampleLoadRequest>& resolved) {
            if (completed.ok() && resolved.size() == 1u)
                autoOneMode.store(
                    static_cast<int>(resolved.front().mode),
                    std::memory_order_release);
        });

    CHECK(autoOneId != 0u);
    CHECK(worker.waitForResult(autoOneId, result, std::chrono::seconds(2)));
    CHECK(result.ok());
    CHECK(exchange.consumePending());
    CHECK(autoOneMode.load(std::memory_order_acquire) ==
          static_cast<int>(SampleLoadMode::OneShot));

    const auto* autoOneSource =
        exchange.activeBank().sourcePool().source(6u);
    CHECK(autoOneSource != nullptr);
    CHECK(autoOneSource->type == SourceType::OneShot);
    CHECK(autoOneSource->sliceCount == 1u);

    // AUTO mode used by drag-and-drop should classify clear repeated
    // transients as a loop and publish the resolved mode to the callback.
    SampleLoadRequest autoDropLoop;
    autoDropLoop.sourceIndex = 4u;
    autoDropLoop.sourceId = 500u;
    autoDropLoop.path = transientPath;
    autoDropLoop.mode = SampleLoadMode::Auto;
    autoDropLoop.equalDivisions = 16u;

    std::atomic<int> autoLoopMode {-1};
    const auto autoLoopId = worker.requestLoad(
        autoDropLoop,
        true,
        [&](const SampleLoadWorkerResult& completed,
            const std::vector<SampleLoadRequest>& resolved) {
            if (completed.ok() && resolved.size() == 1u)
                autoLoopMode.store(
                    static_cast<int>(resolved.front().mode),
                    std::memory_order_release);
        });

    CHECK(autoLoopId != 0u);
    CHECK(worker.waitForResult(autoLoopId, result, std::chrono::seconds(2)));
    CHECK(result.ok());
    CHECK(exchange.consumePending());
    CHECK(autoLoopMode.load(std::memory_order_acquire) ==
          static_cast<int>(SampleLoadMode::EqualSlices));

    const auto* autoLoopSource =
        exchange.activeBank().sourcePool().source(4u);
    CHECK(autoLoopSource != nullptr);
    CHECK(autoLoopSource->type == SourceType::Loop);
    CHECK(autoLoopSource->sliceCount >= 2u);

    // A pluck with a decaying reverb tail can contain several transient-like
    // reflections. AUTO must still classify it as ONE, not LOOP.
    const auto pluckReverbPath = std::filesystem::temp_directory_path() /
                                 "125A_Phraseator_PluckReverb_Test.wav";
    std::filesystem::remove(pluckReverbPath, ec);
    ec.clear();
    CHECK(writeBytes(pluckReverbPath, makePluckWithReverbMono16(48000u)));

    SampleLoadRequest autoPluck;
    autoPluck.sourceIndex = 5u;
    autoPluck.sourceId = 600u;
    autoPluck.path = pluckReverbPath;
    autoPluck.mode = SampleLoadMode::Auto;
    autoPluck.equalDivisions = 16u;

    std::atomic<int> autoPluckMode {-1};
    const auto autoPluckId = worker.requestLoad(
        autoPluck,
        true,
        [&](const SampleLoadWorkerResult& completed,
            const std::vector<SampleLoadRequest>& resolved) {
            if (completed.ok() && resolved.size() == 1u)
                autoPluckMode.store(
                    static_cast<int>(resolved.front().mode),
                    std::memory_order_release);
        });

    CHECK(autoPluckId != 0u);
    CHECK(worker.waitForResult(autoPluckId, result, std::chrono::seconds(2)));
    CHECK(result.ok());
    CHECK(exchange.consumePending());
    CHECK(autoPluckMode.load(std::memory_order_acquire) ==
          static_cast<int>(SampleLoadMode::OneShot));

    const auto* autoPluckSource =
        exchange.activeBank().sourcePool().source(5u);
    CHECK(autoPluckSource != nullptr);
    CHECK(autoPluckSource->type == SourceType::OneShot);
    CHECK(autoPluckSource->sliceCount == 1u);

    // Clearing a slot must remove both its audio and all fragments from the
    // published bank, without requiring a file path.
    const auto beforeClearFragments =
        exchange.activeBank().sourcePool().fragmentCount();

    SampleLoadRequest clearSlot;
    clearSlot.sourceIndex = 1u;
    clearSlot.mode = SampleLoadMode::Clear;

    const auto clearId = worker.requestLoad(clearSlot, true);
    CHECK(clearId != 0u);
    CHECK(worker.waitForResult(clearId, result, std::chrono::seconds(2)));
    CHECK(result.ok());
    CHECK(exchange.consumePending());

    CHECK(exchange.activeBank().sourcePool().source(1u) == nullptr);
    CHECK(!exchange.activeBank().buffers()[1u].valid());
    CHECK(exchange.activeBank().sourcePool().fragmentCount() + 4u ==
          beforeClearFragments);

    std::filesystem::remove(path, ec);
    CHECK(!ec);
    ec.clear();
    std::filesystem::remove(transientPath, ec);
    CHECK(!ec);
    ec.clear();
    std::filesystem::remove(pluckReverbPath, ec);
    CHECK(!ec);

    return 0;
}
