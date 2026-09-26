#pragma once

#include "public.sdk/source/vst/vsteditcontroller.h"

namespace phraseator::vst3 {

class Controller final : public Steinberg::Vst::EditController {
public:
    static Steinberg::FUnknown* createInstance(void*) {
        return static_cast<Steinberg::Vst::IEditController*>(new Controller());
    }

    Steinberg::tresult PLUGIN_API initialize(Steinberg::FUnknown* context) override;
    Steinberg::tresult PLUGIN_API setComponentState(Steinberg::IBStream* state) override;

    Steinberg::tresult sendLoadSample(const Steinberg::Vst::TChar* path,
                                      Steinberg::int32 sourceIndex,
                                      Steinberg::uint32 sourceId,
                                      bool asLoop,
                                      Steinberg::int32 divisions,
                                      bool tonal = false,
                                      double detectedRootMidi = -1.0);
};

} // namespace phraseator::vst3
