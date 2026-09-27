#pragma once

#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/vst/vsttypes.h"

namespace phraseator::vst3 {

static const Steinberg::FUID kProcessorUID(0x125A5048, 0x52415345, 0x41544F52, 0x00000001);
static const Steinberg::FUID kControllerUID(0x125A5048, 0x52415345, 0x41544F52, 0x00000002);

constexpr Steinberg::int32 kStateMagic = 0x50485231; // "PHR1"
constexpr Steinberg::int32 kStateVersion = 9;

constexpr Steinberg::Vst::ParamID kSourceStatusBase = 1500;
constexpr Steinberg::int32 kSourceStatusCount = 8;
constexpr Steinberg::Vst::ParamID kSourceMuteBase = 1600;
constexpr Steinberg::int32 kSourceMuteCount = 8;

constexpr Steinberg::Vst::ParamID kPatternViewBase = 1400;
constexpr Steinberg::int32 kPatternViewCount = 16;
constexpr Steinberg::int32 kPatternViewStepCount = kSourceStatusCount;

constexpr const char* kMsgLoadSample = "Phraseator.LoadSample";
constexpr const char* kMsgClearSample = "Phraseator.ClearSample";
constexpr const char* kMsgGenerate = "Phraseator.Generate";
constexpr const char* kMsgVariate = "Phraseator.Variate";
constexpr const char* kMsgPatternStep = "Phraseator.PatternStep";
constexpr const char* kAttrStepIndex = "StepIndex";
constexpr const char* kAttrStepActive = "StepActive";
constexpr const char* kAttrStepSource = "StepSource";
constexpr const char* kAttrPath = "Path";
constexpr const char* kAttrSourceIndex = "SourceIndex";
constexpr const char* kAttrSourceId = "SourceId";
constexpr const char* kAttrMode = "Mode";
constexpr const char* kAttrDivisions = "Divisions";
constexpr const char* kAttrTonal = "Tonal";
constexpr const char* kAttrDetectedRootMidi = "DetectedRootMidi";

} // namespace phraseator::vst3
