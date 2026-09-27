#pragma once

#include "phrase_engine.h"
#include "source_pool.h"

#include <array>
#include <cstdint>

namespace phraseator {

struct SourceState {
    bool occupied {false};
    std::uint32_t sourceId {0};
    std::uint16_t sliceCount {0};
    bool tonal {false};
    bool muted {false};
    float detectedRootMidi {-1.0f};
};

struct ProjectState {
    static constexpr std::uint32_t kCurrentVersion = 9u;

    std::uint32_t version {kCurrentVersion};
    std::uint32_t randomSeed {0x125A0001u};

    float density {0.50f};
    float variation {0.25f};
    float repeat {0.0f};
    float pitch {0.0f};
    float pan {0.0f};
    float groove {0.0f};
    float velocity {0.50f};
    std::int32_t octaveMode {0};

    std::int32_t keyRoot {0};
    std::int32_t scaleMode {0};
    bool pitchToKey {false};
    bool lockPattern {false};
    bool restartOnNote {true};

    float delayAmount {0.0f};
    std::int32_t delayDivision {2};
    float reverbAmount {0.0f};
    float driveAmount {0.0f};
    float filterAmount {0.0f};
    std::int32_t filterMode {1};

    Pattern pattern {};
    std::array<SourceState, kMaxSources> sources {};
};

} // namespace phraseator
