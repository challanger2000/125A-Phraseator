#pragma once

#include "public.sdk/source/vst/vsteditcontroller.h"
#include "vstgui/plugin-bindings/vst3editor.h"
#include "vstgui/lib/controls/icontrollistener.h"

namespace phraseator::vst3 {

class Controller final :
    public Steinberg::Vst::EditControllerEx1,
    public VSTGUI::VST3EditorDelegate,
    public VSTGUI::IControlListener {
public:
    static Steinberg::FUnknown* createInstance(void*) {
        return static_cast<Steinberg::Vst::IEditController*>(new Controller());
    }

    Steinberg::tresult PLUGIN_API initialize(Steinberg::FUnknown* context) override;
    Steinberg::tresult PLUGIN_API setComponentState(Steinberg::IBStream* state) override;
    Steinberg::tresult PLUGIN_API setState(Steinberg::IBStream* state) override;
    Steinberg::tresult PLUGIN_API getState(Steinberg::IBStream* state) override;
    Steinberg::IPlugView* PLUGIN_API createView(Steinberg::FIDString name) override;

    VSTGUI::CView* createCustomView(
        VSTGUI::UTF8StringPtr name,
        const VSTGUI::UIAttributes& attributes,
        const VSTGUI::IUIDescription* description,
        VSTGUI::VST3Editor* editor) override;

    VSTGUI::CView* verifyView(
        VSTGUI::CView* view,
        const VSTGUI::UIAttributes& attributes,
        const VSTGUI::IUIDescription* description,
        VSTGUI::VST3Editor* editor) override;

    void valueChanged(VSTGUI::CControl* control) override;
    void willClose(VSTGUI::VST3Editor* editor) override;

    Steinberg::tresult sendLoadSample(const Steinberg::Vst::TChar* path,
                                      Steinberg::int32 sourceIndex,
                                      Steinberg::uint32 sourceId,
                                      bool asLoop,
                                      Steinberg::int32 divisions,
                                      bool tonal = false,
                                      double detectedRootMidi = -1.0);

private:
    void openSampleSelector(int sourceIndex, bool asLoop);

    double guiZoom_ {1.0};
    VSTGUI::VST3Editor* editor_ {nullptr};
};

} // namespace phraseator::vst3
