#include "PhraseatorController.h"

#include "PhraseatorIDs.h"
#include "../source/parameters.h"
#include "../source/source_pool.h"

#include "base/source/fstreamer.h"
#include "public.sdk/source/vst/vstcomponentbase.h"
#include "PhraseatorViews.h"
#include "base/source/fstring.h"
#include "vstgui/lib/cfileselector.h"
#include "vstgui/lib/controls/ccontrol.h"
#include <cstring>

#include <algorithm>
#include <cmath>

namespace phraseator::vst3 {

using namespace Steinberg;
using namespace Steinberg::Vst;

namespace {
constexpr int32 kLoadOneBase = 2000;
constexpr int32 kLoadLoopBase = 2100;
constexpr int32 kClearBase = 2200;

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

    {
        auto* root = new StringListParameter(
            STR16("Key Root"), pid(ParameterId::KeyRoot));
        root->appendString(STR16("C"));
        root->appendString(STR16("C#"));
        root->appendString(STR16("D"));
        root->appendString(STR16("D#"));
        root->appendString(STR16("E"));
        root->appendString(STR16("F"));
        root->appendString(STR16("F#"));
        root->appendString(STR16("G"));
        root->appendString(STR16("G#"));
        root->appendString(STR16("A"));
        root->appendString(STR16("A#"));
        root->appendString(STR16("B"));
        parameters.addParameter(root);
    }
    {
        auto* scale = new StringListParameter(
            STR16("Scale"), pid(ParameterId::ScaleMode));
        scale->appendString(STR16("Chromatic"));
        scale->appendString(STR16("Major"));
        scale->appendString(STR16("Minor"));
        parameters.addParameter(scale);
    }
    {
        auto* pitchToKey = new StringListParameter(
            STR16("Pitch To Key"), pid(ParameterId::PitchToKey));
        pitchToKey->appendString(STR16("Off"));
        pitchToKey->appendString(STR16("On"));
        parameters.addParameter(pitchToKey);
    }

    parameters.addParameter(STR16("Delay"), STR16("%"), 0, ParameterDefaults::delayAmount,
                            ParameterInfo::kCanAutomate, pid(ParameterId::DelayAmount));
    parameters.addParameter(STR16("Filter"), STR16("%"), 0, ParameterDefaults::filterAmount,
                            ParameterInfo::kCanAutomate, pid(ParameterId::FilterAmount));
    {
        auto* lock = new StringListParameter(
            STR16("Lock Pattern"), pid(ParameterId::LockPattern));
        lock->appendString(STR16("Unlocked"));
        lock->appendString(STR16("Locked"));
        parameters.addParameter(lock);
    }
    parameters.addParameter(STR16("Generate"), nullptr, 1, 0.0,
                            ParameterInfo::kCanAutomate, pid(ParameterId::GenerateTrigger));
    parameters.addParameter(STR16("Variate"), nullptr, 1, 0.0,
                            ParameterInfo::kCanAutomate, pid(ParameterId::VariateTrigger));

    static const TChar* kPatternStepTitles[kPatternViewCount] {
        STR16("Pattern Step 01"), STR16("Pattern Step 02"),
        STR16("Pattern Step 03"), STR16("Pattern Step 04"),
        STR16("Pattern Step 05"), STR16("Pattern Step 06"),
        STR16("Pattern Step 07"), STR16("Pattern Step 08"),
        STR16("Pattern Step 09"), STR16("Pattern Step 10"),
        STR16("Pattern Step 11"), STR16("Pattern Step 12"),
        STR16("Pattern Step 13"), STR16("Pattern Step 14"),
        STR16("Pattern Step 15"), STR16("Pattern Step 16")
    };

    for (int32 i = 0; i < kPatternViewCount; ++i) {
        parameters.addParameter(
            kPatternStepTitles[i], nullptr, kPatternViewStepCount, 0.0,
            ParameterInfo::kIsHidden,
            static_cast<ParamID>(kPatternViewBase + i));
    }

    static const TChar* kSourceStatusTitles[kSourceStatusCount] {
        STR16("Source Status 01"), STR16("Source Status 02"),
        STR16("Source Status 03"), STR16("Source Status 04"),
        STR16("Source Status 05"), STR16("Source Status 06"),
        STR16("Source Status 07"), STR16("Source Status 08")
    };

