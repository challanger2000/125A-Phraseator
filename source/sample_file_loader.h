#pragma once

#include "owned_audio_source.h"
#include "wav_decoder.h"

#include <filesystem>

namespace phraseator {

enum class SampleFileLoadStatus {
    Ok = 0,
    OpenFailed,
    SizeInvalid,
    ReadFailed,
    DecodeFailed
};

struct SampleFileLoadResult {
    SampleFileLoadStatus status {SampleFileLoadStatus::OpenFailed};
    WavDecodeStatus wavStatus {WavDecodeStatus::InvalidContainer};

    bool ok() const noexcept {
        return status == SampleFileLoadStatus::Ok;
    }
};

class SampleFileLoader {
public:
    static SampleFileLoadResult loadWav(const std::filesystem::path& path,
                                        OwnedAudioSource& output);
};

} // namespace phraseator
