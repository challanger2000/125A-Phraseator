#include "pitch_mapper.h"

#include <cassert>
#include <cmath>

using namespace phraseator;

int main() {
    assert(PitchMapper::quantizeMidi(61, 0, ScaleMode::Chromatic) == 61);

    // C major: C#4 (61) is equidistant to C4 and D4; lower note wins deterministically.
    assert(PitchMapper::quantizeMidi(61, 0, ScaleMode::Major) == 60);

    // D natural minor contains Eb (3).
    assert(PitchMapper::quantizeMidi(63, 2, ScaleMode::Minor) == 63);

    assert(std::fabs(PitchMapper::semitoneOffset(60.0f, 64) - 4.0f) < 1.0e-6f);
    assert(PitchMapper::semitoneOffset(-1.0f, 64) == 0.0f);

    return 0;
}
