#include "sample_load_worker.h"

#include "sample_file_loader.h"
#include "pitch_detector.h"
#include "slicer.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace phraseator {

namespace {

struct PreparedSource {
    SampleLoadRequest request;
    OwnedAudioSource audio;
    SliceSet slices;
};

double monoSampleAt(const OwnedAudioSource& audio, std::size_t frame) noexcept {
    const double left = static_cast<double>(audio.left[frame]);
    const double right = audio.stereo && frame < audio.right.size()
        ? static_cast<double>(audio.right[frame])
        : left;
    return 0.5 * (left + right);
}

double regionRms(const OwnedAudioSource& audio,
                 std::size_t begin,
                 std::size_t end) noexcept {
    if (begin >= end || end > audio.frames())
        return 0.0;

    double sum = 0.0;
    for (std::size_t i = begin; i < end; ++i) {
        const double x = monoSampleAt(audio, i);
        sum += x * x;
    }

    return std::sqrt(sum / static_cast<double>(end - begin));
}

bool autoLooksLikeLoop(const OwnedAudioSource& audio,
                       const SliceSet& slices) noexcept {
    const auto frames = static_cast<std::size_t>(audio.frames());
    if (!audio.valid() || frames < 1024u || slices.count < 3u)
        return false;

    // A real rhythmic loop should contain useful event structure across the
    // file, not only a strong onset followed by reverb reflections.
    bool middleBoundary = false;
    bool lateBoundary = false;
    for (std::size_t i = 0; i + 1u < slices.count; ++i) {
        const double position =
            static_cast<double>(slices.regions[i].endFrame) /
            static_cast<double>(frames);
        middleBoundary = middleBoundary || position >= 0.30;
        lateBoundary = lateBoundary || position >= 0.60;
    }

    if (!middleBoundary || !lateBoundary)
        return false;

    const std::size_t quarter = std::max<std::size_t>(1u, frames / 4u);
    const double earlyRms = regionRms(audio, 0u, quarter);
    const double lateRms = regionRms(audio, frames - quarter, frames);

    if (!std::isfinite(earlyRms) || !std::isfinite(lateRms) ||
        earlyRms < 1.0e-6) {
        return false;
    }

    // EMPIRICALLY TUNED classification guard:
    // pronounced decay across the file is one-shot behaviour even if a
    // reverb tail contains additional flux peaks.
    if (lateRms < earlyRms * 0.28)
        return false;

    return true;
}

} // namespace

SampleLoadWorker::SampleLoadWorker(SampleBankExchange& exchange)
: exchange_(exchange),
  thread_([this] { run(); }) {}

SampleLoadWorker::~SampleLoadWorker() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stopping_ = true;
    }
    cv_.notify_all();

    if (thread_.joinable())
        thread_.join();
}

bool SampleLoadWorker::validRequest(const SampleLoadRequest& request) noexcept {
    if (request.sourceIndex >= kMaxSources)
        return false;

    if (request.mode == SampleLoadMode::Clear)
        return true;

    if (request.path.empty())
        return false;

    if (request.mode == SampleLoadMode::EqualSlices ||
        request.mode == SampleLoadMode::Auto) {
        if (request.equalDivisions == 0 ||
            request.equalDivisions > kMaxSlicesPerSource) {
            return false;
        }

        if (request.mode == SampleLoadMode::EqualSlices &&
            request.useStoredSlices) {
            if (request.resolvedSliceCount == 0 ||
                request.resolvedSliceCount > kMaxSlicesPerSource) {
                return false;
            }

            std::uint32_t previousEnd = 0u;
            for (std::size_t i = 0; i < request.resolvedSliceCount; ++i) {
                const auto region = request.resolvedSlices[i];
                if (!region.valid() ||
                    (i > 0u && region.startFrame < previousEnd)) {
                    return false;
                }
                previousEnd = region.endFrame;
            }
        }
    }

    return true;
}

