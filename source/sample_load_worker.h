#pragma once

#include "sample_bank.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <mutex>
#include <thread>

namespace phraseator {

enum class SampleLoadMode : std::uint8_t {
    OneShot = 0,
    EqualSlices
};

enum class SampleLoadWorkerStatus : std::uint8_t {
    Ok = 0,
    InvalidRequest,
    FileLoadFailed,
    SliceFailed,
    PublishFailed,
    Stopped
};

struct SampleLoadRequest {
    std::uint64_t requestId {0};
    std::size_t sourceIndex {0};
    std::uint32_t sourceId {0};
    std::filesystem::path path;
    SampleLoadMode mode {SampleLoadMode::OneShot};
    std::size_t equalDivisions {0};
    bool tonal {false};
    float detectedRootMidi {-1.0f};
};

struct SampleLoadWorkerResult {
    std::uint64_t requestId {0};
    SampleLoadWorkerStatus status {SampleLoadWorkerStatus::InvalidRequest};

    bool ok() const noexcept {
        return status == SampleLoadWorkerStatus::Ok;
    }
};

class SampleLoadWorker {
public:
    explicit SampleLoadWorker(SampleBankExchange& exchange);
    ~SampleLoadWorker();

    SampleLoadWorker(const SampleLoadWorker&) = delete;
    SampleLoadWorker& operator=(const SampleLoadWorker&) = delete;

    std::uint64_t requestLoad(SampleLoadRequest request);

    bool waitForResult(std::uint64_t requestId,
                       SampleLoadWorkerResult& result,
                       std::chrono::milliseconds timeout);

private:
    void run();
    SampleLoadWorkerResult execute(const SampleLoadRequest& request);
    bool acquireWritableBank(int& index, SampleBank*& bank);

    SampleBankExchange& exchange_;
    std::thread thread_;

    std::mutex mutex_;
    std::condition_variable cv_;
    std::deque<SampleLoadRequest> requests_;
    std::deque<SampleLoadWorkerResult> results_;
    std::atomic<std::uint64_t> nextRequestId_ {1u};
    bool stopping_ {false};
};

} // namespace phraseator
