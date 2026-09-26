#include "sample_file_loader.h"
#include "test_common.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
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

std::vector<std::uint8_t> makeMono16() {
    std::vector<std::uint8_t> out;
    appendId(out, "RIFF");
    appendU32(out, 40u);
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
    appendU32(out, 4u);
    appendU16(out, 0x4000u);
    appendU16(out, 0xC000u);
    return out;
}

}

int main() {
    const auto path = std::filesystem::temp_directory_path() /
                      "125A_Phraseator_SampleFileLoader_Test.wav";

    std::error_code ec;
    std::filesystem::remove(path, ec);

    const auto bytes = makeMono16();
    {
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        CHECK(static_cast<bool>(file));
        file.write(reinterpret_cast<const char*>(bytes.data()),
                   static_cast<std::streamsize>(bytes.size()));
        CHECK(static_cast<bool>(file));
    }

    OwnedAudioSource decoded;
    const auto result = SampleFileLoader::loadWav(path, decoded);

    CHECK(result.ok());
    CHECK(result.wavStatus == WavDecodeStatus::Ok);
    CHECK(decoded.valid());
    CHECK(decoded.sampleRate == 48000u);
    CHECK(decoded.frames() == 2u);
    CHECK(decoded.left[0] > 0.49f && decoded.left[0] < 0.51f);
    CHECK(decoded.left[1] < -0.49f && decoded.left[1] > -0.51f);

    std::filesystem::remove(path, ec);
    CHECK(!ec);

    OwnedAudioSource missing;
    const auto missingResult = SampleFileLoader::loadWav(path, missing);
    CHECK(missingResult.status == SampleFileLoadStatus::OpenFailed);
    CHECK(!missing.valid());

    return 0;
}
