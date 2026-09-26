#include "wav_decoder.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace phraseator {

namespace {

bool hasBytes(std::size_t offset, std::size_t count, std::size_t total) noexcept {
    return offset <= total && count <= (total - offset);
}

std::uint16_t readU16(const std::uint8_t* p) noexcept {
    return static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(p[0]) |
        (static_cast<std::uint16_t>(p[1]) << 8u));
}

std::uint32_t readU32(const std::uint8_t* p) noexcept {
    return static_cast<std::uint32_t>(
        static_cast<std::uint32_t>(p[0]) |
        (static_cast<std::uint32_t>(p[1]) << 8u) |
        (static_cast<std::uint32_t>(p[2]) << 16u) |
        (static_cast<std::uint32_t>(p[3]) << 24u));
}

bool idEquals(const std::uint8_t* p, const char id[5]) noexcept {
    return p[0] == static_cast<std::uint8_t>(id[0]) &&
           p[1] == static_cast<std::uint8_t>(id[1]) &&
           p[2] == static_cast<std::uint8_t>(id[2]) &&
           p[3] == static_cast<std::uint8_t>(id[3]);
}

float decodePcm16(const std::uint8_t* p) noexcept {
    const auto raw = static_cast<std::int16_t>(readU16(p));
    return static_cast<float>(raw) / 32768.0f;
}

float decodePcm24(const std::uint8_t* p) noexcept {
    std::int32_t raw =
        static_cast<std::int32_t>(p[0]) |
        (static_cast<std::int32_t>(p[1]) << 8) |
        (static_cast<std::int32_t>(p[2]) << 16);

    if ((raw & 0x00800000) != 0)
        raw |= static_cast<std::int32_t>(0xFF000000u);

    return static_cast<float>(raw) / 8388608.0f;
}

float decodePcm32(const std::uint8_t* p) noexcept {
    const auto raw = static_cast<std::int32_t>(readU32(p));
    return static_cast<float>(
        static_cast<double>(raw) / 2147483648.0);
}

float decodeFloat32(const std::uint8_t* p) noexcept {
    const std::uint32_t raw = readU32(p);
    float value = 0.0f;
    static_assert(sizeof(value) == sizeof(raw));
    std::memcpy(&value, &raw, sizeof(value));
    return std::isfinite(value) ? value : 0.0f;
}

} // namespace

WavDecodeStatus WavDecoder::decode(const std::uint8_t* bytes,
                                   std::size_t byteCount,
                                   OwnedAudioSource& output) {
    output.clear();

    if (bytes == nullptr || byteCount < 12 ||
        !idEquals(bytes, "RIFF") ||
        !idEquals(bytes + 8, "WAVE")) {
        return WavDecodeStatus::InvalidContainer;
    }

    bool haveFormat = false;
    bool haveData = false;

    std::uint16_t formatTag = 0;
    std::uint16_t channels = 0;
    std::uint16_t bitsPerSample = 0;
    std::uint16_t blockAlign = 0;
    std::uint32_t sampleRate = 0;

    const std::uint8_t* data = nullptr;
    std::size_t dataBytes = 0;

    std::size_t offset = 12;
    while (hasBytes(offset, 8, byteCount)) {
        const auto* header = bytes + offset;
        const std::uint32_t chunkSize = readU32(header + 4);
        const std::size_t payload = offset + 8;

        if (!hasBytes(payload, chunkSize, byteCount))
            return WavDecodeStatus::InvalidContainer;

        if (idEquals(header, "fmt ")) {
            if (chunkSize < 16)
                return WavDecodeStatus::InvalidData;

            const auto* fmt = bytes + payload;
            formatTag = readU16(fmt + 0);
            channels = readU16(fmt + 2);
            sampleRate = readU32(fmt + 4);
            blockAlign = readU16(fmt + 12);
            bitsPerSample = readU16(fmt + 14);
            haveFormat = true;
        } else if (idEquals(header, "data")) {
            data = bytes + payload;
            dataBytes = chunkSize;
            haveData = true;
        }

        const std::size_t padded = static_cast<std::size_t>(chunkSize) + (chunkSize & 1u);
        if (payload > std::numeric_limits<std::size_t>::max() - padded)
            return WavDecodeStatus::InvalidContainer;

        offset = payload + padded;
    }

    if (!haveFormat)
        return WavDecodeStatus::MissingFormat;
    if (!haveData)
        return WavDecodeStatus::MissingData;

    if ((formatTag != 1u && formatTag != 3u) ||
        (channels != 1u && channels != 2u) ||
        sampleRate == 0u) {
        return WavDecodeStatus::UnsupportedFormat;
    }

    std::size_t bytesPerSample = 0;
    if (formatTag == 1u) {
        if (bitsPerSample == 16u) bytesPerSample = 2;
        else if (bitsPerSample == 24u) bytesPerSample = 3;
        else if (bitsPerSample == 32u) bytesPerSample = 4;
        else return WavDecodeStatus::UnsupportedFormat;
    } else {
        if (bitsPerSample != 32u)
            return WavDecodeStatus::UnsupportedFormat;
        bytesPerSample = 4;
    }

    const std::size_t expectedBlockAlign =
        bytesPerSample * static_cast<std::size_t>(channels);

    if (blockAlign != expectedBlockAlign ||
        dataBytes == 0 ||
        (dataBytes % expectedBlockAlign) != 0) {
        return WavDecodeStatus::InvalidData;
    }

    const std::size_t frameCount = dataBytes / expectedBlockAlign;
    if (frameCount == 0 ||
        frameCount > static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max())) {
        return WavDecodeStatus::InvalidData;
    }

    try {
        output.left.resize(frameCount);
        if (channels == 2u)
            output.right.resize(frameCount);
    } catch (...) {
        output.clear();
        return WavDecodeStatus::InvalidData;
    }

    auto decodeSample = [&](const std::uint8_t* p) noexcept {
        if (formatTag == 3u)
            return decodeFloat32(p);

        switch (bitsPerSample) {
            case 16u: return decodePcm16(p);
            case 24u: return decodePcm24(p);
            case 32u: return decodePcm32(p);
            default: return 0.0f;
        }
    };

    for (std::size_t frame = 0; frame < frameCount; ++frame) {
        const auto* frameData = data + frame * expectedBlockAlign;
        output.left[frame] = decodeSample(frameData);

        if (channels == 2u)
            output.right[frame] = decodeSample(frameData + bytesPerSample);
    }

    output.sampleRate = sampleRate;
    output.stereo = channels == 2u;

    return output.valid()
        ? WavDecodeStatus::Ok
        : WavDecodeStatus::InvalidData;
}

} // namespace phraseator
