#pragma once

#include "owned_audio_source.h"

namespace phraseator {

struct PitchDetectionResult {
    bool tonal {false};
    float midiNote {-1.0f};
    float frequencyHz {0.0f};
    float confidence {0.0f};
    int stableWindows {0};
};

class PitchDetector {
public:
    // Offline/non-realtime source analysis.
    static PitchDetectionResult analyze(const OwnedAudioSource& source);
};

} // namespace phraseator
