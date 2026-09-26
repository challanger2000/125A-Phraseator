#include "pitch_detector.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <vector>

namespace phraseator {
namespace {

struct WindowPitch {
    bool valid {false};
    double frequencyHz {0.0};
    double midiNote {-1.0};
    double confidence {0.0};
};

double monoAt(const OwnedAudioSource& source, std::size_t index) noexcept {
    const double l = static_cast<double>(source.left[index]);
    if (!source.stereo)
        return l;
    return 0.5 * (l + static_cast<double>(source.right[index]));
}

WindowPitch analyzeWindow(const OwnedAudioSource& source,
                          std::size_t start,
                          std::size_t length) {
    WindowPitch result {};

    if (!source.valid() || length < 256 || start + length > source.left.size())
        return result;

    const double sampleRate = static_cast<double>(source.sampleRate);
    constexpr double kMinFrequency = 55.0;
    constexpr double kMaxFrequency = 1760.0;
    constexpr double kYinThreshold = 0.15; // EMPIRICALLY TUNED product threshold.

    const std::size_t minTau = std::max<std::size_t>(
        2u, static_cast<std::size_t>(std::floor(sampleRate / kMaxFrequency)));
    const std::size_t maxTau = std::min<std::size_t>(
        length / 2u,
        static_cast<std::size_t>(std::ceil(sampleRate / kMinFrequency)));

    if (maxTau <= minTau + 2u)
        return result;

    const std::size_t compareLength = length - maxTau;
    if (compareLength < 128u)
        return result;

    double energy = 0.0;
    for (std::size_t i = 0; i < length; ++i) {
        const double x = monoAt(source, start + i);
        energy += x * x;
    }
    const double rms = std::sqrt(energy / static_cast<double>(length));
    if (!std::isfinite(rms) || rms < 1.0e-4)
        return result;

    std::vector<double> difference(maxTau + 1u, 0.0);
    std::vector<double> cmnd(maxTau + 1u, 1.0);

    for (std::size_t tau = 1; tau <= maxTau; ++tau) {
        double sum = 0.0;
        for (std::size_t j = 0; j < compareLength; ++j) {
            const double delta =
                monoAt(source, start + j) -
                monoAt(source, start + j + tau);
            sum += delta * delta;
        }
        difference[tau] = sum;
    }

    double running = 0.0;
    for (std::size_t tau = 1; tau <= maxTau; ++tau) {
        running += difference[tau];
        cmnd[tau] = running > 0.0
            ? difference[tau] * static_cast<double>(tau) / running
            : 1.0;
    }

    std::size_t candidate = 0u;
    for (std::size_t tau = minTau; tau + 1u <= maxTau; ++tau) {
        if (cmnd[tau] < kYinThreshold &&
            cmnd[tau] <= cmnd[tau - 1u] &&
            cmnd[tau] <= cmnd[tau + 1u]) {
            candidate = tau;
            break;
        }
    }

    if (candidate == 0u)
        return result;

    double refinedTau = static_cast<double>(candidate);
    if (candidate > 0u && candidate + 1u <= maxTau) {
        const double y0 = cmnd[candidate - 1u];
        const double y1 = cmnd[candidate];
        const double y2 = cmnd[candidate + 1u];
        const double denom = y0 - 2.0 * y1 + y2;
        if (std::fabs(denom) > 1.0e-12) {
            const double offset = 0.5 * (y0 - y2) / denom;
            refinedTau += std::clamp(offset, -1.0, 1.0);
        }
    }

    if (!(refinedTau > 0.0))
        return result;

    const double frequency = sampleRate / refinedTau;
    if (!std::isfinite(frequency) ||
        frequency < kMinFrequency || frequency > kMaxFrequency)
        return result;

    const double confidence = std::clamp(1.0 - cmnd[candidate], 0.0, 1.0);
    const double midi =
        69.0 + 12.0 * std::log2(frequency / 440.0);

    if (!std::isfinite(midi))
        return result;

    result.valid = true;
    result.frequencyHz = frequency;
    result.midiNote = midi;
    result.confidence = confidence;
    return result;
}

} // namespace

PitchDetectionResult PitchDetector::analyze(const OwnedAudioSource& source) {
    PitchDetectionResult output {};
    if (!source.valid())
        return output;

    const std::size_t frames = source.left.size();
    const double sampleRate = static_cast<double>(source.sampleRate);

    const std::size_t targetWindow = static_cast<std::size_t>(
        std::llround(sampleRate * 0.085)); // ~4 periods at 55 Hz.
    const std::size_t window = std::clamp<std::size_t>(
        targetWindow, 2048u, 8192u);

    if (frames < 512u)
        return output;

    std::vector<std::size_t> starts;
    if (frames <= window) {
        starts.push_back(0u);
    } else {
        constexpr double positions[] {0.10, 0.30, 0.50, 0.70, 0.90};
        starts.reserve(5u);
        for (double p : positions) {
            const double center = p * static_cast<double>(frames - 1u);
            const double half = 0.5 * static_cast<double>(window);
            const double rawStart = std::max(0.0, center - half);
            const std::size_t s = std::min<std::size_t>(
                static_cast<std::size_t>(rawStart), frames - window);
            if (starts.empty() || starts.back() != s)
                starts.push_back(s);
        }
    }

    std::vector<WindowPitch> candidates;
    candidates.reserve(starts.size());

    const std::size_t usedLength = std::min(window, frames);
    for (const auto start : starts) {
        auto candidate = analyzeWindow(source, start, usedLength);
        if (candidate.valid && candidate.confidence >= 0.88)
            candidates.push_back(candidate);
    }

    if (candidates.empty())
        return output;

    std::vector<double> midi;
    midi.reserve(candidates.size());
    for (const auto& c : candidates)
        midi.push_back(c.midiNote);
    std::sort(midi.begin(), midi.end());

    const double median = midi[midi.size() / 2u];

    std::vector<const WindowPitch*> inliers;
    inliers.reserve(candidates.size());
    for (const auto& c : candidates) {
        if (std::fabs(c.midiNote - median) <= 0.35)
            inliers.push_back(&c);
    }

    const std::size_t required =
        candidates.size() == 1u ? 1u :
        std::max<std::size_t>(2u, (candidates.size() * 3u + 4u) / 5u);

    if (inliers.size() < required)
        return output;

    double midiSum = 0.0;
    double freqSum = 0.0;
    double confidenceSum = 0.0;
    for (const auto* c : inliers) {
        midiSum += c->midiNote;
        freqSum += c->frequencyHz;
        confidenceSum += c->confidence;
    }

    const double count = static_cast<double>(inliers.size());
    const double meanConfidence = confidenceSum / count;

    // Single-window detections require extra certainty because stability
    // cannot be evaluated across time.
    if ((candidates.size() == 1u && meanConfidence < 0.94) ||
        meanConfidence < 0.90) {
        return output;
    }

    output.tonal = true;
    output.midiNote = static_cast<float>(midiSum / count);
    output.frequencyHz = static_cast<float>(freqSum / count);
    output.confidence = static_cast<float>(meanConfidence);
    output.stableWindows = static_cast<int>(inliers.size());
    return output;
}

} // namespace phraseator