    for (int32 i = 0; i < kSourceStatusCount; ++i) {
        auto* status = new StringListParameter(
            kSourceStatusTitles[i],
            static_cast<ParamID>(kSourceStatusBase + i),
            nullptr,
            ParameterInfo::kIsHidden);
        status->appendString(STR16("EMPTY"));
        status->appendString(STR16("ONE"));
        status->appendString(STR16("LOOP"));
        parameters.addParameter(status);
    }

    return kResultOk;
}

tresult PLUGIN_API Controller::setState(IBStream* state) {
    if (!state) return kInvalidArgument;
    IBStreamer stream(state, kLittleEndian);
    double zoom = 1.0;
    if (!stream.readDouble(zoom)) return kResultOk;
    guiZoom_ = zoom >= 1.25 ? 1.5 : 1.0;
    if (editor_) editor_->setZoomFactor(guiZoom_);
    return kResultOk;
}

tresult PLUGIN_API Controller::getState(IBStream* state) {
    if (!state) return kInvalidArgument;
    IBStreamer stream(state, kLittleEndian);
    return stream.writeDouble(guiZoom_) ? kResultOk : kResultFalse;
}

IPlugView* PLUGIN_API Controller::createView(FIDString name) {
    if (!name || std::strcmp(name, ViewType::kEditor) != 0) return nullptr;
    auto* editor = new VSTGUI::VST3Editor(this, "view", "Phraseator.uidesc");
    gui::configureEditor(editor, 1040.0, 640.0, guiZoom_);
    editor_ = editor;
    return editor;
}

VSTGUI::CView* Controller::createCustomView(VSTGUI::UTF8StringPtr name,
                                            const VSTGUI::UIAttributes& attributes,
                                            const VSTGUI::IUIDescription*,
                                            VSTGUI::VST3Editor* editor) {
    return gui::createCustomView(name, attributes, editor, this);
}

VSTGUI::CView* Controller::verifyView(VSTGUI::CView* view,
                                      const VSTGUI::UIAttributes&,
                                      const VSTGUI::IUIDescription*,
                                      VSTGUI::VST3Editor* editor) {
    editor_ = editor;
    return view;
}

void Controller::valueChanged(VSTGUI::CControl* control) {
    if (!control || control->getValueNormalized() < 0.5f)
        return;

    const auto tag = control->getTag();

    if (tag == static_cast<Steinberg::int32>(ParameterId::GenerateTrigger) ||
        tag == static_cast<Steinberg::int32>(ParameterId::VariateTrigger)) {
        const auto paramId = static_cast<Steinberg::Vst::ParamID>(tag);

        // Action buttons are momentary commands, not persistent parameters.
        // Send an explicit rising edge followed by reset so the processor
        // receives one deterministic trigger even when the custom button is
        // not managed by VST3Editor's normal parameter binding path.
        beginEdit(paramId);
        setParamNormalized(paramId, 1.0);
        performEdit(paramId, 1.0);
        performEdit(paramId, 0.0);
        setParamNormalized(paramId, 0.0);
        endEdit(paramId);
    } else if (tag >= kLoadOneBase && tag < kLoadOneBase + 8) {
        openSampleSelector(tag - kLoadOneBase, false);
    } else if (tag >= kLoadLoopBase && tag < kLoadLoopBase + 8) {
        openSampleSelector(tag - kLoadLoopBase, true);
    } else if (tag >= kClearBase && tag < kClearBase + 8) {
        sendClearSample(tag - kClearBase);
    }

    control->setValueNormalized(0.0f);
    control->invalid();
}

void Controller::willClose(VSTGUI::VST3Editor* editor) {
    if (editor_ == editor) editor_ = nullptr;
}

void Controller::openSampleSelector(int sourceIndex, bool asLoop) {
    if (!editor_ || sourceIndex < 0 || sourceIndex >= 8) return;
    auto* selector = VSTGUI::CNewFileSelector::create(editor_->getFrame(), VSTGUI::CNewFileSelector::kSelectFile);
    if (!selector) return;
    selector->setTitle(asLoop ? "Load loop" : "Load one-shot");
    selector->addFileExtension(VSTGUI::CFileExtension("WAVE", "wav", "audio/wav"));
    selector->setAllowMultiFileSelection(false);
    selector->run([this, sourceIndex, asLoop](VSTGUI::CNewFileSelector* sel) {
        if (!sel || sel->getNumSelectedFiles() == 0) return;
        const auto* utf8 = sel->getSelectedFile(0);
        if (!utf8 || *utf8 == 0) return;
        Steinberg::String path;
        path.fromUTF8(utf8);
        sendLoadSample(path.text(), sourceIndex, static_cast<uint32>(sourceIndex + 1),
                       asLoop, asLoop ? 16 : 0, false, -1.0);
    });
    selector->forget();
}

tresult PLUGIN_API Controller::setComponentState(IBStream* state) {
    if (!state)
        return kInvalidArgument;

    IBStreamer stream(state, kLittleEndian);

    int32 magic = 0;
    int32 version = 0;
    int32 seed = 0;

    if (!stream.readInt32(magic) || magic != kStateMagic ||
        !stream.readInt32(version) ||
        version < 1 || version > kStateVersion ||
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

    for (int32 i = 0; i < kPatternViewCount; ++i) {
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
            return kResultFalse;
        }

        const auto safeFragment = std::clamp<int32>(
            fragment, 0, kPatternViewStepCount - 1);
        const ParamValue viewValue = active != 0
            ? static_cast<ParamValue>(safeFragment + 1) /
                  static_cast<ParamValue>(kPatternViewStepCount)
            : 0.0;

        setParamNormalized(
            static_cast<ParamID>(kPatternViewBase + i), viewValue);
    }

    return kResultOk;
}

} // namespace phraseator::vst3


