#include "sample_file_loader.h"

#include <fstream>
#include <limits>
#include <vector>

namespace phraseator {

SampleFileLoadResult SampleFileLoader::loadWav(
    const std::filesystem::path& path,
    OwnedAudioSource& output) {

    output.clear();

    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file)
        return {SampleFileLoadStatus::OpenFailed, WavDecodeStatus::InvalidContainer};

    const auto end = file.tellg();
    if (end <= std::streampos(0))
        return {SampleFileLoadStatus::SizeInvalid, WavDecodeStatus::InvalidContainer};

    const auto size64 = static_cast<std::uint64_t>(end);
    if (size64 > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max()) ||
        size64 > static_cast<std::uint64_t>(std::numeric_limits<std::streamsize>::max())) {
        return {SampleFileLoadStatus::SizeInvalid, WavDecodeStatus::InvalidContainer};
    }

    const auto size = static_cast<std::size_t>(size64);

    std::vector<std::uint8_t> bytes;
    try {
        bytes.resize(size);
    } catch (...) {
        return {SampleFileLoadStatus::SizeInvalid, WavDecodeStatus::InvalidContainer};
    }

    file.seekg(0, std::ios::beg);
    if (!file.read(reinterpret_cast<char*>(bytes.data()),
                   static_cast<std::streamsize>(bytes.size()))) {
        return {SampleFileLoadStatus::ReadFailed, WavDecodeStatus::InvalidContainer};
    }

    const auto wavStatus = WavDecoder::decode(bytes.data(), bytes.size(), output);
    if (wavStatus != WavDecodeStatus::Ok) {
        output.clear();
        return {SampleFileLoadStatus::DecodeFailed, wavStatus};
    }

    return {SampleFileLoadStatus::Ok, wavStatus};
}

} // namespace phraseator
