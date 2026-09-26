#include "PhraseatorController.h"

#include "PhraseatorIDs.h"
#include "../source/parameters.h"

#include "base/source/fstreamer.h"

#include <algorithm>
#include <cmath>

namespace phraseator::vst3 {

using namespace Steinberg;
using namespace Steinberg::Vst;

namespace {
ParamID pid(ParameterId id) noexcept {
    return static_cast<ParamID>(id);
}

ParamValue clamp01(ParamValue value) noexcept {
    return std::isfinite(value) ? std::clamp(value, 0.0, 1.0) : 0.0;
}
}

tresult PLUGIN_API Controller::initialize(FUnknown* context) {
    const auto result = EditController::initialize(context);
    if (result != kResultOk)
        return result;

    parameters.addParameter(STR16("Density"), STR16("%"), 0, ParameterDefaults::density,
                            ParameterInfo::kCanAutomate, pid(ParameterId::Density));
    parameters.addParameter(STR16("Variation"), STR16("%"), 0, ParameterDefaults::variation,
                            ParameterInfo::kCanAutomate, pid(ParameterId::Variation));
    parameters.addParameter(STR16("Repeat"), STR16("%"), 0, ParameterDefaults::repeat,
                            ParameterInfo::kCanAutomate, pid(ParameterId::Repeat));
    parameters.addParameter(STR16("Pitch"), STR16("%"), 0, ParameterDefaults::pitch,
                            ParameterInfo::kCanAutomate, pid(ParameterId::Pitch));
    parameters.addParameter(STR16("Pan"), STR16("%"), 0, ParameterDefaults::pan,
                            ParameterInfo::kCanAutomate, pid(ParameterId::Pan));
    parameters.addParameter(STR16("Groove"), STR16("%"), 0, ParameterDefaults::groove,
                            ParameterInfo::kCanAutomate, pid(ParameterId::Groove));

    parameters.addParameter(STR16("Key Root"), nullptr, 11, 0.0,
                            ParameterInfo::kCanAutomate, pid(ParameterId::KeyRoot));
    parameters.addParameter(STR16("Scale"), nullptr, 2, 0.0,
                            ParameterInfo::kCanAutomate, pid(ParameterId::ScaleMode));
    parameters.addParameter(STR16("Pitch To Key"), nullptr, 1, 0.0,
                            ParameterInfo::kCanAutomate, pid(ParameterId::PitchToKey));

    parameters.addParameter(STR16("Delay"), STR16("%"), 0, ParameterDefaults::delayAmount,
                            ParameterInfo::kCanAutomate, pid(ParameterId::DelayAmount));
    parameters.addParameter(STR16("Reverb"), STR16("%"), 0, ParameterDefaults::reverbAmount,
                            ParameterInfo::kCanAutomate, pid(ParameterId::ReverbAmount));
    parameters.addParameter(STR16("Drive"), STR16("%"), 0, ParameterDefaults::driveAmount,
                            ParameterInfo::kCanAutomate, pid(ParameterId::DriveAmount));
    parameters.addParameter(STR16("Filter"), STR16("%"), 0, ParameterDefaults::filterAmount,
                            ParameterInfo::kCanAutomate, pid(ParameterId::FilterAmount));
    parameters.addParameter(STR16("Lock Pattern"), nullptr, 1, 0.0,
                            ParameterInfo::kCanAutomate, pid(ParameterId::LockPattern));

    return kResultOk;
}

tresult PLUGIN_API Controller::setComponentState(IBStream* state) {
    if (!state)
        return kInvalidArgument;

    IBStreamer stream(state, kLittleEndian);

    int32 magic = 0;
    int32 version = 0;
    int32 seed = 0;

    if (!stream.readInt32(magic) || magic != kStateMagic ||
        !stream.readInt32(version) || version != kStateVersion ||
        !stream.readInt32(seed)) {
        return kResultFalse;
    }

    double values[10] {};
    for (double& value : values) {
        if (!stream.readDouble(value) || !std::isfinite(value))
            return kResultFalse;
        value = clamp01(value);
    }

    int32 keyRoot = 0;
    int32 scaleMode = 0;
    int32 pitchToKey = 0;
    int32 lockPattern = 0;

    if (!stream.readInt32(keyRoot) ||
        !stream.readInt32(scaleMode) ||
        !stream.readInt32(pitchToKey) ||
        !stream.readInt32(lockPattern)) {
        return kResultFalse;
    }

    setParamNormalized(pid(ParameterId::Density), values[0]);
    setParamNormalized(pid(ParameterId::Variation), values[1]);
    setParamNormalized(pid(ParameterId::Repeat), values[2]);
    setParamNormalized(pid(ParameterId::Pitch), values[3]);
    setParamNormalized(pid(ParameterId::Pan), values[4]);
    setParamNormalized(pid(ParameterId::Groove), values[5]);
    setParamNormalized(pid(ParameterId::DelayAmount), values[6]);
    setParamNormalized(pid(ParameterId::ReverbAmount), values[7]);
    setParamNormalized(pid(ParameterId::DriveAmount), values[8]);
    setParamNormalized(pid(ParameterId::FilterAmount), values[9]);

    setParamNormalized(pid(ParameterId::KeyRoot),
        static_cast<double>(std::clamp<int32>(keyRoot, 0, 11)) / 11.0);
    setParamNormalized(pid(ParameterId::ScaleMode),
        static_cast<double>(std::clamp<int32>(scaleMode, 0, 2)) / 2.0);
    setParamNormalized(pid(ParameterId::PitchToKey), pitchToKey != 0 ? 1.0 : 0.0);
    setParamNormalized(pid(ParameterId::LockPattern), lockPattern != 0 ? 1.0 : 0.0);

    return kResultOk;
}

} // namespace phraseator::vst3
