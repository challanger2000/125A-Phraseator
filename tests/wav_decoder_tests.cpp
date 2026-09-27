#include "wav_decoder.h"
#include "test_common.h"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <iterator>
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
    appendU32(out, 42u);
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
    appendU32(out, 6u);
    appendU16(out, 0x8000u);
    appendU16(out, 0x0000u);
    appendU16(out, 0x7FFFu);
    return out;
}

std::vector<std::uint8_t> makeStereoFloat() {
    std::vector<std::uint8_t> out;
    appendId(out, "RIFF");
    appendU32(out, 52u);
    appendId(out, "WAVE");

    appendId(out, "fmt ");
    appendU32(out, 16u);
    appendU16(out, 3u);
    appendU16(out, 2u);
    appendU32(out, 44100u);
    appendU32(out, 352800u);
    appendU16(out, 8u);
    appendU16(out, 32u);

    appendId(out, "data");
    appendU32(out, 16u);

    const float values[4] {0.25f, -0.25f, 1.5f, -1.5f};
    for (float value : values) {
        std::uint32_t raw = 0;
        std::memcpy(&raw, &value, sizeof(raw));
        appendU32(out, raw);
    }

    return out;
std::vector<std::uint8_t> makeStereo24Extensible(std::uint32_t subFormat = 1u) {
    std::vector<std::uint8_t> out;
    appendId(out, "RIFF");
    appendU32(out, 72u);
    appendId(out, "WAVE");

    appendId(out, "fmt ");
    appendU32(out, 40u);
    appendU16(out, 0xFFFEu); // WAVE_FORMAT_EXTENSIBLE
    appendU16(out, 2u);
    appendU32(out, 48000u);
    appendU32(out, 288000u);
    appendU16(out, 6u);
    appendU16(out, 24u);
    appendU16(out, 22u); // cbSize
    appendU16(out, 24u); // valid bits
    appendU32(out, 0x3u); // FL|FR
    appendU32(out, subFormat);
    const std::uint8_t guidTail[12] {
        0x00,0x00,0x10,0x00,0x80,0x00,0x00,0xAA,0x00,0x38,0x9B,0x71
    };
    out.insert(out.end(), std::begin(guidTail), std::end(guidTail));

    appendId(out, "data");
    appendU32(out, 12u);
    const std::uint8_t samples[12] {
        0x00,0x00,0x40, 0x00,0x00,0xC0,
        0xFF,0xFF,0x7F, 0x00,0x00,0x80
    };
    out.insert(out.end(), std::begin(samples), std::end(samples));
    return out;
}


}

}

int main() {
    {
        const auto bytes = makeMono16();
        OwnedAudioSource decoded;
        const auto status = WavDecoder::decode(bytes.data(), bytes.size(), decoded);

        CHECK(status == WavDecodeStatus::Ok);
        CHECK(decoded.valid());
        CHECK(decoded.sampleRate == 48000u);
        CHECK(!decoded.stereo);
        CHECK(decoded.frames() == 3u);
        CHECK(std::fabs(decoded.left[0] + 1.0f) < 1.0e-6f);
        CHECK(std::fabs(decoded.left[1]) < 1.0e-6f);
        CHECK(decoded.left[2] > 0.999f);
    }

    {
        const auto bytes = makeStereoFloat();
        OwnedAudioSource decoded;
        const auto status = WavDecoder::decode(bytes.data(), bytes.size(), decoded);

        CHECK(status == WavDecodeStatus::Ok);
        CHECK(decoded.valid());
        CHECK(decoded.sampleRate == 44100u);
        CHECK(decoded.stereo);
        CHECK(decoded.frames() == 2u);
        CHECK(std::fabs(decoded.left[0] - 0.25f) < 1.0e-6f);
        CHECK(std::fabs(decoded.right[0] + 0.25f) < 1.0e-6f);
        CHECK(std::fabs(decoded.left[1] - 1.5f) < 1.0e-6f);
        CHECK(std::fabs(decoded.right[1] + 1.5f) < 1.0e-6f);
    }


    {
        const auto bytes = makeStereo24Extensible();
        OwnedAudioSource decoded;
        const auto status = WavDecoder::decode(bytes.data(), bytes.size(), decoded);

        CHECK(status == WavDecodeStatus::Ok);
        CHECK(decoded.valid());
        CHECK(decoded.sampleRate == 48000u);
        CHECK(decoded.stereo);
        CHECK(decoded.frames() == 2u);
        CHECK(std::fabs(decoded.left[0] - 0.5f) < 1.0e-5f);
        CHECK(std::fabs(decoded.right[0] + 0.5f) < 1.0e-5f);
        CHECK(decoded.left[1] > 0.999f);
        CHECK(std::fabs(decoded.right[1] + 1.0f) < 1.0e-5f);
    }

    {
        const auto bytes = makeStereo24Extensible(99u);
        OwnedAudioSource decoded;
        CHECK(WavDecoder::decode(bytes.data(), bytes.size(), decoded) ==
              WavDecodeStatus::UnsupportedFormat);
        CHECK(!decoded.valid());
    }

    {
        const std::uint8_t invalid[4] {0, 1, 2, 3};
        OwnedAudioSource decoded;
        CHECK(WavDecoder::decode(invalid, sizeof(invalid), decoded) ==
               WavDecodeStatus::InvalidContainer);
        CHECK(!decoded.valid());
    }

    return 0;
}
