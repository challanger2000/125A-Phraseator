#include "pitch_mapper.h"
#include "test_common.h"

#include <cmath>

using namespace phraseator;

int main() {
    CHECK(PitchMapper::quantizeMidi(61, 0, ScaleMode::Chromatic) == 61);

    // C major: C#4 (61) is equidistant to C4 and D4; lower note wins deterministically.
    CHECK(PitchMapper::quantizeMidi(61, 0, ScaleMode::Major) == 60);

    // D natural minor contains D and E, not Eb/D#. MIDI 63 is equidistant;
    // the documented deterministic tie-break selects the lower scale note D (62).
    CHECK(PitchMapper::quantizeMidi(63, 2, ScaleMode::Minor) == 62);

    CHECK(std::fabs(PitchMapper::semitoneOffset(60.0f, 64) - 4.0f) < 1.0e-6f);
    CHECK(PitchMapper::semitoneOffset(-1.0f, 64) == 0.0f);

    return 0;
}