namespace phraseator::vst3 {

Steinberg::tresult Controller::sendLoadSample(const Steinberg::Vst::TChar* path,
                                              Steinberg::int32 sourceIndex,
                                              Steinberg::uint32 sourceId,
                                              bool asLoop,
                                              Steinberg::int32 divisions,
                                              bool tonal,
                                              double detectedRootMidi) {
    return sendLoadSampleMode(
        path, sourceIndex, sourceId, asLoop ? 1 : 0, divisions,
        tonal, detectedRootMidi);
}

Steinberg::tresult Controller::sendLoadSampleMode(
    const Steinberg::Vst::TChar* path,
    Steinberg::int32 sourceIndex,
    Steinberg::uint32 sourceId,
    Steinberg::int32 mode,
    Steinberg::int32 divisions,
    bool tonal,
    double detectedRootMidi) {
    using namespace Steinberg;
    using namespace Steinberg::Vst;

    if (!path || *path == 0 ||
        sourceIndex < 0 || sourceIndex >= static_cast<int32>(kMaxSources) ||
        mode < 0 || mode > 2 ||
        divisions < 0 || divisions > static_cast<int32>(kMaxSlicesPerSource)) {
        return kInvalidArgument;
    }

    auto message = owned(allocateMessage());
    if (!message)
        return kResultFalse;

    message->setMessageID(kMsgLoadSample);
    auto* attributes = message->getAttributes();
    if (!attributes)
        return kResultFalse;

    if (attributes->setString(kAttrPath, path) != kResultTrue ||
        attributes->setInt(kAttrSourceIndex, sourceIndex) != kResultTrue ||
        attributes->setInt(kAttrSourceId, static_cast<int64>(sourceId)) != kResultTrue ||
        attributes->setInt(kAttrMode, mode) != kResultTrue ||
        attributes->setInt(kAttrDivisions, divisions) != kResultTrue ||
        attributes->setInt(kAttrTonal, tonal ? 1 : 0) != kResultTrue ||
        attributes->setFloat(kAttrDetectedRootMidi, detectedRootMidi) != kResultTrue) {
        return kResultFalse;
    }

    return sendMessage(message);
}

bool Controller::loadDroppedSample(const std::string& utf8Path,
                                   Steinberg::int32 sourceIndex) {
    if (utf8Path.empty() || sourceIndex < 0 || sourceIndex >= 8)
        return false;

    Steinberg::String path;
    path.fromUTF8(utf8Path.c_str());

    return sendLoadSampleMode(
        path.text(),
        sourceIndex,
        static_cast<Steinberg::uint32>(sourceIndex + 1),
        2,
        16,
        false,
        -1.0) == Steinberg::kResultTrue;
}

Steinberg::tresult Controller::sendClearSample(Steinberg::int32 sourceIndex) {
    using namespace Steinberg;
    using namespace Steinberg::Vst;

    if (sourceIndex < 0 || sourceIndex >= static_cast<int32>(kMaxSources))
        return kInvalidArgument;

    auto message = owned(allocateMessage());
    if (!message)
        return kResultFalse;

    message->setMessageID(kMsgClearSample);
    auto* attributes = message->getAttributes();
    if (!attributes)
        return kResultFalse;

    if (attributes->setInt(kAttrSourceIndex, sourceIndex) != kResultTrue)
        return kResultFalse;

    return sendMessage(message);
}

} // namespace phraseator::vst3
