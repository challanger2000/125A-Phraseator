#pragma once

#include "vstgui/lib/controls/cknob.h"
#include "vstgui/lib/controls/ccontrol.h"
#include "vstgui/lib/cview.h"
#include "vstgui/lib/dragging.h"
#include "vstgui/plugin-bindings/vst3editor.h"
#include "vstgui/uidescription/uiattributes.h"
#include <string>

namespace phraseator::vst3 {
class Controller;
}

namespace phraseator::vst3::gui {

class FaceplateView final : public VSTGUI::CView {
public:
    explicit FaceplateView(const VSTGUI::CRect& size);
    void draw(VSTGUI::CDrawContext* context) override;
};

class LogoView final : public VSTGUI::CView {
public:
    explicit LogoView(const VSTGUI::CRect& size);
    void draw(VSTGUI::CDrawContext* context) override;
};

class StepIndicator final : public VSTGUI::CControl {
public:
    StepIndicator(const VSTGUI::CRect& size,
                  VSTGUI::IControlListener* listener,
                  std::int32_t tag,
                  Controller* controller);
    void draw(VSTGUI::CDrawContext* context) override;
    VSTGUI::CMouseEventResult onMouseDown(
        VSTGUI::CPoint& where,
        const VSTGUI::CButtonState& buttons) override;
    CLASS_METHODS(StepIndicator, VSTGUI::CControl)

private:
    Controller* controller_ {nullptr};
};

class RatchetView final : public VSTGUI::CControl {
public:
    RatchetView(const VSTGUI::CRect& size,
                VSTGUI::IControlListener* listener,
                std::int32_t tag);
    void draw(VSTGUI::CDrawContext* context) override;
    VSTGUI::CMouseEventResult onMouseDown(
        VSTGUI::CPoint& where,
        const VSTGUI::CButtonState& buttons) override;
    CLASS_METHODS(RatchetView, VSTGUI::CControl)
};

class SourceSlotView final :
    public VSTGUI::CControl,
    public VSTGUI::IDropTarget {
public:
    SourceSlotView(const VSTGUI::CRect& size,
                   VSTGUI::IControlListener* listener,
                   std::int32_t tag,
                   Controller* controller);

    void draw(VSTGUI::CDrawContext* context) override;
    VSTGUI::SharedPointer<VSTGUI::IDropTarget> getDropTarget() override { return this; }
    VSTGUI::DragOperation onDragEnter(VSTGUI::DragEventData data) override;
    VSTGUI::DragOperation onDragMove(VSTGUI::DragEventData data) override;
    void onDragLeave(VSTGUI::DragEventData data) override;
    bool onDrop(VSTGUI::DragEventData data) override;
    CLASS_METHODS(SourceSlotView, VSTGUI::CControl)

private:
    static bool extractWavePath(VSTGUI::IDataPackage* drag, std::string& path);
    Controller* controller_ {nullptr};
    bool dragActive_ {false};
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
                                Controller* controller);

} // namespace phraseator::vst3::gui
