#include "PhraseatorProcessor.h"

#include "PhraseatorIDs.h"
#include "../source/parameters.h"
#include "../source/pitch_mapper.h"
#include "../source/pattern_fragment_remap.h"

#include "base/source/fstreamer.h"
#include "base/source/fstring.h"
#include "pluginterfaces/base/fstrdefs.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include "pluginterfaces/vst/ivstevents.h"
#include "pluginterfaces/vst/ivstprocesscontext.h"
#include "pluginterfaces/vst/vstspeaker.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iterator>
#include <string>
#include <vector>

namespace phraseator::vst3 {

using namespace Steinberg;
using namespace Steinberg::Vst;

namespace {

std::uint32_t liveHash(std::uint32_t seed,
                       std::uint32_t stepIndex,
                       std::uint32_t fragment,
                       std::uint32_t salt) noexcept {
    std::uint32_t x = seed ^ (stepIndex * 0x9E3779B9u) ^
                      (fragment * 0x85EBCA6Bu) ^ salt;
    x ^= x >> 16;
    x *= 0x7FEB352Du;
    x ^= x >> 15;
    x *= 0x846CA68Bu;
    x ^= x >> 16;
    return x;
}

float liveUnit(std::uint32_t h) noexcept {
    return static_cast<float>(h & 0x00FFFFFFu) /
           static_cast<float>(0x01000000u);
}

float livePitchSemitones(float amount, std::uint32_t h) noexcept {
    amount = std::clamp(amount, 0.0f, 1.0f);
    if (amount <= 0.0f)
        return 0.0f;

    static constexpr int gentle[] {0, 2, -2, 3, -3};
    static constexpr int musical[] {0, 2, -2, 3, -3, 5, -5, 7, -7};
    static constexpr int wide[] {0, 2, -2, 3, -3, 5, -5, 7, -7, 12, -12};

    auto choose = [h](const int* values, std::size_t count) noexcept {
        return static_cast<float>(values[h % static_cast<std::uint32_t>(count)]);
    };

    if (amount < 0.35f)
        return choose(gentle, std::size(gentle));
    if (amount < 0.60f)
        return choose(musical, std::size(musical));
    if (amount < 0.80f)
        return choose(wide, std::size(wide));

    const float chromaticChance = (amount - 0.80f) / 0.20f;
    const float selector = liveUnit(liveHash(h, 0u, 0u, 0xC001u));
    if (selector < chromaticChance) {
        return static_cast<float>(static_cast<int>(h % 25u) - 12);
    }

    return choose(wide, std::size(wide));
}
constexpr int32 kMinStateVersion = 1;
constexpr int32 kMaxRecallPathBytes = 16384;

double clamp01(double v) noexcept {
    if (!std::isfinite(v))
        return 0.0;
    return std::clamp(v, 0.0, 1.0);
}

}

Processor::Processor() {
    setControllerClass(kControllerUID);
    for (auto& edit : patternEditPending_)
        edit.store(-1, std::memory_order_relaxed);
}

tresult PLUGIN_API Processor::initialize(FUnknown* context) {
    const auto result = AudioEffect::initialize(context);
    if (result != kResultOk)
        return result;

    addAudioOutput(STR16("Stereo Out"), SpeakerArr::kStereo);
    addEventInput(STR16("Event In"), 16);

    sampleLoader_ = std::make_unique<SampleLoadWorker>(sampleBanks_);

    return kResultOk;
}

tresult PLUGIN_API Processor::terminate() {
    sampleLoader_.reset();
    return AudioEffect::terminate();
}

tresult PLUGIN_API Processor::notify(IMessage* message) {
    if (!message)
        return kInvalidArgument;

    if (FIDStringsEqual(message->getMessageID(), kMsgGenerate)) {
        generateCommandPending_.store(true, std::memory_order_release);
        return kResultTrue;
    }

    if (FIDStringsEqual(message->getMessageID(), kMsgVariate)) {
        variateCommandPending_.store(true, std::memory_order_release);
        return kResultTrue;
    }

    if (FIDStringsEqual(message->getMessageID(), kMsgPatternStep)) {
        auto* attributes = message->getAttributes();
        if (!attributes)
            return kResultFalse;

        int64 stepIndex = -1;
        int64 active = 0;
        int64 sourceIndex = 0;

        if (attributes->getInt(kAttrStepIndex, stepIndex) != kResultTrue ||
            attributes->getInt(kAttrStepActive, active) != kResultTrue ||
            attributes->getInt(kAttrStepSource, sourceIndex) != kResultTrue ||
            stepIndex < 0 || stepIndex >= static_cast<int64>(kStepCount) ||
            sourceIndex < 0 || sourceIndex >= static_cast<int64>(kSourceStatusCount)) {
            return kResultFalse;
        }

        const int encoded = active != 0
            ? static_cast<int>(sourceIndex) + 1
            : 0;
        patternEditPending_[static_cast<std::size_t>(stepIndex)].store(
            encoded, std::memory_order_release);
        return kResultTrue;
    }

    if (FIDStringsEqual(message->getMessageID(), kMsgClearSample)) {
        auto* attributes = message->getAttributes();
        if (!attributes || !sampleLoader_)
            return kResultFalse;

        int64 sourceIndex = -1;
        if (attributes->getInt(kAttrSourceIndex, sourceIndex) != kResultTrue ||
            sourceIndex < 0 ||
            sourceIndex >= static_cast<int64>(kMaxSources)) {
            return kResultFalse;
        }

        SampleLoadRequest request;
        request.sourceIndex = static_cast<std::size_t>(sourceIndex);
        request.mode = SampleLoadMode::Clear;

        const auto targetIndex = static_cast<std::size_t>(sourceIndex);
        const auto requestId = sampleLoader_->requestLoad(
            request,
            false,
            [this, targetIndex](
                const SampleLoadWorkerResult& result,
                const std::vector<SampleLoadRequest>&) {
                if (!result.ok())
                    return;

                std::lock_guard<std::mutex> lock(sourceRecallMutex_);
                sourceRecall_[targetIndex] = {};
                state_.sources[targetIndex] = {};
            });

        return requestId != 0u ? kResultTrue : kResultFalse;
    }

    if (!FIDStringsEqual(message->getMessageID(), kMsgLoadSample))
        return AudioEffect::notify(message);

    auto* attributes = message->getAttributes();
    if (!attributes || !sampleLoader_)
        return kResultFalse;

    TChar pathChars[2048] {};
    int64 sourceIndex = -1;
    int64 sourceId = 0;
    int64 mode = 0;
    int64 divisions = 0;
    int64 tonal = 0;
    double detectedRootMidi = -1.0;

    if (attributes->getString(kAttrPath, pathChars, sizeof(pathChars)) != kResultTrue ||
        attributes->getInt(kAttrSourceIndex, sourceIndex) != kResultTrue ||
        attributes->getInt(kAttrSourceId, sourceId) != kResultTrue ||
        attributes->getInt(kAttrMode, mode) != kResultTrue ||
        attributes->getInt(kAttrDivisions, divisions) != kResultTrue ||
        attributes->getInt(kAttrTonal, tonal) != kResultTrue ||
        attributes->getFloat(kAttrDetectedRootMidi, detectedRootMidi) != kResultTrue) {
        return kResultFalse;
    }

    if (sourceIndex < 0 || sourceIndex >= static_cast<int64>(kMaxSources) ||
        sourceId < 0 ||
        mode < 0 || mode > 2 ||
        divisions < 0 || divisions > static_cast<int64>(kMaxSlicesPerSource)) {
        return kResultFalse;
    }

    String path(pathChars);
    if (!path.toMultiByte(kCP_Utf8))
        return kResultFalse;

    const auto* utf8 = path.text8();
    if (!utf8 || *utf8 == 0)
        return kResultFalse;

    const std::string utf8Path(utf8);

    SampleLoadRequest request;
    request.sourceIndex = static_cast<std::size_t>(sourceIndex);
    request.sourceId = static_cast<std::uint32_t>(sourceId);
    request.path = std::filesystem::u8path(utf8Path);
    request.mode = mode == 0
        ? SampleLoadMode::OneShot
        : (mode == 1 ? SampleLoadMode::EqualSlices : SampleLoadMode::Auto);
    request.equalDivisions = static_cast<std::size_t>(divisions);
    request.preferTransient =
        request.mode == SampleLoadMode::EqualSlices ||
        request.mode == SampleLoadMode::Auto;
    request.tonal = tonal != 0;
    request.detectedRootMidi = static_cast<float>(detectedRootMidi);

    SourceRecallEntry recallCandidate;
    recallCandidate.occupied = true;
    recallCandidate.sourceId = static_cast<std::uint32_t>(sourceId);
    recallCandidate.mode = request.mode;
    recallCandidate.divisions = static_cast<std::uint16_t>(
        request.mode == SampleLoadMode::OneShot ? 1u : request.equalDivisions);
    recallCandidate.preferTransient = request.preferTransient;
    recallCandidate.tonal = request.tonal;
    recallCandidate.detectedRootMidi = request.detectedRootMidi;
    recallCandidate.utf8Path = utf8Path;

    const auto targetIndex = static_cast<std::size_t>(sourceIndex);
    const auto requestId = sampleLoader_->requestLoad(
        request,
        false,
        [this, targetIndex, recallCandidate](
            const SampleLoadWorkerResult& result,
            const std::vector<SampleLoadRequest>& resolvedRequests) {
            if (!result.ok() || resolvedRequests.empty())
                return;

            auto resolvedRecall = recallCandidate;
            const auto& resolved = resolvedRequests.front();
            resolvedRecall.mode = resolved.mode;
            resolvedRecall.divisions = static_cast<std::uint16_t>(
                resolved.mode == SampleLoadMode::OneShot
                    ? 1u
                    : (resolved.equalDivisions > 0u
                        ? resolved.equalDivisions
                        : resolved.resolvedSliceCount));
            resolvedRecall.preferTransient =
                resolved.mode == SampleLoadMode::EqualSlices &&
                resolved.preferTransient;
            resolvedRecall.resolvedSliceCount = resolved.resolvedSliceCount;
            resolvedRecall.resolvedSlices = resolved.resolvedSlices;
            resolvedRecall.tonal = resolved.tonal;
            resolvedRecall.detectedRootMidi = resolved.detectedRootMidi;

            std::lock_guard<std::mutex> lock(sourceRecallMutex_);
            sourceRecall_[targetIndex] = resolvedRecall;

            auto& meta = state_.sources[targetIndex];
            meta.occupied = true;
            meta.sourceId = resolvedRecall.sourceId;
            meta.sliceCount = resolved.resolvedSliceCount;
            meta.tonal = resolvedRecall.tonal;
            meta.detectedRootMidi = resolvedRecall.detectedRootMidi;
        });

    if (requestId == 0u)
        return kResultFalse;

    return kResultTrue;
}

tresult PLUGIN_API Processor::setBusArrangements(
    SpeakerArrangement* inputs,
    int32 numIns,
    SpeakerArrangement* outputs,
    int32 numOuts) {

    if (numIns != 0 || numOuts != 1 || outputs == nullptr ||
        outputs[0] != SpeakerArr::kStereo) {
        return kResultFalse;
    }

    return AudioEffect::setBusArrangements(inputs, numIns, outputs, numOuts);
}

tresult PLUGIN_API Processor::canProcessSampleSize(int32 symbolicSampleSize) {
    return symbolicSampleSize == kSample32
        ? kResultTrue
        : kResultFalse;
}

tresult PLUGIN_API Processor::setupProcessing(ProcessSetup& setup) {
    sampleRate_ = (std::isfinite(setup.sampleRate) && setup.sampleRate > 1000.0)
        ? setup.sampleRate
        : 48000.0;

    scheduler_.prepare(sampleRate_, 120.0);
    fx_.prepare(sampleRate_);
    fallbackProjectTimeSamples_ = 0.0;

    return AudioEffect::setupProcessing(setup);
}

tresult PLUGIN_API Processor::setActive(TBool state) {
    active_ = state != 0;
    if (!active_) {
        scheduler_.reset();
        fx_.reset();
        fallbackProjectTimeSamples_ = 0.0;
        heldMidiNotes_.fill(false);
        activeMidiNote_ = -1;
        midiTransposeSemitones_ = 0.0f;
        midiPhraseTimeSamples_ = 0.0;
        refreshSchedulerPattern();
    }
    return AudioEffect::setActive(state);
}

tresult PLUGIN_API Processor::setProcessing(TBool state) {
    processing_ = state != 0;
    if (processing_) {
        scheduler_.reset();
        scheduler_.prepare(sampleRate_, 120.0);
        refreshSchedulerPattern();
        fx_.reset();
        fallbackProjectTimeSamples_ = 0.0;
    } else {
        scheduler_.reset();
        fx_.reset();
        heldMidiNotes_.fill(false);
        activeMidiNote_ = -1;
        midiTransposeSemitones_ = 0.0f;
        refreshSchedulerPattern();
    }

    AudioEffect::setProcessing(state);
    return kResultTrue;
}

void Processor::applyNormalizedParameter(ParamID id, double rawValue) noexcept {
    const double value = clamp01(rawValue);

    if (id >= static_cast<ParamID>(kStepRatchetBase) &&
        id < static_cast<ParamID>(kStepRatchetBase + kStepRatchetCount)) {
        const auto stepIndex = static_cast<std::size_t>(
            id - static_cast<ParamID>(kStepRatchetBase));
        if (stepIndex < state_.pattern.size()) {
            state_.pattern[stepIndex].repeats = static_cast<std::uint8_t>(
                std::clamp(static_cast<int>(std::lround(value * 3.0)) + 1, 1, 4));
            refreshSchedulerPattern();
            patternViewDirty_ = true;
        }
        return;
    }

    if (id >= static_cast<ParamID>(kSourceMuteBase) &&
        id < static_cast<ParamID>(kSourceMuteBase + kSourceMuteCount)) {
        const auto sourceIndex = static_cast<std::size_t>(
            id - static_cast<ParamID>(kSourceMuteBase));
        if (sourceIndex < state_.sources.size()) {
            state_.sources[sourceIndex].muted = value >= 0.5;
            refreshSchedulerPattern();
        }
        return;
    }

    switch (id) {
        case static_cast<ParamID>(ParameterId::Density):
            state_.density = static_cast<float>(value);
            break;
        case static_cast<ParamID>(ParameterId::Variation):
            state_.variation = static_cast<float>(value);
            break;
        case static_cast<ParamID>(ParameterId::Repeat):
            state_.repeat = static_cast<float>(value);
            break;
        case static_cast<ParamID>(ParameterId::Pitch):
            state_.pitch = static_cast<float>(value);
            refreshSchedulerPattern();
            break;
        case static_cast<ParamID>(ParameterId::Pan):
            state_.pan = static_cast<float>(value);
            refreshSchedulerPattern();
            break;
        case static_cast<ParamID>(ParameterId::Groove):
            state_.groove = static_cast<float>(value);
            refreshSchedulerPattern();
            break;
        case static_cast<ParamID>(ParameterId::Velocity):
            state_.velocity = static_cast<float>(value);
            refreshSchedulerPattern();
            break;
        case static_cast<ParamID>(ParameterId::OctaveMode):
            state_.octaveMode = std::clamp(
                static_cast<int>(std::lround(value * 3.0)), 0, 3);
            refreshSchedulerPattern();
            break;
        case static_cast<ParamID>(ParameterId::KeyRoot):
            state_.keyRoot = std::clamp(static_cast<int>(std::lround(value * 11.0)), 0, 11);
            refreshSchedulerPattern();
            break;
        case static_cast<ParamID>(ParameterId::ScaleMode):
            state_.scaleMode = std::clamp(static_cast<int>(std::lround(value * 2.0)), 0, 2);
            refreshSchedulerPattern();
            break;
        case static_cast<ParamID>(ParameterId::PitchToKey):
            state_.pitchToKey = value >= 0.5;
            refreshSchedulerPattern();
            break;
        case static_cast<ParamID>(ParameterId::DelayAmount):
            state_.delayAmount = static_cast<float>(value);
            break;
        case static_cast<ParamID>(ParameterId::DelayDivision):
            state_.delayDivision = std::clamp(
                static_cast<int>(std::lround(value * 7.0)), 0, 7);
            break;
        case static_cast<ParamID>(ParameterId::ReverbAmount):
            state_.reverbAmount = static_cast<float>(value);
            break;
        case static_cast<ParamID>(ParameterId::DriveAmount):
            state_.driveAmount = static_cast<float>(value);
            break;
        case static_cast<ParamID>(ParameterId::FilterAmount):
            state_.filterAmount = static_cast<float>(value);
            break;
        case static_cast<ParamID>(ParameterId::FilterMode):
            state_.filterMode = value >= 0.5 ? 1 : 0;
            break;
        case static_cast<ParamID>(ParameterId::LockPattern):
            state_.lockPattern = value >= 0.5;
            break;
        case static_cast<ParamID>(ParameterId::RestartMode):
            state_.restartOnNote = value >= 0.5;
            break;
        case static_cast<ParamID>(ParameterId::GenerateTrigger): {
            const bool rising = generateTrigger_ < 0.5 && value >= 0.5;
            generateTrigger_ = value;
            if (rising && !state_.lockPattern)
                generatePattern();
            break;
        }
        case static_cast<ParamID>(ParameterId::VariateTrigger): {
            const bool rising = variateTrigger_ < 0.5 && value >= 0.5;
            variateTrigger_ = value;
            if (rising && !state_.lockPattern)
                varyPattern();
            break;
        }
        default:
            break;
    }
}

void Processor::readParameterChanges(IParameterChanges* changes) noexcept {
    if (!changes)
        return;

    for (int32 i = 0; i < changes->getParameterCount(); ++i) {
        auto* queue = changes->getParameterData(i);
        if (!queue || queue->getPointCount() <= 0)
            continue;

        int32 sampleOffset = 0;
        ParamValue value = 0.0;
        if (queue->getPoint(queue->getPointCount() - 1, sampleOffset, value) != kResultTrue)
            continue;

        applyNormalizedParameter(queue->getParameterId(), value);
    }
}

GenerationSettings Processor::currentGenerationSettings() const noexcept {
    GenerationSettings settings;
    settings.density = state_.density;
    settings.variation = state_.variation;
    settings.repeat = state_.repeat;
    // Live-shape controls are applied during playback so changing them does
    // not destroy or regenerate the phrase. The generator owns structure only.
    settings.pitch = 0.0f;
    settings.pan = 0.0f;
    settings.groove = 0.0f;
    settings.velocity = 0.0f;
    settings.octaveMode = 0;

    const auto& pool = sampleBanks_.activeBank().sourcePool();
    const auto fragments = pool.fragmentCount();
    settings.fragmentCount = static_cast<std::uint16_t>(
        std::clamp<std::size_t>(fragments == 0 ? 1 : fragments, 1, kMaxFragments));

    // Build source spans in the same flat-fragment order used by SourcePool.
    // PhraseEngine can then choose a source first and a slice second, so a
    // sliced loop does not automatically outweigh a one-shot by slice count.
    std::size_t flatCursor = 0u;
    std::uint8_t spanCount = 0u;

    for (std::size_t sourceIndex = 0;
         sourceIndex < kMaxSources && spanCount < kMaxGenerationSources;
         ++sourceIndex) {
        const auto* source = pool.source(sourceIndex);
        if (!source)
            continue;

        if (sourceIndex < state_.sources.size() &&
            state_.sources[sourceIndex].muted) {
            flatCursor += static_cast<std::size_t>(source->sliceCount);
            continue;
        }

        const auto sourceFragments =
            static_cast<std::size_t>(source->sliceCount);

        if (sourceFragments == 0u)
            continue;

        if (flatCursor < settings.fragmentCount) {
            const auto available = std::min<std::size_t>(
                sourceFragments,
                static_cast<std::size_t>(settings.fragmentCount) - flatCursor);

            if (available > 0u) {
                settings.sourceSpans[spanCount++] = {
                    static_cast<std::uint16_t>(flatCursor),
                    static_cast<std::uint16_t>(available)
                };
            }
        }

        flatCursor += sourceFragments;
    }

    settings.sourceSpanCount = spanCount;
    return settings;
}

void Processor::applyPitchToKey(Pattern& pattern) noexcept {
    if (!state_.pitchToKey)
        return;

    const auto& pool = sampleBanks_.activeBank().sourcePool();
    const auto scale = static_cast<ScaleMode>(std::clamp(state_.scaleMode, 0, 2));

    for (auto& step : pattern) {
        if (!step.active || pool.fragmentCount() == 0)
            continue;

        FragmentRef ref {};
        const auto flatIndex = static_cast<std::size_t>(step.fragment) % pool.fragmentCount();
        if (!pool.fragmentAt(flatIndex, ref))
            continue;

        const auto* source = pool.source(ref.sourceIndex);
        if (!source || !source->tonal || source->detectedRootMidi < 0.0f)
            continue;

        step.pitchSemitones = PitchMapper::quantizedOffset(
            source->detectedRootMidi,
            step.pitchSemitones,
            state_.keyRoot,
            scale);
    }
}

void Processor::generatePattern() noexcept {
    recallPatternRemapPending_.store(false, std::memory_order_release);
    state_.pattern = engine_.generate(currentGenerationSettings());
    refreshSchedulerPattern();
    patternViewDirty_ = true;
}

void Processor::varyPattern() noexcept {
    recallPatternRemapPending_.store(false, std::memory_order_release);
    state_.pattern = engine_.vary(state_.pattern, currentGenerationSettings());
    refreshSchedulerPattern();
    patternViewDirty_ = true;
}

void Processor::syncEngineFromState() noexcept {
    engine_.setSeed(state_.randomSeed);
    refreshSchedulerPattern();
    patternViewDirty_ = true;
}

bool Processor::patternHasActiveSteps() const noexcept {
    for (const auto& step : state_.pattern) {
        if (step.active)
            return true;
    }
    return false;
}

void Processor::refreshSchedulerPattern() noexcept {
    Pattern playback = state_.pattern;
    const auto& pool = sampleBanks_.activeBank().sourcePool();

    for (std::size_t i = 0; i < playback.size(); ++i) {
        auto& step = playback[i];
        if (!step.active)
            continue;

        FragmentRef ref {};
        if (pool.fragmentAt(step.fragment, ref) &&
            ref.sourceIndex < state_.sources.size() &&
            state_.sources[ref.sourceIndex].muted) {
            step.active = false;
            continue;
        }

        // Deterministic per-step live shaping. Moving a macro immediately
        // changes playback, but the stored pattern/source choice stays intact.
        const auto base = liveHash(
            state_.randomSeed,
            static_cast<std::uint32_t>(i),
            static_cast<std::uint32_t>(step.fragment),
            0x125A0001u);

        const float velocityDraw = liveUnit(liveHash(base, 0u, 0u, 0x1101u));
        step.velocity = 1.0f - velocityDraw * (0.28f * state_.velocity);

        const float panDraw =
            liveUnit(liveHash(base, 0u, 0u, 0x2202u)) * 2.0f - 1.0f;
        step.pan = panDraw * state_.pan;

        step.timingOffset = (i & 1u) != 0u
            ? state_.groove * 0.20f
            : 0.0f;

        step.pitchSemitones =
            livePitchSemitones(state_.pitch, liveHash(base, 0u, 0u, 0x3303u));

        if (state_.octaveMode == 1) {
            step.pitchSemitones += 12.0f;
        } else if (state_.octaveMode == 2) {
            step.pitchSemitones -= 12.0f;
        } else if (state_.octaveMode == 3) {
            const bool up =
                (liveHash(base, 0u, 0u, 0x4404u) & 1u) != 0u;
            step.pitchSemitones += up ? 12.0f : -12.0f;
        }
    }

    applyPitchToKey(playback);

    for (auto& step : playback) {
        step.pitchSemitones = std::clamp(
            step.pitchSemitones + midiTransposeSemitones_,
            -48.0f, 48.0f);
    }

    scheduler_.setPattern(playback);
}

void Processor::handleMidiEvent(const Event& event) noexcept {
    auto applyCurrentNote = [this](int note) noexcept {
        activeMidiNote_ = note;
        midiTransposeSemitones_ = note >= 0
            ? static_cast<float>(std::clamp(note - 60, -24, 24))
            : 0.0f;
        refreshSchedulerPattern();
    };

    if (event.type == Event::kNoteOnEvent) {
        const int pitch = std::clamp<int>(event.noteOn.pitch, 0, 127);

        if (event.noteOn.velocity <= 0.0f) {
            heldMidiNotes_[static_cast<std::size_t>(pitch)] = false;
        } else {
            bool hadHeldNote = false;
            for (const bool held : heldMidiNotes_) {
                if (held) {
                    hadHeldNote = true;
                    break;
                }
            }

            heldMidiNotes_[static_cast<std::size_t>(pitch)] = true;

            if (!patternHasActiveSteps() &&
                sampleBanks_.activeBank().sourcePool().fragmentCount() > 0u) {
                generatePattern();
            }

            if (state_.restartOnNote || !hadHeldNote) {
                midiPhraseTimeSamples_ = 0.0;
                scheduler_.reset();
            }

            applyCurrentNote(pitch);
            return;
        }
    } else if (event.type == Event::kNoteOffEvent) {
        const int pitch = std::clamp<int>(event.noteOff.pitch, 0, 127);
        heldMidiNotes_[static_cast<std::size_t>(pitch)] = false;
    } else {
        return;
    }

    if (activeMidiNote_ >= 0 &&
        heldMidiNotes_[static_cast<std::size_t>(activeMidiNote_)]) {
        return;
    }

    for (int note = 127; note >= 0; --note) {
        if (heldMidiNotes_[static_cast<std::size_t>(note)]) {
            applyCurrentNote(note);
            return;
        }
    }

    applyCurrentNote(-1);
}

void Processor::emitPatternViewParameters(ProcessData& data,
                                          int32 sampleOffset) noexcept {
    if (!patternViewDirty_ || !data.outputParameterChanges)
        return;

    const int32 safeOffset = data.numSamples > 0
        ? std::clamp<int32>(sampleOffset, 0, data.numSamples - 1)
        : 0;

    bool complete = true;

    for (int32 i = 0; i < kPatternViewCount; ++i) {
        int32 queueIndex = 0;
        auto* queue = data.outputParameterChanges->addParameterData(
            static_cast<ParamID>(kPatternViewBase + i), queueIndex);

        if (!queue) {
            complete = false;
            continue;
        }

        const auto& step = state_.pattern[static_cast<std::size_t>(i)];
        ParamValue value = 0.0;
        if (step.active) {
            FragmentRef fragment {};
            if (sampleBanks_.activeBank().sourcePool().fragmentAt(
                    step.fragment, fragment) &&
                fragment.sourceIndex < kSourceStatusCount) {
                value = static_cast<ParamValue>(fragment.sourceIndex + 1u) /
                        static_cast<ParamValue>(kPatternViewStepCount);
            }
        }

        int32 pointIndex = 0;
        if (queue->addPoint(safeOffset, value, pointIndex) != kResultTrue)
            complete = false;

        int32 ratchetQueueIndex = 0;
        auto* ratchetQueue = data.outputParameterChanges->addParameterData(
            static_cast<ParamID>(kStepRatchetBase + i), ratchetQueueIndex);
        if (!ratchetQueue) {
            complete = false;
        } else {
            const ParamValue ratchetValue =
                static_cast<ParamValue>(
                    std::clamp<int>(static_cast<int>(step.repeats), 1, 4) - 1) / 3.0;
            int32 ratchetPointIndex = 0;
            if (ratchetQueue->addPoint(
                    safeOffset, ratchetValue, ratchetPointIndex) != kResultTrue) {
                complete = false;
            }
        }
    }

    if (complete)
        patternViewDirty_ = false;
}

void Processor::emitSourceStatusParameters(ProcessData& data,
                                           int32 sampleOffset) noexcept {
    if (!sourceStatusDirty_ || !data.outputParameterChanges)
        return;

    const int32 safeOffset = data.numSamples > 0
        ? std::clamp<int32>(sampleOffset, 0, data.numSamples - 1)
        : 0;

    const auto& pool = sampleBanks_.activeBank().sourcePool();
    bool complete = true;

    for (int32 i = 0; i < kSourceStatusCount; ++i) {
        int32 queueIndex = 0;
        auto* queue = data.outputParameterChanges->addParameterData(
            static_cast<ParamID>(kSourceStatusBase + i), queueIndex);

        if (!queue) {
            complete = false;
            continue;
        }

        ParamValue value = 0.0;
        if (const auto* source = pool.source(static_cast<std::size_t>(i))) {
            if (source->type == SourceType::OneShot)
                value = 0.5;
            else if (source->type == SourceType::Loop)
                value = 1.0;
        }

        int32 pointIndex = 0;
        if (queue->addPoint(safeOffset, value, pointIndex) != kResultTrue)
            complete = false;
    }

    if (complete)
        sourceStatusDirty_ = false;
}

tresult PLUGIN_API Processor::process(ProcessData& data) {
    if (data.numOutputs <= 0 || data.outputs == nullptr || data.numSamples <= 0) {
        // Parameter-only flush calls still need to leave the component in the
        // final host-provided state even though there is no audio to segment.
        readParameterChanges(data.inputParameterChanges);
        emitPatternViewParameters(data, 0);
        emitSourceStatusParameters(data, 0);
        return kResultOk;
    }

    if (data.outputs[0].numChannels < 2)
        return kResultFalse;

    double tempo = 120.0;
    double projectTime = fallbackProjectTimeSamples_;
    bool playing = processing_ && active_;

    if (data.processContext) {
        const auto& ctx = *data.processContext;

        if ((ctx.state & ProcessContext::kTempoValid) != 0 &&
            std::isfinite(ctx.tempo) && ctx.tempo > 0.0) {
            tempo = ctx.tempo;
        }

        // Steinberg VST3 defines projectTimeSamples as always valid.
        projectTime = static_cast<double>(ctx.projectTimeSamples);

        playing = playing && ((ctx.state & ProcessContext::kPlaying) != 0);
    }

    const bool recallRemapPending =
        recallPatternRemapPending_.load(std::memory_order_acquire);

    PatternFragmentSnapshot patternSnapshot {};
    const auto& oldPool = sampleBanks_.activeBank().sourcePool();
    const bool hadOldFragments = oldPool.fragmentCount() > 0u;

    if (!recallRemapPending && hadOldFragments)
        patternSnapshot = snapshotPatternFragments(state_.pattern, oldPool);

    if (sampleBanks_.consumePending()) {
        sourceStatusDirty_ = true;
        const auto& newPool = sampleBanks_.activeBank().sourcePool();

        bool patternChanged = false;

        if (recallPatternRemapPending_.exchange(
                false, std::memory_order_acq_rel)) {
            patternChanged = restorePatternFragments(
                state_.pattern, recallPatternSnapshot_, newPool);
        } else if (hadOldFragments) {
            patternChanged = restorePatternFragments(
                state_.pattern, patternSnapshot, newPool);
        }

        if (patternChanged) {
            scheduler_.setPattern(state_.pattern);
            patternViewDirty_ = true;
        }
    }

    const auto& bank = sampleBanks_.activeBank();

    scheduler_.prepare(sampleRate_, tempo);
    refreshSchedulerPattern();

    if (generateCommandPending_.exchange(false, std::memory_order_acq_rel) &&
        !state_.lockPattern) {
        generatePattern();
    }
    if (variateCommandPending_.exchange(false, std::memory_order_acq_rel) &&
        !state_.lockPattern) {
        varyPattern();
    }

    bool patternEdited = false;
    for (std::size_t i = 0; i < kStepCount; ++i) {
        const int encoded =
            patternEditPending_[i].exchange(-1, std::memory_order_acq_rel);
        if (encoded < 0)
            continue;

        auto& step = state_.pattern[i];
        if (encoded == 0) {
            step.active = false;
            patternEdited = true;
            continue;
        }

        const auto sourceIndex = static_cast<std::uint16_t>(encoded - 1);
        std::size_t flatIndex = 0u;
        if (!bank.sourcePool().flatIndexOf({sourceIndex, 0u}, flatIndex))
            continue;

        step.active = true;
        step.fragment = static_cast<std::uint16_t>(flatIndex);
        step.velocity = std::clamp(step.velocity, 0.0f, 1.0f);
        if (step.velocity <= 0.0f)
            step.velocity = 1.0f;
        step.gate = std::clamp(step.gate, 0.0f, 1.0f);
        if (step.gate <= 0.0f)
            step.gate = 1.0f;
        if (step.repeats == 0u)
            step.repeats = 1u;
        patternEdited = true;
    }

    if (patternEdited) {
        refreshSchedulerPattern();
        patternViewDirty_ = true;
    }

    if (data.symbolicSampleSize != kSample32)
        return kResultFalse;

    auto** out = data.outputs[0].channelBuffers32;
    if (!out || !out[0] || !out[1])
        return kResultFalse;

    struct ParameterCursor {
        IParamValueQueue* queue {nullptr};
        int32 pointIndex {0};
        int32 pointCount {0};
        int32 nextOffset {0};
        ParamValue nextValue {0.0};
        bool valid {false};
    };

    // Phraseator currently exports far fewer than 32 parameters. Keeping this
    // fixed-size avoids allocation in process() while still leaving headroom.
    constexpr std::size_t kMaxParameterQueues = 32;
    std::array<ParameterCursor, kMaxParameterQueues> cursors {};
    std::size_t cursorCount = 0;

    if (data.inputParameterChanges) {
        const int32 parameterCount = data.inputParameterChanges->getParameterCount();
        const int32 boundedCount = std::min<int32>(
            parameterCount, static_cast<int32>(kMaxParameterQueues));

        for (int32 i = 0; i < boundedCount; ++i) {
            auto* queue = data.inputParameterChanges->getParameterData(i);
            if (!queue)
                continue;

            const int32 pointCount = queue->getPointCount();
            if (pointCount <= 0)
                continue;

            auto& cursor = cursors[cursorCount];
            cursor.queue = queue;
            cursor.pointCount = pointCount;

            int32 offset = 0;
            ParamValue value = 0.0;
            if (queue->getPoint(0, offset, value) != kResultTrue)
                continue;

            cursor.nextOffset = std::clamp<int32>(offset, 0, data.numSamples - 1);
            cursor.nextValue = value;
            cursor.valid = true;
            ++cursorCount;
        }
    }

    auto advanceCursor = [&](ParameterCursor& cursor) noexcept {
        ++cursor.pointIndex;
        if (cursor.pointIndex >= cursor.pointCount) {
            cursor.valid = false;
            return;
        }

        int32 offset = 0;
        ParamValue value = 0.0;
        if (cursor.queue->getPoint(cursor.pointIndex, offset, value) != kResultTrue) {
            cursor.valid = false;
            return;
        }

        cursor.nextOffset = std::clamp<int32>(offset, 0, data.numSamples - 1);
        cursor.nextValue = value;
    };

    auto applyChangesAt = [&](int32 sampleOffset) noexcept {
        for (std::size_t i = 0; i < cursorCount; ++i) {
            auto& cursor = cursors[i];

            while (cursor.valid && cursor.nextOffset <= sampleOffset) {
                applyNormalizedParameter(
                    cursor.queue->getParameterId(), cursor.nextValue);
                advanceCursor(cursor);
            }
        }
    };

    auto nextAutomationOffset = [&](int32 currentOffset) noexcept {
        int32 next = data.numSamples;

        for (std::size_t i = 0; i < cursorCount; ++i) {
            const auto& cursor = cursors[i];
            if (cursor.valid && cursor.nextOffset >= currentOffset)
                next = std::min(next, cursor.nextOffset);
        }

        return next;
    };

    int32 eventIndex = 0;
    int32 eventCount = data.inputEvents ? data.inputEvents->getEventCount() : 0;
    Event nextEvent {};
    bool eventValid = false;
    int32 nextEventOffset = data.numSamples;

    auto advanceEvent = [&]() noexcept {
        eventValid = false;
        while (eventIndex < eventCount) {
            Event candidate {};
            if (data.inputEvents->getEvent(eventIndex++, candidate) != kResultTrue)
                continue;

            nextEvent = candidate;
            nextEventOffset = std::clamp<int32>(
                candidate.sampleOffset, 0, data.numSamples - 1);
            eventValid = true;
            return;
        }
        nextEventOffset = data.numSamples;
    };

    auto applyEventsAt = [&](int32 sampleOffset) noexcept {
        while (eventValid && nextEventOffset <= sampleOffset) {
            handleMidiEvent(nextEvent);
            advanceEvent();
        }
    };

    advanceEvent();

    bool producedAudio = false;
    int32 currentOffset = 0;

    // Changes/events at sample 0 must affect the first rendered sample.
    applyChangesAt(0);
    applyEventsAt(0);

    while (currentOffset < data.numSamples) {
        int32 boundary = std::min(
            nextAutomationOffset(currentOffset),
            eventValid ? nextEventOffset : data.numSamples);

        // If the next pending point is exactly at the current position,
        // consume it first and re-evaluate the following boundary.
        if (boundary == currentOffset) {
            applyChangesAt(currentOffset);
            applyEventsAt(currentOffset);
            boundary = std::min(
                nextAutomationOffset(currentOffset),
                eventValid ? nextEventOffset : data.numSamples);
            if (boundary == currentOffset)
                boundary = std::min<int32>(data.numSamples, currentOffset + 1);
        }

        const int32 segmentEnd = std::clamp<int32>(
            boundary, currentOffset + 1, data.numSamples);
        const std::size_t segmentSamples =
            static_cast<std::size_t>(segmentEnd - currentOffset);

        const bool midiGateOpen = activeMidiNote_ >= 0;
        const bool phrasePlaying = playing && midiGateOpen;
        const double absoluteSegmentTime =
            projectTime + static_cast<double>(currentOffset);
        const double phraseTime = state_.restartOnNote
            ? midiPhraseTimeSamples_
            : absoluteSegmentTime;

        const bool schedulerAudio = scheduler_.processBlock(
            bank.sourcePool(),
            bank.buffers(),
            phraseTime,
            phrasePlaying,
            out[0] + currentOffset,
            out[1] + currentOffset,
            segmentSamples);

        fx_.setDelayAmount(state_.delayAmount);
        fx_.setDelayDivision(state_.delayDivision);
        fx_.setFilterAmount(state_.filterAmount);
        fx_.setFilterMode(state_.filterMode);
        const bool fxAudio = fx_.processBlock(
            out[0] + currentOffset,
            out[1] + currentOffset,
            segmentSamples,
            tempo);

        producedAudio = producedAudio || schedulerAudio || fxAudio;
        if (state_.restartOnNote && phrasePlaying)
            midiPhraseTimeSamples_ += static_cast<double>(segmentSamples);
        currentOffset = segmentEnd;

        if (currentOffset < data.numSamples) {
            applyChangesAt(currentOffset);
            applyEventsAt(currentOffset);
        }
    }

    data.outputs[0].silenceFlags = producedAudio ? 0 : 0x3;
    emitPatternViewParameters(data, data.numSamples - 1);
    emitSourceStatusParameters(data, data.numSamples - 1);
    fallbackProjectTimeSamples_ = projectTime + static_cast<double>(data.numSamples);

    return kResultOk;
}

bool Processor::writeProjectState(IBStream* state) const noexcept {
    if (!state)
        return false;

    IBStreamer stream(state, kLittleEndian);

    if (!stream.writeInt32(kStateMagic) ||
        !stream.writeInt32(kStateVersion) ||
        !stream.writeInt32(static_cast<int32>(state_.randomSeed))) {
        return false;
    }

    const double scalars[] {
        state_.density,
        state_.variation,
        state_.repeat,
        state_.pitch,
        state_.pan,
        state_.groove,
        state_.delayAmount,
        state_.reverbAmount,
        state_.driveAmount,
        state_.filterAmount,
        state_.velocity
    };

    for (const auto value : scalars) {
        if (!stream.writeDouble(value))
            return false;
    }

    if (!stream.writeInt32(state_.keyRoot) ||
        !stream.writeInt32(state_.scaleMode) ||
        !stream.writeInt32(state_.pitchToKey ? 1 : 0) ||
        !stream.writeInt32(state_.lockPattern ? 1 : 0) ||
        !stream.writeInt32(state_.restartOnNote ? 1 : 0) ||
        !stream.writeInt32(state_.delayDivision) ||
        !stream.writeInt32(state_.filterMode) ||
        !stream.writeInt32(state_.octaveMode)) {
        return false;
    }

    for (std::size_t i = 0; i < kSourceMuteCount; ++i) {
        if (!stream.writeInt32(
                state_.sources[i].muted ? 1 : 0)) {
            return false;
        }
    }

    for (const auto& step : state_.pattern) {
        if (!stream.writeInt32(step.active ? 1 : 0) ||
            !stream.writeInt32(static_cast<int32>(step.fragment)) ||
            !stream.writeDouble(step.velocity) ||
            !stream.writeDouble(step.pitchSemitones) ||
            !stream.writeDouble(step.pan) ||
            !stream.writeDouble(step.gate) ||
            !stream.writeInt32(static_cast<int32>(step.repeats)) ||
            !stream.writeDouble(step.timingOffset)) {
            return false;
        }
    }

    std::lock_guard<std::mutex> lock(sourceRecallMutex_);
    for (const auto& recall : sourceRecall_) {
        if (recall.utf8Path.size() > static_cast<std::size_t>(kMaxRecallPathBytes))
            return false;

        const auto pathSize = static_cast<int32>(recall.utf8Path.size());

        if (!stream.writeInt32(recall.occupied ? 1 : 0) ||
            !stream.writeInt32u(recall.sourceId) ||
            !stream.writeInt32(static_cast<int32>(recall.mode)) ||
            !stream.writeInt32(static_cast<int32>(recall.divisions)) ||
            !stream.writeInt32(recall.preferTransient ? 1 : 0) ||
            !stream.writeInt32(recall.tonal ? 1 : 0) ||
            !stream.writeDouble(recall.detectedRootMidi) ||
            !stream.writeInt32(pathSize)) {
            return false;
        }

        if (pathSize > 0 &&
            stream.writeRaw(recall.utf8Path.data(), pathSize) != pathSize) {
            return false;
        }

        if (!stream.writeInt32(static_cast<int32>(recall.resolvedSliceCount)))
            return false;

        for (const auto& region : recall.resolvedSlices) {
            if (!stream.writeInt32u(region.startFrame) ||
                !stream.writeInt32u(region.endFrame)) {
                return false;
            }
        }
    }

    return true;
}

bool Processor::readProjectState(IBStream* state) noexcept {
    if (!state)
        return false;

    IBStreamer stream(state, kLittleEndian);

    int32 magic = 0;
    int32 version = 0;
    int32 seed = 0;

    if (!stream.readInt32(magic) || magic != kStateMagic ||
        !stream.readInt32(version) ||
        version < kMinStateVersion || version > kStateVersion ||
        !stream.readInt32(seed)) {
        return false;
    }

    ProjectState candidate {};
    candidate.version = static_cast<std::uint32_t>(version);
    candidate.randomSeed = static_cast<std::uint32_t>(seed);

    double values[10] {};
    for (double& value : values) {
        if (!stream.readDouble(value) || !std::isfinite(value))
            return false;
        value = std::clamp(value, 0.0, 1.0);
    }

    candidate.density = static_cast<float>(values[0]);
    candidate.variation = static_cast<float>(values[1]);
    candidate.repeat = static_cast<float>(values[2]);
    candidate.pitch = static_cast<float>(values[3]);
    candidate.pan = static_cast<float>(values[4]);
    candidate.groove = static_cast<float>(values[5]);
    candidate.delayAmount = static_cast<float>(values[6]);
    candidate.reverbAmount = static_cast<float>(values[7]);
    candidate.driveAmount = static_cast<float>(values[8]);
    candidate.filterAmount = static_cast<float>(values[9]);

    // V1-v8 always generated velocity in the full historical range.
    candidate.velocity = 1.0f;
    if (version >= 9) {
        double velocityDepth = 0.0;
        if (!stream.readDouble(velocityDepth) || !std::isfinite(velocityDepth))
            return false;
        candidate.velocity =
            static_cast<float>(std::clamp(velocityDepth, 0.0, 1.0));
    }

    int32 keyRoot = 0;
    int32 scaleMode = 0;
    int32 pitchToKey = 0;
    int32 lockPattern = 0;
    int32 restartOnNote = 1;
    int32 delayDivision = 1;
    int32 filterMode = 1;
    int32 octaveMode = 0;
    std::array<int32, kSourceMuteCount> sourceMuted {};

    if (!stream.readInt32(keyRoot) ||
        !stream.readInt32(scaleMode) ||
        !stream.readInt32(pitchToKey) ||
        !stream.readInt32(lockPattern) ||
        (version >= 5 && !stream.readInt32(restartOnNote)) ||
        (version >= 6 && !stream.readInt32(delayDivision)) ||
        (version >= 6 && !stream.readInt32(filterMode)) ||
        (version >= 9 && !stream.readInt32(octaveMode))) {
        return false;
    }

    if (version >= 8) {
        for (auto& muted : sourceMuted) {
            if (!stream.readInt32(muted))
                return false;
        }
    }

    candidate.keyRoot = std::clamp<int32>(keyRoot, 0, 11);
    candidate.scaleMode = std::clamp<int32>(scaleMode, 0, 2);
    candidate.pitchToKey = pitchToKey != 0;
    candidate.lockPattern = lockPattern != 0;
    candidate.restartOnNote = version >= 5 ? restartOnNote != 0 : true;
    candidate.delayDivision = version >= 7
        ? std::clamp(delayDivision, 0, 7)
        : (version >= 6 ? std::clamp(delayDivision, 0, 6) + 1 : 2);
    candidate.filterMode = version >= 6 ? (filterMode != 0 ? 1 : 0) : 1;
    candidate.octaveMode = version >= 9 ? std::clamp<int32>(octaveMode, 0, 3) : 0;
    for (std::size_t i = 0; i < kSourceMuteCount; ++i)
        candidate.sources[i].muted =
            version >= 8 && sourceMuted[i] != 0;

    for (auto& step : candidate.pattern) {
        int32 active = 0;
        int32 fragment = 0;
        int32 repeats = 1;
        double velocity = 0.0;
        double pitch = 0.0;
        double pan = 0.0;
        double gate = 0.0;
        double timing = 0.0;

        if (!stream.readInt32(active) ||
            !stream.readInt32(fragment) ||
            !stream.readDouble(velocity) ||
            !stream.readDouble(pitch) ||
            !stream.readDouble(pan) ||
            !stream.readDouble(gate) ||
            !stream.readInt32(repeats) ||
            !stream.readDouble(timing)) {
            return false;
        }

        if (!std::isfinite(velocity) || !std::isfinite(pitch) ||
            !std::isfinite(pan) || !std::isfinite(gate) ||
            !std::isfinite(timing)) {
            return false;
        }

        step.active = active != 0;
        step.fragment = static_cast<std::uint16_t>(
            std::clamp<int32>(fragment, 0, static_cast<int32>(kMaxFragments - 1)));
        step.velocity = static_cast<float>(std::clamp(velocity, 0.0, 1.0));
        step.pitchSemitones = static_cast<float>(std::clamp(pitch, -24.0, 24.0));
        step.pan = static_cast<float>(std::clamp(pan, -1.0, 1.0));
        step.gate = static_cast<float>(std::clamp(gate, 0.0, 1.0));
        step.repeats = static_cast<std::uint8_t>(std::clamp<int32>(repeats, 1, 8));
        step.timingOffset = static_cast<float>(std::clamp(timing, -0.5, 0.5));
    }

    std::array<SourceRecallEntry, kMaxSources> recallEntries {};

    if (version >= 2) {
        for (std::size_t i = 0; i < recallEntries.size(); ++i) {
            int32 occupied = 0;
            uint32 sourceId = 0;
            int32 mode = 0;
            int32 divisions = 0;
            int32 preferTransient = 0;
            int32 tonal = 0;
            double detectedRootMidi = -1.0;
            int32 pathSize = 0;

            if (!stream.readInt32(occupied) ||
                !stream.readInt32u(sourceId) ||
                !stream.readInt32(mode) ||
                !stream.readInt32(divisions) ||
                (version >= 3 && !stream.readInt32(preferTransient)) ||
                !stream.readInt32(tonal) ||
                !stream.readDouble(detectedRootMidi) ||
                !stream.readInt32(pathSize)) {
                return false;
            }

            if (mode < 0 || mode > 1 ||
                divisions < 0 || divisions > static_cast<int32>(kMaxSlicesPerSource) ||
                !std::isfinite(detectedRootMidi) ||
                pathSize < 0 || pathSize > kMaxRecallPathBytes) {
                return false;
            }

            std::string path;
            try {
                path.resize(static_cast<std::size_t>(pathSize));
            } catch (...) {
                return false;
            }

            if (pathSize > 0 &&
                stream.readRaw(path.data(), pathSize) != pathSize) {
                return false;
            }

            std::uint16_t resolvedSliceCount = 0u;
            std::array<SliceRegion, kMaxSlicesPerSource> resolvedSlices {};

            if (version >= 4) {
                int32 storedCount = 0;
                if (!stream.readInt32(storedCount) ||
                    storedCount < 0 ||
                    storedCount > static_cast<int32>(kMaxSlicesPerSource)) {
                    return false;
                }

                resolvedSliceCount = static_cast<std::uint16_t>(storedCount);
                std::uint32_t previousEnd = 0u;

                for (std::size_t slice = 0; slice < kMaxSlicesPerSource; ++slice) {
                    uint32 startFrame = 0u;
                    uint32 endFrame = 0u;

                    if (!stream.readInt32u(startFrame) ||
                        !stream.readInt32u(endFrame)) {
                        return false;
                    }

                    resolvedSlices[slice] = {startFrame, endFrame};

                    if (slice < resolvedSliceCount) {
                        const auto region = resolvedSlices[slice];
                        if (!region.valid() ||
                            (slice > 0u && region.startFrame < previousEnd)) {
                            return false;
                        }
                        previousEnd = region.endFrame;
                    }
                }
            }

            auto& recall = recallEntries[i];
            recall.occupied = occupied != 0;
            recall.sourceId = sourceId;
            recall.mode = mode == 0 ? SampleLoadMode::OneShot : SampleLoadMode::EqualSlices;
            recall.divisions = static_cast<std::uint16_t>(divisions);
            recall.preferTransient = version >= 3 && preferTransient != 0;
            recall.resolvedSliceCount = resolvedSliceCount;
            recall.resolvedSlices = resolvedSlices;
            recall.tonal = tonal != 0;
            recall.detectedRootMidi = static_cast<float>(detectedRootMidi);
            recall.utf8Path = std::move(path);

            auto& meta = candidate.sources[i];
            meta.occupied = recall.occupied;
            meta.sourceId = recall.sourceId;
            meta.sliceCount = recall.divisions;
            meta.tonal = recall.tonal;
            meta.detectedRootMidi = recall.detectedRootMidi;
        }
    }

    std::array<std::uint16_t, kMaxSources> savedSliceCounts {};
    for (std::size_t i = 0; i < recallEntries.size(); ++i) {
        const auto& recall = recallEntries[i];
        if (!recall.occupied)
            continue;

        if (recall.mode == SampleLoadMode::OneShot) {
            savedSliceCounts[i] = 1u;
        } else {
            savedSliceCounts[i] = recall.resolvedSliceCount > 0u
                ? recall.resolvedSliceCount
                : recall.divisions;
        }
    }

    recallPatternSnapshot_ =
        snapshotPatternFragmentsFromSourceCounts(
            candidate.pattern, savedSliceCounts);

    bool hasRecallIdentity = false;
    for (const auto& identity : recallPatternSnapshot_) {
        if (identity.valid) {
            hasRecallIdentity = true;
            break;
        }
    }
    recallPatternRemapPending_.store(
        hasRecallIdentity, std::memory_order_release);

    state_ = candidate;
    {
        std::lock_guard<std::mutex> lock(sourceRecallMutex_);
        sourceRecall_ = std::move(recallEntries);
    }

    syncEngineFromState();
    queueRecallLoads();
    return true;
}

void Processor::queueRecallLoads() noexcept {
    if (!sampleLoader_) {
        recallPatternRemapPending_.store(false, std::memory_order_release);
        return;
    }

    try {
        std::vector<SampleLoadRequest> requests;
        requests.reserve(kMaxSources);

        {
            std::lock_guard<std::mutex> lock(sourceRecallMutex_);

            for (std::size_t i = 0; i < sourceRecall_.size(); ++i) {
                const auto& recall = sourceRecall_[i];

                SampleLoadRequest request;
                request.sourceIndex = i;

                bool canRestoreFile = false;
                std::filesystem::path path;

                if (recall.occupied && !recall.utf8Path.empty()) {
                    std::error_code ec;
                    path = std::filesystem::u8path(recall.utf8Path);
                    canRestoreFile =
                        std::filesystem::exists(path, ec) && !ec;
                }

                if (!canRestoreFile) {
                    // State restore is authoritative. Empty or missing sources
                    // must clear stale material from any previous bank state.
                    request.mode = SampleLoadMode::Clear;
                    requests.push_back(std::move(request));
                    continue;
                }

                request.sourceId = recall.sourceId;
                request.path = std::move(path);
                request.mode = recall.mode;
                request.equalDivisions = recall.mode == SampleLoadMode::OneShot
                    ? 0u
                    : static_cast<std::size_t>(recall.divisions);
                request.preferTransient = recall.preferTransient;
                request.useStoredSlices =
                    recall.mode == SampleLoadMode::EqualSlices &&
                    recall.resolvedSliceCount > 0u;
                request.resolvedSliceCount = recall.resolvedSliceCount;
                request.resolvedSlices = recall.resolvedSlices;
                request.tonal = recall.tonal;
                request.detectedRootMidi = recall.detectedRootMidi;
                requests.push_back(std::move(request));
            }
        }

        const auto requestId =
            sampleLoader_->requestBatch(std::move(requests));

        if (requestId == 0u) {
            recallPatternRemapPending_.store(
                false, std::memory_order_release);
        }

    } catch (...) {
        recallPatternRemapPending_.store(false, std::memory_order_release);
        // Project state remains intact even if asynchronous source restoration
        // cannot be queued. The host must never crash on a recall failure.
    }
}

tresult PLUGIN_API Processor::setState(IBStream* state) {
    return readProjectState(state) ? kResultOk : kResultFalse;
}

tresult PLUGIN_API Processor::getState(IBStream* state) {
    return writeProjectState(state) ? kResultOk : kResultFalse;
}

} // namespace phraseator::vst3
