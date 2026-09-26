#pragma once

#include "owned_audio_source.h"

#include <cstddef>
#include <cstdint>

namespace phraseator {

enum class WavDecodeStatus {
    Ok = 0,
    InvalidContainer,
    MissingFormat,
    MissingData,
    UnsupportedFormat,
    InvalidData
};

class WavDecoder {
public:
    static WavDecodeStatus decode(const std::uint8_t* bytes,
                                  std::size_t byteCount,
                                  OwnedAudioSource& output);
};

} // namespace phraseator
