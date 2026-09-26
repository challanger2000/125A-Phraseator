#include "sample_load_worker.h"

#include "sample_file_loader.h"
#include "slicer.h"

#include <utility>

namespace phraseator {

namespace {

struct PreparedSource {
    SampleLoadRequest request;
    OwnedAudioSource audio;
    SliceSet slices;
};

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
    if (request.sourceIndex >= kMaxSources || request.path.empty())
        return false;

    if (request.mode == SampleLoadMode::EqualSlices &&
        (request.equalDivisions == 0 || request.equalDivisions > kMaxSlicesPerSource)) {
        return false;
    }

    return true;
}

std::uint64_t SampleLoadWorker::requestLoad(SampleLoadRequest request) {
    std::vector<SampleLoadRequest> requests;
    requests.push_back(std::move(request));
    return requestBatch(std::move(requests));
}

std::uint64_t SampleLoadWorker::requestBatch(std::vector<SampleLoadRequest> requests) {
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

SampleLoadWorkerResult SampleLoadWorker::execute(const WorkItem& item) {
    SampleLoadWorkerResult result {item.id, SampleLoadWorkerStatus::InvalidRequest};

    std::vector<PreparedSource> prepared;
    prepared.reserve(item.requests.size());

    for (const auto& request : item.requests) {
        if (!validRequest(request)) {
            result.status = SampleLoadWorkerStatus::InvalidRequest;
            return result;
        }

        PreparedSource source;
        source.request = request;

        const auto loaded = SampleFileLoader::loadWav(request.path, source.audio);
        if (!loaded.ok()) {
            result.status = SampleLoadWorkerStatus::FileLoadFailed;
            return result;
        }

        if (request.mode == SampleLoadMode::EqualSlices) {
            source.slices = Slicer::equalDivisions(
                source.audio.frames(), request.equalDivisions);

            if (source.slices.count == 0) {
                result.status = SampleLoadWorkerStatus::SliceFailed;
                return result;
            }
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

        if (source.request.mode == SampleLoadMode::OneShot) {
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

        {
            std::lock_guard<std::mutex> lock(mutex_);
            results_.push_back(result);
        }
        cv_.notify_all();
    }
}

} // namespace phraseator
