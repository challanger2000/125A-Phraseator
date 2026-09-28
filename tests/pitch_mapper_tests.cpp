#include "pitch_mapper.h"
#include "test_common.h"

#include <cmath>
#include <limits>

using namespace phraseator;

int main() {
    CHECK(PitchMapper::quantizeMidi(61, 0, ScaleMode::Chromatic) == 61);

    // C major: C#4 (61) is equidistant to C4 and D4; lower note wins deterministically.
    CHECK(PitchMapper::quantizeMidi(61, 0, ScaleMode::Major) == 60);

    // D natural minor contains F (65).
    CHECK(PitchMapper::quantizeMidi(65, 2, ScaleMode::Minor) == 65);

    CHECK(std::fabs(PitchMapper::semitoneOffset(60.0f, 64) - 4.0f) < 1.0e-6f);
    CHECK(PitchMapper::semitoneOffset(-1.0f, 64) == 0.0f);

    // C#4 requested from C4 in C major must quantize down to C4.
    CHECK(std::fabs(PitchMapper::quantizedOffset(60.0f, 1.0f, 0, ScaleMode::Major) - 0.0f) < 1.0e-6f);

    // F4 remains valid in D natural minor.
    CHECK(std::fabs(PitchMapper::quantizedOffset(60.0f, 5.0f, 2, ScaleMode::Minor) - 5.0f) < 1.0e-6f);

    CHECK(PitchMapper::quantizedOffset(
        60.0f,
        std::numeric_limits<float>::quiet_NaN(),
        0,
        ScaleMode::Major) == 0.0f);

    // Invalid root detection disables quantization but preserves a finite
    // requested creative offset.
    CHECK(PitchMapper::quantizedOffset(
        -1.0f, 5.0f, 0, ScaleMode::Major) == 5.0f);

    // Processor order contract: MIDI transpose is added before final
    // Pitch-To-Key quantization. C4 + live +1 + MIDI +1 = D4, which is valid
    // in C major; quantizing before MIDI would incorrectly leave C#4.
    const float preQuantized = 1.0f + 1.0f;
    CHECK(std::fabs(
        PitchMapper::quantizedOffset(
            60.0f, preQuantized, 0, ScaleMode::Major) - 2.0f) < 1.0e-6f);

    return 0;
}
