#include "public.sdk/source/main/pluginfactory.h"

#include "PhraseatorController.h"
#include "PhraseatorIDs.h"
#include "PhraseatorProcessor.h"

#define stringPluginName "125A Phraseator"
#define stringPluginVersion "0.1.0"

BEGIN_FACTORY_DEF(
    "125A",
    "https://github.com/challanger2000/125A-Phraseator",
    "")

DEF_CLASS2(
    INLINE_UID_FROM_FUID(phraseator::vst3::kProcessorUID),
    Steinberg::PClassInfo::kManyInstances,
    kVstAudioEffectClass,
    stringPluginName,
    Steinberg::Vst::kDistributable,
    "Instrument|Sampler",
    stringPluginVersion,
    kVstVersionString,
    phraseator::vst3::Processor::createInstance)

DEF_CLASS2(
    INLINE_UID_FROM_FUID(phraseator::vst3::kControllerUID),
    Steinberg::PClassInfo::kManyInstances,
    kVstComponentControllerClass,
    "125A Phraseator Controller",
    0,
    "",
    stringPluginVersion,
    kVstVersionString,
    phraseator::vst3::Controller::createInstance)

END_FACTORY
