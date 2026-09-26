#pragma once

#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/vst/vsttypes.h"

namespace phraseator::vst3 {

static const Steinberg::FUID kProcessorUID(0x125A5048, 0x52415345, 0x41544F52, 0x00000001);
static const Steinberg::FUID kControllerUID(0x125A5048, 0x52415345, 0x41544F52, 0x00000002);

constexpr Steinberg::int32 kStateMagic = 0x50485231; // "PHR1"
constexpr Steinberg::int32 kStateVersion = 1;

} // namespace phraseator::vst3
