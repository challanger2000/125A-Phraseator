#pragma once

#include "public.sdk/source/vst/vstaudioeffect.h"
#include "pluginterfaces/vst/ivstevents.h"

#include "../source/phrase_engine.h"
#include "../source/phrase_scheduler.h"
#include "../source/phrase_fx.h"
#include "../source/project_state.h"
#include "../source/pattern_fragment_remap.h"
#include "../source/sample_bank.h"
#include "../source/sample_load_worker.h"
#include "../source/midi_note_tracker.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>

namespace phraseator::vst3 {

class Processor final : public Steinberg::Vst::AudioEffect {
public:
    Processor();

    static Steinberg::FUnknown* createInstance(void*) {
        return static_cast<Steinberg::Vst::IAudioProcessor*>(new Processor());
    }

    Steinberg::tresult PLUGIN_API initialize(Steinberg::FUnknown* context) override;
    Steinberg::tresult PLUGIN_API terminate() override;
    Steinberg::tresult PLUGIN_API notify(Steinberg::Vst::IMessage* message) override;
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
    GenerationSettings currentGenerationSettings() const noexcept;
    void generatePattern() noexcept;
    void varyPattern() noexcept;
    void applyPitchToKey(Pattern& pattern) noexcept;
    void queueRecallLoads() noexcept;
    void emitPatternViewParameters(Steinberg::Vst::ProcessData& data,
                                   Steinberg::int32 sampleOffset) noexcept;
    void emitSourceStatusParameters(Steinberg::Vst::ProcessData& data,
                                    Steinberg::int32 sampleOffset) noexcept;
    bool patternHasActiveSteps() const noexcept;
    void refreshSchedulerPattern() noexcept;
    void handleMidiEvent(const Steinberg::Vst::Event& event) noexcept;

    struct SourceRecallEntry {
        bool occupied {false};
        std::uint32_t sourceId {0};
        SampleLoadMode mode {SampleLoadMode::OneShot};
        std::uint16_t divisions {0};
        bool preferTransient {false};
        std::uint16_t resolvedSliceCount {0};
        std::array<SliceRegion, kMaxSlicesPerSource> resolvedSlices {};
        bool tonal {false};
        float detectedRootMidi {-1.0f};
        std::string utf8Path;
    };

    ProjectState state_ {};
    PhraseEngine engine_ {state_.randomSeed};
    PhraseScheduler scheduler_ {};
    PhraseFx fx_ {};
    SampleBankExchange sampleBanks_ {};
    std::unique_ptr<SampleLoadWorker> sampleLoader_;
    std::array<SourceRecallEntry, kMaxSources> sourceRecall_ {};
    mutable std::mutex sourceRecallMutex_;
    std::atomic<std::uint64_t> sourceRecallEpoch_ {1u};
    std::atomic<std::uint64_t> recallLoadRequestId_ {0u};
    std::atomic<bool> recallAudioPending_ {false};
    PatternFragmentSnapshot recallPatternSnapshot_ {};
    std::atomic<bool> recallPatternRemapPending_ {false};

    double sampleRate_ {48000.0};
    double fallbackProjectTimeSamples_ {0.0};
    bool active_ {false};
    bool processing_ {false};
    double generateTrigger_ {0.0};
    double variateTrigger_ {0.0};
    bool patternViewDirty_ {true};
    bool sourceStatusDirty_ {true};
    MidiNoteTracker heldMidiNotes_ {};
    int activeMidiNote_ {-1};
    float midiTransposeSemitones_ {0.0f};
    double midiPhraseTimeSamples_ {0.0};
    std::atomic<bool> generateCommandPending_ {false};
    std::atomic<bool> variateCommandPending_ {false};
    std::array<std::atomic<int>, kStepCount> patternEditPending_ {};
};

} // namespace phraseator::vst3
