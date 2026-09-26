#include "sample_load_worker.h"
#include "test_common.h"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <thread>
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

    const auto oneShotId = worker.requestLoad(oneShot);
    CHECK(oneShotId != 0u);

    SampleLoadWorkerResult result;
    CHECK(worker.waitForResult(oneShotId, result, std::chrono::seconds(2)));
    CHECK(result.ok());

    CHECK(exchange.consumePending());
    const auto* source0 = exchange.activeBank().sourcePool().source(0u);
    CHECK(source0 != nullptr);
    CHECK(source0->sliceCount == 1u);

    SampleLoadRequest loop;
    loop.sourceIndex = 1u;
    loop.sourceId = 200u;
    loop.path = path;
    loop.mode = SampleLoadMode::EqualSlices;
    loop.equalDivisions = 4u;

    const auto loopId = worker.requestLoad(loop);
    CHECK(loopId != 0u);
    CHECK(worker.waitForResult(loopId, result, std::chrono::seconds(2)));
    CHECK(result.ok());

    CHECK(exchange.consumePending());
    const auto* source1 = exchange.activeBank().sourcePool().source(1u);
    CHECK(source1 != nullptr);
    CHECK(source1->sliceCount == 4u);
    CHECK(exchange.activeBank().sourcePool().fragmentCount() == 5u);

    std::filesystem::remove(path, ec);
    CHECK(!ec);

    return 0;
}