std::uint64_t SampleLoadWorker::requestLoad(SampleLoadRequest request,
                                            bool retainResult,
                                            CompletionCallback completion) {
    std::vector<SampleLoadRequest> requests;
    requests.push_back(std::move(request));
    return requestBatch(
        std::move(requests), retainResult, std::move(completion));
}

std::uint64_t SampleLoadWorker::requestBatch(std::vector<SampleLoadRequest> requests,
                                             bool retainResult,
                                             CompletionCallback completion) {
    if (requests.empty())
        return 0u;

    for (const auto& request : requests) {
        if (!validRequest(request))
            return 0u;
    }

    const auto requestId = nextRequestId_.fetch_add(1u, std::memory_order_relaxed);

    WorkItem item;
    item.id = requestId;
    item.requests = std::move(requests);
    item.retainResult = retainResult;
    item.completion = std::move(completion);

    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (stopping_)
            return 0u;
        requests_.push_back(std::move(item));
    }

    cv_.notify_one();
    return requestId;
}

bool SampleLoadWorker::waitForResult(std::uint64_t requestId,
                                     SampleLoadWorkerResult& result,
                                     std::chrono::milliseconds timeout) {
    if (requestId == 0u)
        return false;

    std::unique_lock<std::mutex> lock(mutex_);
    const auto ready = [this, requestId] {
        if (stopping_)
            return true;
        for (const auto& r : results_) {
            if (r.requestId == requestId)
                return true;
        }
        return false;
    };

    if (!cv_.wait_for(lock, timeout, ready))
        return false;

    for (auto it = results_.begin(); it != results_.end(); ++it) {
        if (it->requestId == requestId) {
            result = *it;
            results_.erase(it);
            return true;
        }
    }

    return false;
}

