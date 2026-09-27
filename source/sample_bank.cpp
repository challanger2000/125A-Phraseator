#include "sample_bank.h"

#include <algorithm>
#include <cmath>

namespace phraseator {

namespace {

void applyConservativeAutoLevel(OwnedAudioSource& audio) noexcept {
    if (!audio.valid())
        return;

    float peak = 0.0f;
    for (std::size_t i = 0; i < audio.left.size(); ++i) {
        peak = std::max(peak, std::fabs(audio.left[i]));
        if (audio.stereo)
            peak = std::max(peak, std::fabs(audio.right[i]));
    }
    if (!std::isfinite(peak) || peak < 1.0e-6f)
        return;

    const float activityThreshold = peak * 0.02f;
    double sumSquares = 0.0;
    std::size_t activeSamples = 0u;
    for (std::size_t i = 0; i < audio.left.size(); ++i) {
        const float l = audio.left[i];
        const float r = audio.stereo ? audio.right[i] : l;
        const float activity = std::max(std::fabs(l), std::fabs(r));
        if (activity < activityThreshold)
            continue;
        sumSquares += 0.5 * (static_cast<double>(l) * l + static_cast<double>(r) * r);
        ++activeSamples;
    }
    if (activeSamples == 0u)
        return;

    const double rms = std::sqrt(sumSquares / static_cast<double>(activeSamples));
    if (!std::isfinite(rms) || rms < 1.0e-8)
        return;

    constexpr double kTargetActiveRms = 0.18;
    constexpr double kPeakCeiling = 0.89;
    double gain = std::min(kTargetActiveRms / rms, kPeakCeiling / peak);
    gain = std::clamp(gain, 0.25, 4.0);

    for (auto& x : audio.left)
        x = static_cast<float>(x * gain);
    if (audio.stereo) {
        for (auto& x : audio.right)
            x = static_cast<float>(x * gain);
    }
}

} // namespace

SampleBank::SampleBank(const SampleBank& other)
: pool_(other.pool_),
  audio_(other.audio_) {
    for (std::size_t i = 0; i < kMaxSources; ++i)
        refreshView(i);
}

SampleBank& SampleBank::operator=(const SampleBank& other) {
    if (this == &other)
        return *this;

    pool_ = other.pool_;
    audio_ = other.audio_;
    for (std::size_t i = 0; i < kMaxSources; ++i)
        refreshView(i);
    return *this;
}

void SampleBank::clear() noexcept {
    pool_.clear();
    views_ = {};
    for (auto& source : audio_)
        source.clear();
}

bool SampleBank::clearSource(std::size_t sourceIndex) noexcept {
    if (sourceIndex >= kMaxSources)
        return false;

    audio_[sourceIndex].clear();
    views_[sourceIndex] = {};
    return pool_.clearSource(sourceIndex);
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

    applyConservativeAutoLevel(audio);
    applyConservativeAutoLevel(audio);
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
