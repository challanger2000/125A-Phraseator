#pragma once

#include "public.sdk/source/vst/vstaudioeffect.h"

#include "../source/phrase_engine.h"
#include "../source/phrase_scheduler.h"
#include "../source/project_state.h"
#include "../source/source_pool.h"

#include <array>
#include <cstdint>

namespace phraseator::vst3 {

class Processor final : public Steinberg::Vst::AudioEffect {
public:
    Processor();

    static Steinberg::FUnknown* createInstance(void*) {
        return static_cast<Steinberg::Vst::IAudioProcessor*>(new Processor());
    }

    Steinberg::tresult PLUGIN_API initialize(Steinberg::FUnknown* context) override;
    Steinberg::tresult PLUGIN_API setBusArrangements(
        Steinberg::Vst::SpeakerArrangement* inputs,
        Steinberg::int32 numIns,
        Steinberg::Vst::SpeakerArrangement* outputs,
        Steinberg::int32 numOuts) override;
    Steinberg::tresult PLUGIN_API canProcessSampleSize(
        Steinberg::int32 symbolicSampleSize) override;
    Steinberg::tresult PLUGIN_API setupProcessing(
        Steinberg::Vst::ProcessSetup& setup) override;
    Steinberg::tresult PLUGIN_API setActive(Steinberg::TBool state) override;
    Steinberg::tresult PLUGIN_API setProcessing(Steinberg::TBool state) override;
    Steinberg::tresult PLUGIN_API process(Steinberg::Vst::ProcessData& data) override;
    Steinberg::tresult PLUGIN_API setState(Steinberg::IBStream* state) override;
    Steinberg::tresult PLUGIN_API getState(Steinberg::IBStream* state) override;

private:
    void readParameterChanges(Steinberg::Vst::IParameterChanges* changes) noexcept;
    void applyNormalizedParameter(Steinberg::Vst::ParamID id, double value) noexcept;
    bool readProjectState(Steinberg::IBStream* stream) noexcept;
    bool writeProjectState(Steinberg::IBStream* stream) const noexcept;
    void syncEngineFromState() noexcept;

    ProjectState state_ {};
    PhraseEngine engine_ {state_.randomSeed};
    PhraseScheduler scheduler_ {};
    SourcePool sourcePool_ {};
    std::array<AudioBufferView, kMaxSources> buffers_ {};

    double sampleRate_ {48000.0};
    double fallbackProjectTimeSamples_ {0.0};
    bool active_ {false};
    bool processing_ {false};
};

} // namespace phraseator::vst3