bool SampleLoadWorker::acquireWritableBank(int& index, SampleBank*& bank) {
    for (;;) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (stopping_)
                return false;
        }

        index = exchange_.beginWrite();
        if (index >= 0) {
            bank = exchange_.writableBank(index);
            return bank != nullptr;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

SampleLoadWorkerResult SampleLoadWorker::execute(WorkItem& item) {
    SampleLoadWorkerResult result {item.id, SampleLoadWorkerStatus::InvalidRequest};

    std::vector<PreparedSource> prepared;
    prepared.reserve(item.requests.size());

    for (auto& request : item.requests) {
        if (!validRequest(request)) {
            result.status = SampleLoadWorkerStatus::InvalidRequest;
            return result;
        }

        PreparedSource source;
        source.request = request;

        if (request.mode == SampleLoadMode::Clear) {
            prepared.push_back(std::move(source));
            continue;
        }

        const auto loaded = SampleFileLoader::loadWav(request.path, source.audio);
        if (!loaded.ok()) {
            result.status = SampleLoadWorkerStatus::FileLoadFailed;
            return result;
        }

        if (!source.request.tonal &&
            source.request.detectedRootMidi < 0.0f) {
            const auto pitch = PitchDetector::analyze(source.audio);
            if (pitch.tonal) {
                source.request.tonal = true;
                source.request.detectedRootMidi = pitch.midiNote;
                request.tonal = true;
                request.detectedRootMidi = pitch.midiNote;
            }
        }

        if (request.mode == SampleLoadMode::Auto) {
            source.slices = Slicer::transientDivisions(
                source.audio.view(),
                static_cast<double>(source.audio.sampleRate),
                request.equalDivisions);

            // AUTO stays conservative. Multiple transient divisions alone
            // are not enough: a pluck with reverb can contain several flux
            // peaks while still being a decaying one-shot.
            if (autoLooksLikeLoop(source.audio, source.slices)) {
                request.mode = SampleLoadMode::EqualSlices;
                source.request.mode = SampleLoadMode::EqualSlices;
                request.preferTransient = true;
                source.request.preferTransient = true;
            } else {
                source.slices = {};
                request.mode = SampleLoadMode::OneShot;
                source.request.mode = SampleLoadMode::OneShot;
            }
        }

        if (request.mode == SampleLoadMode::EqualSlices) {
            if (request.useStoredSlices) {
                for (std::size_t i = 0; i < request.resolvedSliceCount; ++i) {
                    const auto region = request.resolvedSlices[i];
                    if (!region.valid() ||
                        region.endFrame > source.audio.frames() ||
                        (i > 0u &&
                         region.startFrame < source.slices.regions[i - 1u].endFrame)) {
                        result.status = SampleLoadWorkerStatus::SliceFailed;
                        return result;
                    }

                    source.slices.regions[i] = region;
                }
                source.slices.count = request.resolvedSliceCount;
            } else if (request.preferTransient) {
                source.slices = Slicer::transientDivisions(
                    source.audio.view(),
                    static_cast<double>(source.audio.sampleRate),
                    request.equalDivisions);
            }

            if (source.slices.count == 0) {
                source.slices = Slicer::equalDivisions(
                    source.audio.frames(), request.equalDivisions);
            }

            if (source.slices.count == 0) {
                result.status = SampleLoadWorkerStatus::SliceFailed;
                return result;
            }

            request.resolvedSliceCount =
                static_cast<std::uint16_t>(source.slices.count);
            request.resolvedSlices = source.slices.regions;
            source.request.resolvedSliceCount = request.resolvedSliceCount;
            source.request.resolvedSlices = request.resolvedSlices;
        } else {
            request.resolvedSliceCount = 1u;
            request.resolvedSlices[0] = {0u, source.audio.frames()};
            source.request.resolvedSliceCount = 1u;
            source.request.resolvedSlices = request.resolvedSlices;
        }

        prepared.push_back(std::move(source));
    }

    int bankIndex = -1;
    SampleBank* bank = nullptr;
    if (!acquireWritableBank(bankIndex, bank)) {
        result.status = SampleLoadWorkerStatus::Stopped;
        return result;
    }

    *bank = exchange_.activeBank();

    for (auto& source : prepared) {
        bool staged = false;

        if (source.request.mode == SampleLoadMode::Clear) {
            staged = bank->clearSource(source.request.sourceIndex);
        } else if (source.request.mode == SampleLoadMode::OneShot) {
            staged = bank->setOneShot(
                source.request.sourceIndex,
                source.request.sourceId,
                std::move(source.audio),
                source.request.tonal,
                source.request.detectedRootMidi);
        } else {
            staged = bank->setLoop(
                source.request.sourceIndex,
                source.request.sourceId,
                std::move(source.audio),
                source.slices.regions.data(),
                source.slices.count,
                source.request.tonal,
                source.request.detectedRootMidi);
        }

        if (!staged) {
            exchange_.cancelWrite(bankIndex);
            result.status = SampleLoadWorkerStatus::SliceFailed;
            return result;
        }
    }

    if (!exchange_.commitWrite(bankIndex)) {
        exchange_.cancelWrite(bankIndex);
        result.status = SampleLoadWorkerStatus::PublishFailed;
        return result;
    }

    result.status = SampleLoadWorkerStatus::Ok;
    return result;
}

void SampleLoadWorker::run() {
    for (;;) {
        WorkItem item;

        {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait(lock, [this] {
                return stopping_ || !requests_.empty();
            });

            if (stopping_ && requests_.empty())
                break;

            item = std::move(requests_.front());
            requests_.pop_front();
        }

        const auto result = execute(item);

        if (item.completion) {
            try {
                item.completion(result, item.requests);
            } catch (...) {
                // Completion callbacks are non-realtime notifications.
                // A callback failure must never terminate the loader thread.
            }
        }

        if (item.retainResult) {
            {
                std::lock_guard<std::mutex> lock(mutex_);
                results_.push_back(result);
            }
            cv_.notify_all();
        }
    }
}

} // namespace phraseator
