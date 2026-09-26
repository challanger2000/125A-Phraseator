#include "pitch_detector.h"
#include "test_common.h"

#include <cmath>
#include <cstdint>

using namespace phraseator;

int main() {
    {
        OwnedAudioSource sine;
        sine.sampleRate = 48000u;
        sine.stereo = false;

        constexpr double frequency = 220.0;
        constexpr std::size_t frames = 12000u;
        sine.left.resize(frames);

        for (std::size_t i = 0; i < frames; ++i) {
            sine.left[i] = static_cast<float>(
                0.7 * std::sin(
                    2.0 * 3.14159265358979323846 * frequency *
                    static_cast<double>(i) /
                    static_cast<double>(sine.sampleRate)));
        }

        const auto result = PitchDetector::analyze(sine);
        CHECK(result.tonal);
        CHECK(result.confidence > 0.90f);
        CHECK(std::fabs(result.frequencyHz - 220.0f) < 1.0f);
        CHECK(std::fabs(result.midiNote - 57.0f) < 0.10f);
    }

    {
        OwnedAudioSource changing;
        changing.sampleRate = 48000u;
        changing.stereo = false;
        constexpr std::size_t frames = 24000u;
        changing.left.resize(frames);

        for (std::size_t i = 0; i < frames; ++i) {
            const double f = i < frames / 2u ? 220.0 : 329.6275569;
            changing.left[i] = static_cast<float>(
                0.7 * std::sin(
                    2.0 * 3.14159265358979323846 * f *
                    static_cast<double>(i) /
                    static_cast<double>(changing.sampleRate)));
        }

        const auto result = PitchDetector::analyze(changing);
        CHECK(!result.tonal);
    }

    {
        OwnedAudioSource silence;
        silence.sampleRate = 48000u;
        silence.left.assign(4096u, 0.0f);

        const auto result = PitchDetector::analyze(silence);
        CHECK(!result.tonal);
    }

    return 0;
}
