#pragma once

#include "vstgui/lib/controls/cknob.h"
#include "vstgui/lib/controls/ccontrol.h"
#include "vstgui/lib/controls/coptionmenu.h"
#include "vstgui/lib/cview.h"
#include "vstgui/lib/dragging.h"
#include "vstgui/plugin-bindings/vst3editor.h"
#include "vstgui/uidescription/uiattributes.h"
#include <string>
#include <vector>

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

class StepEditView final : public VSTGUI::CView {
public:
    StepEditView(const VSTGUI::CRect& size,
                 std::int32_t patternViewTag,
                 Controller* controller);

    void draw(VSTGUI::CDrawContext* context) override;
    VSTGUI::CMouseEventResult onMouseDown(
        VSTGUI::CPoint& where,
        const VSTGUI::CButtonState& buttons) override;

private:
    std::int32_t patternViewTag_ {-1};
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

public:
    static bool extractWavePath(VSTGUI::IDataPackage* drag, std::string& path);

private:
    Controller* controller_ {nullptr};
    bool dragActive_ {false};
};

class SourceDropView final :
    public VSTGUI::CView,
    public VSTGUI::IDropTarget {
public:
    SourceDropView(const VSTGUI::CRect& size,
                   std::int32_t sourceStatusTag,
                   Controller* controller);

    void draw(VSTGUI::CDrawContext* context) override;
    VSTGUI::SharedPointer<VSTGUI::IDropTarget> getDropTarget() override { return this; }
    VSTGUI::DragOperation onDragEnter(VSTGUI::DragEventData data) override;
    VSTGUI::DragOperation onDragMove(VSTGUI::DragEventData data) override;
    void onDragLeave(VSTGUI::DragEventData data) override;
    bool onDrop(VSTGUI::DragEventData data) override;

private:
    std::int32_t sourceStatusTag_ {-1};
    Controller* controller_ {nullptr};
    bool dragActive_ {false};
};

class MacroKnob final : public VSTGUI::CKnob {
public:
    MacroKnob(const VSTGUI::CRect& size,
              VSTGUI::IControlListener* listener,
              std::int32_t tag);
    void draw(VSTGUI::CDrawContext* context) override;
    VSTGUI::CMouseEventResult onMouseDown(
        VSTGUI::CPoint& where,
        const VSTGUI::CButtonState& buttons) override;
};

class SelectorView final : public VSTGUI::CControl {
public:
    SelectorView(const VSTGUI::CRect& size,
                 VSTGUI::IControlListener* listener,
                 std::int32_t tag,
                 std::vector<std::string> labels);
    SelectorView(const SelectorView& other);
    VSTGUI::CBaseObject* newCopy() const override { return new SelectorView(*this); }
    void draw(VSTGUI::CDrawContext* context) override;
    VSTGUI::CMouseEventResult onMouseDown(
        VSTGUI::CPoint& where,
        const VSTGUI::CButtonState& buttons) override;
private:
    std::vector<std::string> labels_;
};

class MenuSelectorView final : public VSTGUI::COptionMenu {
public:
    MenuSelectorView(const VSTGUI::CRect& size,
                     VSTGUI::IControlListener* listener,
                     std::int32_t tag,
                     std::vector<std::string> labels);
    MenuSelectorView(const MenuSelectorView& other);
    VSTGUI::CBaseObject* newCopy() const override { return new MenuSelectorView(*this); }
    void draw(VSTGUI::CDrawContext* context) override;
    VSTGUI::CMouseEventResult onMouseDown(
        VSTGUI::CPoint& where,
        const VSTGUI::CButtonState& buttons) override;
};

class ToggleView final : public VSTGUI::CControl {
public:
    ToggleView(const VSTGUI::CRect& size,
               VSTGUI::IControlListener* listener,
               std::int32_t tag,
               std::string offLabel,
               std::string onLabel,
               bool compact=false);
    ToggleView(const ToggleView& other);
    VSTGUI::CBaseObject* newCopy() const override { return new ToggleView(*this); }
    void draw(VSTGUI::CDrawContext* context) override;
    VSTGUI::CMouseEventResult onMouseDown(
        VSTGUI::CPoint& where,
        const VSTGUI::CButtonState& buttons) override;
private:
    std::string offLabel_;
    std::string onLabel_;
    bool compact_ {false};
};

class ActionButton final : public VSTGUI::CControl {
public:
    ActionButton(const VSTGUI::CRect& size,
                 VSTGUI::IControlListener* listener,
                 std::int32_t tag,
                 std::string label,
                 bool compact=false,
                 bool accent=false);
    ActionButton(const ActionButton& other);
    VSTGUI::CBaseObject* newCopy() const override { return new ActionButton(*this); }
    void draw(VSTGUI::CDrawContext* context) override;
    VSTGUI::CMouseEventResult onMouseDown(
        VSTGUI::CPoint& where,
        const VSTGUI::CButtonState& buttons) override;
private:
    std::string label_;
    bool compact_ {false};
    bool accent_ {false};
};

class UIScaleView final : public VSTGUI::CView {
public:
    UIScaleView(const VSTGUI::CRect& size,
                VSTGUI::VST3Editor* editor,
                Controller* controller);
    void draw(VSTGUI::CDrawContext* context) override;
    VSTGUI::CMouseEventResult onMouseDown(
        VSTGUI::CPoint& where,
        const VSTGUI::CButtonState& buttons) override;
private:
    VSTGUI::VST3Editor* editor_ {nullptr};
    Controller* controller_ {nullptr};
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
