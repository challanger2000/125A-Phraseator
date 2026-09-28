#pragma once

#include <cstdint>

namespace phraseator {

enum class ParameterId : std::uint32_t {
    Density = 1000,
    Variation = 1001,
    Repeat = 1002,
    Pitch = 1003,
    Pan = 1004,
    Groove = 1005,
    Velocity = 1006,
    OctaveMode = 1007,

    KeyRoot = 1100,
    ScaleMode = 1101,
    PitchToKey = 1102,

    DelayAmount = 1200,
    DelayDivision = 1201,
    // Legacy serialized IDs retained for state compatibility. V1 does not
    // register or process Reverb/Drive controls.
    ReverbAmount = 1202,
    DriveAmount = 1203,
    FilterAmount = 1204,
    FilterMode = 1205,

    GenerateTrigger = 1300,
    VariateTrigger = 1301,
    LockPattern = 1302,
    RestartMode = 1303
};

struct ParameterDefaults {
    static constexpr float density = 0.50f;
    static constexpr float variation = 0.25f;
    static constexpr float repeat = 0.0f;
    static constexpr float pitch = 0.0f;
    static constexpr float pan = 0.0f;
    static constexpr float groove = 0.0f;
    static constexpr float velocity = 0.50f;
    static constexpr std::int32_t octaveMode = 0; // OFF

    static constexpr float delayAmount = 0.0f;
    static constexpr std::int32_t delayDivision = 2; // 1/8 (0=OFF)
    static constexpr std::int32_t filterMode = 1; // HP / low-cut
    static constexpr float reverbAmount = 0.0f;
    static constexpr float driveAmount = 0.0f;
    static constexpr float filterAmount = 0.0f;
};

} // namespace phraseator
