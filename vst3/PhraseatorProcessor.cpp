#include "PhraseatorProcessor.h"

#include "PhraseatorIDs.h"
#include "../source/parameters.h"
#include "../source/pitch_mapper.h"

#include "base/source/fstreamer.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#include "pluginterfaces/vst/ivstprocesscontext.h"
#include "pluginterfaces/vst/vstspeaker.h"

#include <algorithm>
#include <cmath>

namespace phraseator::vst3 {

using namespace Steinberg;
using namespace Steinberg::Vst;

namespace {
double clamp01(double v) noexcept {
    if (!std::isfinite(v))
        return 0.0;
    return std::clamp(v, 0.0, 1.0);
}

}

Processor::Processor() {
    setControllerClass(kControllerUID);
}

tresult PLUGIN_API Processor::initialize(FUnknown* context) {
    const auto result = AudioEffect::initialize(context);
    if (result != kResultOk)
        return result;

    addAudioOutput(STR16("Stereo Out"), SpeakerArr::kStereo);
    addEventInput(STR16("Event In"), 16);

    return kResultOk;
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
    fallbackProjectTimeSamples_ = 0.0;

    return AudioEffect::setupProcessing(setup);
}

tresult PLUGIN_API Processor::setActive(TBool state) {
    active_ = state != 0;
    if (!active_) {
        scheduler_.reset();
        fallbackProjectTimeSamples_ = 0.0;
    }
    return AudioEffect::setActive(state);
}

tresult PLUGIN_API Processor::setProcessing(TBool state) {
    processing_ = state != 0;
    if (processing_) {
        scheduler_.reset();
        scheduler_.prepare(sampleRate_, 120.0);
        scheduler_.setPattern(state_.pattern);
        fallbackProjectTimeSamples_ = 0.0;
    } else {
        scheduler_.reset();
    }

    AudioEffect::setProcessing(state);
    return kResultTrue;
}

void Processor::applyNormalizedParameter(ParamID id, double rawValue) noexcept {
    const double value = clamp01(rawValue);

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
            break;
        case static_cast<ParamID>(ParameterId::Pan):
            state_.pan = static_cast<float>(value);
            break;
        case static_cast<ParamID>(ParameterId::Groove):
            state_.groove = static_cast<float>(value);
            break;
        case static_cast<ParamID>(ParameterId::KeyRoot):
            state_.keyRoot = std::clamp(static_cast<int>(std::lround(value * 11.0)), 0, 11);
            break;
        case static_cast<ParamID>(ParameterId::ScaleMode):
            state_.scaleMode = std::clamp(static_cast<int>(std::lround(value * 2.0)), 0, 2);
            break;
        case static_cast<ParamID>(ParameterId::PitchToKey):
            state_.pitchToKey = value >= 0.5;
            break;
        case static_cast<ParamID>(ParameterId::DelayAmount):
            state_.delayAmount = static_cast<float>(value);
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
        case static_cast<ParamID>(ParameterId::LockPattern):
            state_.lockPattern = value >= 0.5;
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
    settings.pitch = state_.pitch;
    settings.pan = state_.pan;
    settings.groove = state_.groove;

    const auto fragments = sampleBanks_.activeBank().sourcePool().fragmentCount();
    settings.fragmentCount = static_cast<std::uint16_t>(
        std::clamp<std::size_t>(fragments == 0 ? 1 : fragments, 1, kMaxFragments));
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
    state_.pattern = engine_.generate(currentGenerationSettings());
    applyPitchToKey(state_.pattern);
    scheduler_.setPattern(state_.pattern);
}

void Processor::varyPattern() noexcept {
    state_.pattern = engine_.vary(state_.pattern, currentGenerationSettings());
    applyPitchToKey(state_.pattern);
    scheduler_.setPattern(state_.pattern);
}

void Processor::syncEngineFromState() noexcept {
    engine_.setSeed(state_.randomSeed);
    scheduler_.setPattern(state_.pattern);
}

tresult PLUGIN_API Processor::process(ProcessData& data) {
    readParameterChanges(data.inputParameterChanges);

    if (data.numOutputs <= 0 || data.outputs == nullptr || data.numSamples <= 0)
        return kResultOk;

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

    sampleBanks_.consumePending();
    const auto& bank = sampleBanks_.activeBank();

    scheduler_.prepare(sampleRate_, tempo);
    scheduler_.setPattern(state_.pattern);

    if (data.symbolicSampleSize == kSample32) {
        auto** out = data.outputs[0].channelBuffers32;
        if (!out || !out[0] || !out[1])
            return kResultFalse;

        const bool producedAudio = scheduler_.processBlock(
            bank.sourcePool(), bank.buffers(), projectTime, playing,
            out[0], out[1], static_cast<std::size_t>(data.numSamples));

        data.outputs[0].silenceFlags = producedAudio ? 0 : 0x3;

    } else {
        return kResultFalse;
    }

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
        state_.filterAmount
    };

    for (const auto value : scalars) {
        if (!stream.writeDouble(value))
            return false;
    }

    if (!stream.writeInt32(state_.keyRoot) ||
        !stream.writeInt32(state_.scaleMode) ||
        !stream.writeInt32(state_.pitchToKey ? 1 : 0) ||
        !stream.writeInt32(state_.lockPattern ? 1 : 0)) {
        return false;
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
        !stream.readInt32(version) || version != kStateVersion ||
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

    int32 keyRoot = 0;
    int32 scaleMode = 0;
    int32 pitchToKey = 0;
    int32 lockPattern = 0;

    if (!stream.readInt32(keyRoot) ||
        !stream.readInt32(scaleMode) ||
        !stream.readInt32(pitchToKey) ||
        !stream.readInt32(lockPattern)) {
        return false;
    }

    candidate.keyRoot = std::clamp<int32>(keyRoot, 0, 11);
    candidate.scaleMode = std::clamp<int32>(scaleMode, 0, 2);
    candidate.pitchToKey = pitchToKey != 0;
    candidate.lockPattern = lockPattern != 0;

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

    state_ = candidate;
    syncEngineFromState();
    return true;
}

tresult PLUGIN_API Processor::setState(IBStream* state) {
    return readProjectState(state) ? kResultOk : kResultFalse;
}

tresult PLUGIN_API Processor::getState(IBStream* state) {
    return writeProjectState(state) ? kResultOk : kResultFalse;
}

} // namespace phraseator::vst3
