#include "sample_bank.h"

namespace phraseator {

void SampleBank::clear() noexcept {
    pool_.clear();
    views_ = {};
    for (auto& source : audio_)
        source.clear();
}

void SampleBank::refreshView(std::size_t sourceIndex) noexcept {
    if (sourceIndex >= kMaxSources)
        return;
    views_[sourceIndex] = audio_[sourceIndex].view();
}

bool SampleBank::setOneShot(std::size_t sourceIndex,
                            std::uint32_t sourceId,
                            OwnedAudioSource&& audio,
                            bool tonal,
                            float detectedRootMidi) noexcept {
    if (sourceIndex >= kMaxSources || !audio.valid())
        return false;

    const auto frames = audio.frames();
    const auto rate = static_cast<double>(audio.sampleRate);
    const auto stereo = audio.stereo;

    audio_[sourceIndex] = std::move(audio);
    refreshView(sourceIndex);

    if (!pool_.setOneShot(sourceIndex, sourceId, frames, rate, stereo, tonal, detectedRootMidi)) {
        audio_[sourceIndex].clear();
        views_[sourceIndex] = {};
        return false;
    }

    return true;
}

bool SampleBank::setLoop(std::size_t sourceIndex,
                         std::uint32_t sourceId,
                         OwnedAudioSource&& audio,
                         const SliceRegion* slices,
                         std::size_t sliceCount,
                         bool tonal,
                         float detectedRootMidi) noexcept {
    if (sourceIndex >= kMaxSources || !audio.valid())
        return false;

    const auto frames = audio.frames();
    const auto rate = static_cast<double>(audio.sampleRate);
    const auto stereo = audio.stereo;

    audio_[sourceIndex] = std::move(audio);
    refreshView(sourceIndex);

    if (!pool_.setLoopSlices(sourceIndex, sourceId, frames, rate, stereo,
                             slices, sliceCount, tonal, detectedRootMidi)) {
        audio_[sourceIndex].clear();
        views_[sourceIndex] = {};
        return false;
    }

    return true;
}

int SampleBankExchange::beginWrite() noexcept {
    if (pendingIndex_.load(std::memory_order_acquire) != -1)
        return -1;

    int expected = -1;
    const int inactive = 1 - activeIndex_.load(std::memory_order_acquire);

    if (!writerIndex_.compare_exchange_strong(
            expected, inactive,
            std::memory_order_acq_rel,
            std::memory_order_acquire)) {
        return -1;
    }

    return inactive;
}

SampleBank* SampleBankExchange::writableBank(int index) noexcept {
    if (index < 0 || index > 1)
        return nullptr;

    if (writerIndex_.load(std::memory_order_acquire) != index)
        return nullptr;

    return &banks_[static_cast<std::size_t>(index)];
}

bool SampleBankExchange::commitWrite(int index) noexcept {
    if (index < 0 || index > 1)
        return false;

    if (writerIndex_.load(std::memory_order_acquire) != index)
        return false;

    int expectedPending = -1;
    if (!pendingIndex_.compare_exchange_strong(
            expectedPending, index,
            std::memory_order_release,
            std::memory_order_acquire)) {
        return false;
    }

    writerIndex_.store(-1, std::memory_order_release);
    return true;
}

void SampleBankExchange::cancelWrite(int index) noexcept {
    int expected = index;
    writerIndex_.compare_exchange_strong(
        expected, -1,
        std::memory_order_release,
        std::memory_order_relaxed);
}

bool SampleBankExchange::consumePending() noexcept {
    const int pending = pendingIndex_.load(std::memory_order_acquire);
    if (pending < 0)
        return false;

    // Publish the new active bank before making the writer gate available.
    // This prevents a non-realtime writer from observing pending==-1 while
    // activeIndex still refers to the previous bank.
    activeIndex_.store(pending, std::memory_order_release);

    int expected = pending;
    return pendingIndex_.compare_exchange_strong(
        expected, -1,
        std::memory_order_acq_rel,
        std::memory_order_acquire);
}

} // namespace phraseator
