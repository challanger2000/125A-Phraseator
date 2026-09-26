#include "pitch_mapper.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace phraseator {

namespace {
constexpr std::array<int, 7> kMajor {0, 2, 4, 5, 7, 9, 11};
constexpr std::array<int, 7> kMinor {0, 2, 3, 5, 7, 8, 10};

bool contains(const std::array<int, 7>& scale, int pc) noexcept {
    for (const auto v : scale) {
        if (v == pc)
            return true;
    }
    return false;
}
}

int PitchMapper::quantizeMidi(int midiNote,
                              int rootPitchClass,
                              ScaleMode scale) noexcept {
    midiNote = std::clamp(midiNote, 0, 127);
    rootPitchClass = ((rootPitchClass % 12) + 12) % 12;

    if (scale == ScaleMode::Chromatic)
        return midiNote;

    const auto& intervals = (scale == ScaleMode::Major) ? kMajor : kMinor;

    int best = midiNote;
    int bestDistance = std::numeric_limits<int>::max();

    for (int candidate = 0; candidate <= 127; ++candidate) {
        const int pc = ((candidate - rootPitchClass) % 12 + 12) % 12;
        if (!contains(intervals, pc))
            continue;

        const int distance = std::abs(candidate - midiNote);
        if (distance < bestDistance ||
            (distance == bestDistance && candidate < best)) {
            best = candidate;
            bestDistance = distance;
        }
    }

    return best;
}

float PitchMapper::semitoneOffset(float detectedRootMidi,
                                  int targetMidi) noexcept {
    if (!std::isfinite(detectedRootMidi) || detectedRootMidi < 0.0f)
        return 0.0f;

    targetMidi = std::clamp(targetMidi, 0, 127);
    return static_cast<float>(targetMidi) - detectedRootMidi;
}

} // namespace phraseator
