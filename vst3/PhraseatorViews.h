#pragma once

#include "vstgui/lib/controls/cknob.h"
#include "vstgui/lib/controls/ccontrol.h"
#include "vstgui/lib/cview.h"
#include "vstgui/plugin-bindings/vst3editor.h"
#include "vstgui/uidescription/uiattributes.h"

namespace phraseator::vst3::gui {

class LogoView final : public VSTGUI::CView {
public:
    explicit LogoView(const VSTGUI::CRect& size);
    void draw(VSTGUI::CDrawContext* context) override;
};

class StepIndicator final : public VSTGUI::CControl {
public:
    StepIndicator(const VSTGUI::CRect& size,
                  VSTGUI::IControlListener* listener,
                  std::int32_t tag);
    void draw(VSTGUI::CDrawContext* context) override;
};

class MacroKnob final : public VSTGUI::CKnob {
public:
    MacroKnob(const VSTGUI::CRect& size,
              VSTGUI::IControlListener* listener,
              std::int32_t tag);
    void draw(VSTGUI::CDrawContext* context) override;
};

void configureEditor(VSTGUI::VST3Editor* editor,
                     double width,
                     double height,
                     double zoom);

VSTGUI::CView* createCustomView(VSTGUI::UTF8StringPtr name,
                                const VSTGUI::UIAttributes& attributes,
                                VSTGUI::VST3Editor* editor,
                                VSTGUI::IControlListener* controllerListener);

} // namespace phraseator::vst3::gui
