#pragma once

#include "owned_audio_source.h"
#include "source_pool.h"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <utility>

namespace phraseator {

class SampleBank {
public:
    void clear() noexcept;
    bool clearSource(std::size_t sourceIndex) noexcept;

    bool setOneShot(std::size_t sourceIndex,
                    std::uint32_t sourceId,
                    OwnedAudioSource&& audio,
                    bool tonal = false,
                    float detectedRootMidi = -1.0f) noexcept;

    bool setLoop(std::size_t sourceIndex,
                 std::uint32_t sourceId,
                 OwnedAudioSource&& audio,
                 const SliceRegion* slices,
                 std::size_t sliceCount,
                 bool tonal = false,
                 float detectedRootMidi = -1.0f) noexcept;

    const SourcePool& sourcePool() const noexcept { return pool_; }
    const std::array<AudioBufferView, kMaxSources>& buffers() const noexcept { return views_; }

private:
    void refreshView(std::size_t sourceIndex) noexcept;

    SourcePool pool_ {};
    std::array<OwnedAudioSource, kMaxSources> audio_ {};
    std::array<AudioBufferView, kMaxSources> views_ {};
};

class SampleBankExchange {
public:
    int beginWrite() noexcept;
    SampleBank* writableBank(int index) noexcept;
    bool commitWrite(int index) noexcept;
    void cancelWrite(int index) noexcept;

    bool consumePending() noexcept;

    const SampleBank& activeBank() const noexcept {
        return banks_[static_cast<std::size_t>(activeIndex_.load(std::memory_order_acquire))];
    }

    int activeIndex() const noexcept {
        return activeIndex_.load(std::memory_order_acquire);
    }

private:
    std::array<SampleBank, 2> banks_ {};
    std::atomic<int> activeIndex_ {0};
    std::atomic<int> pendingIndex_ {-1};
    std::atomic<int> writerIndex_ {-1};
};

} // namespace phraseator
