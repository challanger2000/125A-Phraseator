#pragma once

#include <array>
#include <cstdint>

namespace phraseator {

enum class ScaleMode : std::uint8_t {
    Chromatic = 0,
    Major,
    Minor
};

class PitchMapper {
public:
    static int quantizeMidi(int midiNote,
                            int rootPitchClass,
                            ScaleMode scale) noexcept;

    static float semitoneOffset(float detectedRootMidi,
                                int targetMidi) noexcept;

    static float quantizedOffset(float detectedRootMidi,
                                 float requestedOffsetSemitones,
                                 int rootPitchClass,
                                 ScaleMode scale) noexcept;
};

} // namespace phraseator
