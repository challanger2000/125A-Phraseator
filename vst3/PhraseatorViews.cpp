#include "PhraseatorViews.h"
#include "PhraseatorController.h"
#include "PhraseatorIDs.h"
#include "branding_master.h"

#include "vstgui/lib/cdrawcontext.h"
#include "vstgui/lib/cgraphicspath.h"
#include "vstgui/lib/controls/cbuttons.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace phraseator::vst3::gui {
namespace {

constexpr VSTGUI::CColor kSilver {217,217,217,255};
constexpr VSTGUI::CColor kRed {215,25,32,255};
constexpr VSTGUI::CColor kAccent {86,154,220,255};
constexpr VSTGUI::CColor kText {239,243,247,255};
constexpr VSTGUI::CColor kTick {126,139,153,180};
constexpr double kPi = 3.14159265358979323846;

bool resetToDefaultOnCtrlClick(VSTGUI::CControl* control,
                               const VSTGUI::CButtonState& buttons) {
    if (!control || !buttons.isLeftButton() || !buttons.isControlSet())
        return false;

    control->beginEdit();
    control->setValue(control->getDefaultValue());
    control->valueChanged();
    control->endEdit();
    control->invalid();
    return true;
}

struct LogoSubpath { std::vector<VSTGUI::CPoint> points; };
struct LogoPath { std::vector<LogoSubpath> subpaths; bool red {false}; };

std::vector<LogoPath> parseLogo() {
    std::vector<LogoPath> result;
    result.reserve(branding::kMasterPathCount);
    for (const auto& src : branding::kMasterPaths) {
        LogoPath path;
        path.red = src.red;
        std::string_view d{src.d};
        const char* p=d.data(); const char* end=d.data()+d.size();
        char command=0; LogoSubpath* current=nullptr;
        while (p<end) {
            while (p<end && (std::isspace(static_cast<unsigned char>(*p)) || *p==',')) ++p;
            if (p>=end) break;
            if (std::isalpha(static_cast<unsigned char>(*p))) {
                command=*p++;
                if (command=='Z' || command=='z') { current=nullptr; command=0; continue; }
            }
            if (command!='M' && command!='L' && command!='m' && command!='l') { ++p; continue; }
            char* next=nullptr;
            double x=std::strtod(p,&next); if (next==p) break; p=next;
            while (p<end && (std::isspace(static_cast<unsigned char>(*p)) || *p==',')) ++p;
            double y=std::strtod(p,&next); if (next==p) break; p=next;
            if (command=='M' || command=='m') {
                path.subpaths.emplace_back();
                current=&path.subpaths.back();
                current->points.emplace_back(x,y);
                command=(command=='M')?'L':'l';
            } else if (current) current->points.emplace_back(x,y);
        }
        if (!path.subpaths.empty()) result.emplace_back(std::move(path));
    }
    return result;
}

}


FaceplateView::FaceplateView(const VSTGUI::CRect& size) : CView(size) {
    setMouseEnabled(false);
}

void FaceplateView::draw(VSTGUI::CDrawContext* context) {
    const auto r = getViewSize();
    const double ox = r.left;
    const double oy = r.top;
    const auto rect = [&](double x,double y,double w,double h) {
        return VSTGUI::CRect(ox+x,oy+y,ox+x+w,oy+y+h);
    };
    const auto line = [&](double x1,double y1,double x2,double y2,
                          const VSTGUI::CColor& color,double width=1.0) {
        context->setFrameColor(color);
        context->setLineWidth(width);
        context->drawLine({ox+x1,oy+y1},{ox+x2,oy+y2});
    };
    const auto panel = [&](double x,double y,double w,double h) {
        const auto rr=rect(x,y,w,h);
        context->setFillColor({15,20,27,255});
        context->drawRect(rr,VSTGUI::kDrawFilled);
        context->setFrameColor({59,72,87,215});
        context->setLineWidth(1.0);
        context->drawRect(rr,VSTGUI::kDrawStroked);
    };
    const auto well = [&](double x,double y,double w,double h) {
        const auto shadow=rect(x+1,y+2,w,h);
        context->setFillColor({3,5,8,220});
        context->drawRect(shadow,VSTGUI::kDrawFilled);
        const auto rr=rect(x,y,w,h);
        context->setFillColor({8,12,17,255});
        context->drawRect(rr,VSTGUI::kDrawFilled);
        context->setFrameColor({48,61,75,230});
        context->setLineWidth(1.0);
        context->drawRect(rr,VSTGUI::kDrawStroked);
    };
    const auto screw = [&](double x,double y) {
        const auto sr=rect(x-3.5,y-3.5,7,7);
        context->setFillColor({70,77,87,255});
        context->setFrameColor({6,8,10,255});
        context->drawEllipse(sr,VSTGUI::kDrawFilledAndStroked);
        line(x-1.8,y,x+1.8,y,{8,10,13,230},1.0);
    };

    context->setDrawMode(VSTGUI::kAntiAliasing);
    context->setFillColor({5,8,12,255});
    context->drawRect(r,VSTGUI::kDrawFilled);

    const auto chassis=rect(8,8,1024,504);
    context->setFillColor({20,26,34,255});
    context->setFrameColor({3,5,7,255});
    context->setLineWidth(2.0);
    context->drawRect(chassis,VSTGUI::kDrawFilledAndStroked);

    for(int y=14;y<508;y+=5)
        line(12,y,1028,y,{205,216,227,static_cast<uint8_t>((y%20)==0?9:3)},1.0);

    panel(20,16,1000,52);
    line(30,72,1010,72,{86,154,220,76},1.3);

    panel(20,84,300,210);
    panel(332,84,688,210);

    // Lower half is intentionally compact after checking the real 150% host view.
    panel(20,306,650,190);
    panel(682,306,338,88);
    panel(682,406,338,90);

    // Larger, simpler 4+4 source bays.
    constexpr double px[4] = {32,104,176,248};
    for(int row=0;row<2;++row)
        for(int col=0;col<4;++col)
            well(px[col],124+row*82,64,78);

    // Compact step pads: wider than before, less vertical "fader slot" appearance.
    for(int i=0;i<16;++i) {
        const double x=350+i*39.0;
        well(x,130,34,50);
        if(i>0 && i%4==0)
            line(x-7,124,x-7,186,{86,154,220,72},1.0);
    }

    well(350,204,638,70);

    line(36,342,654,342,{86,154,220,60},1.0);
    // Clear functional split: structural generation controls vs live playback shaping.
    line(292,350,292,478,{86,154,220,72},1.0);
    line(698,342,1004,342,{86,154,220,50},1.0);
    line(698,438,1004,438,{86,154,220,50},1.0);

    for(auto p : {VSTGUI::CPoint{16,16},VSTGUI::CPoint{1024,16},
                  VSTGUI::CPoint{16,504},VSTGUI::CPoint{1024,504},
                  VSTGUI::CPoint{28,92},VSTGUI::CPoint{312,92},
                  VSTGUI::CPoint{340,92},VSTGUI::CPoint{1012,92},
                  VSTGUI::CPoint{28,488},VSTGUI::CPoint{662,488},
                  VSTGUI::CPoint{690,488},VSTGUI::CPoint{1012,488}})
        screw(p.x,p.y);

    setDirty(false);
}


LogoView::LogoView(const VSTGUI::CRect& size) : CView(size) {
    setMouseEnabled(false);
}

void LogoView::draw(VSTGUI::CDrawContext* context) {
    static const auto logo=parseLogo();
    const auto r=getViewSize();
    constexpr double mw=1774.0, mh=887.0;
    const double scale=std::min(r.getWidth()/mw,r.getHeight()/mh);
    const double ox=r.left+(r.getWidth()-mw*scale)*0.5;
    const double oy=r.top +(r.getHeight()-mh*scale)*0.5;
    context->setDrawMode(VSTGUI::kAntiAliasing);
    for (const auto& lp:logo) {
        auto* gp=context->createGraphicsPath(); if(!gp) continue;
        for (const auto& sp:lp.subpaths) {
            if(sp.points.empty()) continue;
            auto tr=[&](const VSTGUI::CPoint& q){return VSTGUI::CPoint{ox+q.x*scale,oy+q.y*scale};};
            gp->beginSubpath(tr(sp.points.front()));
            for(std::size_t i=1;i<sp.points.size();++i) gp->addLine(tr(sp.points[i]));
            gp->closeSubpath();
        }
        context->setFillColor(lp.red?kRed:kSilver);
        context->drawGraphicsPath(gp,VSTGUI::CDrawContext::kPathFilledEvenOdd);
        gp->forget();
    }
    setDirty(false);
}


StepIndicator::StepIndicator(const VSTGUI::CRect& size,
                             VSTGUI::IControlListener* listener,
                             std::int32_t tag,
                             Controller* controller)
: VSTGUI::CControl(size, listener, tag),
  controller_(controller) {
    setMouseEnabled(true);
    setTransparency(true);
}

void StepIndicator::draw(VSTGUI::CDrawContext* context) {
    const auto r = getViewSize();
    const double normalized =
        std::clamp(static_cast<double>(getValueNormalized()), 0.0, 1.0);
    const bool active = normalized > 0.0;

    const auto stepIndex = std::clamp<std::int32_t>(
        getTag() - static_cast<std::int32_t>(kPatternViewBase), 0, 15);
    const bool strongBeat = (stepIndex % 4) == 0;

    context->setDrawMode(VSTGUI::kAntiAliasing);
    context->setFillColor(
        active ? VSTGUI::CColor{50, 105, 156, 255}
               : (strongBeat ? VSTGUI::CColor{24, 31, 40, 255}
                             : VSTGUI::CColor{13, 18, 24, 255}));
    context->setFrameColor(
        active ? VSTGUI::CColor{111, 181, 239, 255}
               : VSTGUI::CColor{52, 65, 79, 255});
    context->setLineWidth(active ? 1.6 : 1.0);
    context->drawRect(r, VSTGUI::kDrawFilledAndStroked);

    if (active) {
        const int sourceIndex = std::clamp(
            static_cast<int>(std::lround(normalized * kPatternViewStepCount)) - 1,
            0, kPatternViewStepCount - 1);
        char text[8] {};
        std::snprintf(text, sizeof(text), "%d", sourceIndex + 1);
        context->setFont(VSTGUI::kNormalFontSmall);
        context->setFontColor({240, 245, 250, 255});
        context->drawString(text, r, VSTGUI::kCenterText);
    }

    setDirty(false);
}

VSTGUI::CMouseEventResult StepIndicator::onMouseDown(
    VSTGUI::CPoint&,
    const VSTGUI::CButtonState& buttons) {

    if (!controller_)
        return VSTGUI::kMouseEventNotHandled;

    const auto stepIndex = getTag() -
        static_cast<std::int32_t>(kPatternViewBase);

    if (buttons.isRightButton()) {
        controller_->sendPatternStepEdit(stepIndex, false, 0);
        return VSTGUI::kMouseDownEventHandledButDontNeedMovedOrUpEvents;
    }

    if (!buttons.isLeftButton())
        return VSTGUI::kMouseEventNotHandled;

    const double normalized =
        std::clamp(static_cast<double>(getValueNormalized()), 0.0, 1.0);
    const int currentSource = normalized > 0.0
        ? std::clamp(
            static_cast<int>(std::lround(normalized * kPatternViewStepCount)) - 1,
            0, kPatternViewStepCount - 1)
        : -1;

    int nextSource = -1;

    // Cycle forward only. Reaching the end clears the step; the next click
    // starts again at the first loaded/unmuted source. This makes deletion
    // available with ordinary left-click and avoids host-specific right-click
    // interception by VST3Editor context menus.
    const int begin = currentSource >= 0 ? currentSource + 1 : 0;
    for (int source = begin; source < kSourceStatusCount; ++source) {
        const auto status = controller_->getParamNormalized(
            static_cast<Steinberg::Vst::ParamID>(kSourceStatusBase + source));
        const auto muted = controller_->getParamNormalized(
            static_cast<Steinberg::Vst::ParamID>(kSourceMuteBase + source));

        if (status > 0.0 && muted < 0.5) {
            nextSource = source;
            break;
        }
    }

    if (nextSource >= 0)
        controller_->sendPatternStepEdit(stepIndex, true, nextSource);
    else
        controller_->sendPatternStepEdit(stepIndex, false, 0);

    return VSTGUI::kMouseDownEventHandledButDontNeedMovedOrUpEvents;
}

RatchetView::RatchetView(const VSTGUI::CRect& size,
                         VSTGUI::IControlListener* listener,
                         std::int32_t tag)
: VSTGUI::CControl(size, listener, tag) {
    setMouseEnabled(true);
    setTransparency(true);
}

void RatchetView::draw(VSTGUI::CDrawContext* context) {
    const auto r = getViewSize();
    const int hits = std::clamp(
        static_cast<int>(std::lround(getValueNormalized() * 3.0)) + 1,
        1, 4);

    context->setDrawMode(VSTGUI::kAntiAliasing);
    context->setFillColor(hits > 1
        ? VSTGUI::CColor{36, 73, 107, 255}
        : VSTGUI::CColor{10, 15, 21, 255});
    context->setFrameColor(hits > 1
        ? VSTGUI::CColor{86, 154, 220, 255}
        : VSTGUI::CColor{52, 65, 79, 255});
    context->setLineWidth(1.0);
    context->drawRect(r, VSTGUI::kDrawFilledAndStroked);

    char label[8] {};
    std::snprintf(label, sizeof(label), "%dx", hits);
    context->setFont(VSTGUI::kNormalFontSmall);
    context->setFontColor(kText);
    context->drawString(label, r, VSTGUI::kCenterText);
    setDirty(false);
}

VSTGUI::CMouseEventResult RatchetView::onMouseDown(
    VSTGUI::CPoint&,
    const VSTGUI::CButtonState& buttons) {

    if (resetToDefaultOnCtrlClick(this, buttons))
        return VSTGUI::kMouseDownEventHandledButDontNeedMovedOrUpEvents;

    if (!buttons.isLeftButton())
        return VSTGUI::kMouseEventNotHandled;

    const int current = std::clamp(
        static_cast<int>(std::lround(getValueNormalized() * 3.0)) + 1,
        1, 4);
    const int next = current >= 4 ? 1 : current + 1;

    beginEdit();
    setValueNormalized(static_cast<float>(next - 1) / 3.0f);
    valueChanged();
    endEdit();
    invalid();

    return VSTGUI::kMouseDownEventHandledButDontNeedMovedOrUpEvents;
}

SourceSlotView::SourceSlotView(const VSTGUI::CRect& size,
                               VSTGUI::IControlListener* listener,
                               std::int32_t tag,
                               Controller* controller)
: VSTGUI::CControl(size, listener, tag),
  controller_(controller) {
    setTransparency(true);
}

bool SourceSlotView::extractWavePath(VSTGUI::IDataPackage* drag,
                                     std::string& path) {
    if (!drag)
        return false;

    const auto normalizeCandidate = [](std::string candidate) -> std::string {
        while (!candidate.empty() &&
               (candidate.back() == '\0' ||
                candidate.back() == '\r' ||
                candidate.back() == '\n')) {
            candidate.pop_back();
        }

        const auto first = candidate.find_first_not_of(" \t\r\n");
        const auto last = candidate.find_last_not_of(" \t\r\n");
        if (first == std::string::npos)
            return {};
        candidate = candidate.substr(first, last - first + 1u);

        if (candidate.size() >= 2u &&
            ((candidate.front() == '"' && candidate.back() == '"') ||
             (candidate.front() == '\'' && candidate.back() == '\''))) {
            candidate = candidate.substr(1u, candidate.size() - 2u);
        }

        constexpr std::string_view filePrefix = "file:///";
        if (candidate.size() >= filePrefix.size()) {
            bool hasFilePrefix = true;
            for (std::size_t i = 0; i < filePrefix.size(); ++i) {
                if (std::tolower(static_cast<unsigned char>(candidate[i])) !=
                    filePrefix[i]) {
                    hasFilePrefix = false;
                    break;
                }
            }

            if (hasFilePrefix) {
                candidate.erase(0u, filePrefix.size());
                std::replace(candidate.begin(), candidate.end(), '/', '\\');

                // Decode the small set of URL escapes commonly produced by
                // Windows/host browser file URLs. Unknown escapes are kept.
                std::string decoded;
                decoded.reserve(candidate.size());

                const auto hexValue = [](char ch) noexcept -> int {
                    if (ch >= '0' && ch <= '9') return ch - '0';
                    if (ch >= 'a' && ch <= 'f') return 10 + ch - 'a';
                    if (ch >= 'A' && ch <= 'F') return 10 + ch - 'A';
                    return -1;
                };

                for (std::size_t i = 0; i < candidate.size(); ++i) {
                    if (candidate[i] == '%' && i + 2u < candidate.size()) {
                        const int hi = hexValue(candidate[i + 1u]);
                        const int lo = hexValue(candidate[i + 2u]);
                        if (hi >= 0 && lo >= 0) {
                            decoded.push_back(
                                static_cast<char>((hi << 4) | lo));
                            i += 2u;
                            continue;
                        }
                    }
                    decoded.push_back(candidate[i]);
                }

                candidate = std::move(decoded);
            }
        }

        return candidate;
    };

    const auto isWavePath = [](const std::string& candidate) {
        if (candidate.empty())
            return false;

        std::string lower = candidate;
        std::transform(lower.begin(), lower.end(), lower.begin(),
                       [](unsigned char ch) {
                           return static_cast<char>(std::tolower(ch));
                       });

        if (lower.size() < 4u ||
            lower.compare(lower.size() - 4u, 4u, ".wav") != 0) {
            return false;
        }

        std::error_code ec;
        const auto fsPath = std::filesystem::u8path(candidate);
        return std::filesystem::is_regular_file(fsPath, ec) && !ec;
    };

    const auto count = drag->getCount();
    for (std::uint32_t i = 0; i < count; ++i) {
        const auto advertisedType = drag->getDataType(i);
        if (advertisedType != VSTGUI::IDataPackage::kFilePath &&
            advertisedType != VSTGUI::IDataPackage::kText) {
            continue;
        }

        const void* buffer = nullptr;
        VSTGUI::IDataPackage::Type type {};
        const auto size = drag->getData(i, buffer, type);
        if (size == 0 || buffer == nullptr)
            continue;

        if (type != VSTGUI::IDataPackage::kFilePath &&
            type != VSTGUI::IDataPackage::kText) {
            continue;
        }

        std::string raw(
            reinterpret_cast<const char*>(buffer),
            static_cast<std::size_t>(size));

        // Some hosts expose CF_UNICODETEXT before CF_HDROP. VSTGUI maps that
        // to kText, so consider each text line as a possible file path instead
        // of rejecting the package solely because it is not kFilePath.
        std::size_t start = 0u;
        while (start <= raw.size()) {
            const auto end = raw.find_first_of("\r\n", start);
            auto candidate = normalizeCandidate(
                raw.substr(start,
                           end == std::string::npos
                               ? std::string::npos
                               : end - start));

            if (isWavePath(candidate)) {
                path = std::move(candidate);
                return true;
            }

            if (end == std::string::npos)
                break;

            start = end + 1u;
            while (start < raw.size() &&
                   (raw[start] == '\r' || raw[start] == '\n')) {
                ++start;
            }
        }
    }

    return false;
}

void SourceSlotView::draw(VSTGUI::CDrawContext* context) {
    const auto r = getViewSize();
    const double normalized =
        std::clamp(static_cast<double>(getValueNormalized()), 0.0, 1.0);
    const bool loaded = normalized > 0.0;
    const bool loopState = normalized > 0.75;
    const char* label = dragActive_ ? "DROP" : (loaded ? (loopState ? "LOOP" : "ONE") : "WAV");
    const int sourceNumber = std::clamp(
        getTag() - static_cast<std::int32_t>(kSourceStatusBase) + 1, 1, 8);

    context->setDrawMode(VSTGUI::kAntiAliasing);
    VSTGUI::CRect shadow=r; shadow.offset(0.0,2.0);
    context->setFillColor({1,3,5,220});
    context->drawRect(shadow,VSTGUI::kDrawFilled);

    context->setFillColor(
        dragActive_ ? VSTGUI::CColor{31,66,96,255}
                    : (loaded ? VSTGUI::CColor{24,35,45,255}
                              : VSTGUI::CColor{12,17,22,255}));
    context->setFrameColor(
        dragActive_ ? VSTGUI::CColor{110,190,249,255}
                    : (loaded ? VSTGUI::CColor{72,111,145,240}
                              : VSTGUI::CColor{53,65,79,230}));
    context->setLineWidth(dragActive_ ? 1.8 : 1.0);
    context->drawRect(r,VSTGUI::kDrawFilledAndStroked);

    const double d=5.0;
    VSTGUI::CRect led(r.right-9.0,r.top+4.0,r.right-9.0+d,r.top+4.0+d);
    context->setFillColor(
        dragActive_ ? VSTGUI::CColor{184,224,255,255}
                    : (loaded ? VSTGUI::CColor{86,176,235,255}
                              : VSTGUI::CColor{21,31,40,255}));
    context->setFrameColor({4,7,10,255});
    context->drawEllipse(led,VSTGUI::kDrawFilledAndStroked);

    char sourceText[4]{};
    std::snprintf(sourceText,sizeof(sourceText),"%d",sourceNumber);
    VSTGUI::CRect nr(r.left+5.0,r.top+3.0,r.left+16.0,r.top+13.0);
    context->setFont(VSTGUI::kNormalFont,6.8,VSTGUI::kBoldFace);
    context->setFontColor({128,147,164,255});
    context->drawString(sourceText,nr,VSTGUI::kLeftText);

    context->setFont(VSTGUI::kNormalFont,7.2,VSTGUI::kBoldFace);
    context->setFontColor(
        dragActive_ ? VSTGUI::CColor{245,249,252,255}
                    : (loaded ? VSTGUI::CColor{213,228,240,255}
                              : VSTGUI::CColor{133,149,164,255}));
    context->drawString(label,r,VSTGUI::kCenterText);
    setDirty(false);
}

VSTGUI::DragOperation SourceSlotView::onDragEnter(VSTGUI::DragEventData data) {
    std::string path;
    if (!extractWavePath(data.drag, path))
        return VSTGUI::DragOperation::None;

    dragActive_ = true;
    invalid();
    return VSTGUI::DragOperation::Copy;
}

VSTGUI::DragOperation SourceSlotView::onDragMove(VSTGUI::DragEventData data) {
    std::string path;
    return extractWavePath(data.drag, path)
        ? VSTGUI::DragOperation::Copy
        : VSTGUI::DragOperation::None;
}

void SourceSlotView::onDragLeave(VSTGUI::DragEventData) {
    dragActive_ = false;
    invalid();
}

bool SourceSlotView::onDrop(VSTGUI::DragEventData data) {
    std::string path;
    const bool valid = extractWavePath(data.drag, path);
    dragActive_ = false;
    invalid();

    if (!valid || !controller_)
        return false;

    const auto sourceIndex = getTag() -
        static_cast<std::int32_t>(kSourceStatusBase);
    return controller_->loadDroppedSample(path, sourceIndex);
}

MacroKnob::MacroKnob(const VSTGUI::CRect& size,
                     VSTGUI::IControlListener* listener,
                     std::int32_t tag)
: VSTGUI::CKnob(size,listener,tag,nullptr,nullptr) {
    setStartAngle(static_cast<float>(135.0/180.0*kPi));
    setRangeAngle(static_cast<float>(270.0/180.0*kPi));
    setWantsFocus(true);
    setTransparency(true);
}

void MacroKnob::draw(VSTGUI::CDrawContext* context) {
    const auto r=getViewSize();
    const auto c=r.getCenter();
    const double radius=std::min(r.getWidth(),r.getHeight())*0.30;
    const double n=std::clamp(static_cast<double>(getValueNormalized()),0.0,1.0);
    const double angle=(135.0+n*270.0)*kPi/180.0;
    context->setDrawMode(VSTGUI::kAntiAliasing);

    for(int i=0;i<11;++i){
        const double t=static_cast<double>(i)/10.0;
        const double a=(135.0+270.0*t)*kPi/180.0;
        const bool major=(i==0||i==5||i==10);
        context->setFrameColor(major?VSTGUI::CColor{205,216,226,190}:VSTGUI::CColor{103,118,133,125});
        context->setLineWidth(major?1.3:0.8);
        context->drawLine({c.x+std::cos(a)*(radius+5.0),c.y+std::sin(a)*(radius+5.0)},
                          {c.x+std::cos(a)*(radius+(major?10.0:8.0)),c.y+std::sin(a)*(radius+(major?10.0:8.0))});
    }

    context->setFillColor({0,0,0,100});
    context->drawEllipse({c.x-radius-5,c.y-radius-2,c.x+radius+5,c.y+radius+8},VSTGUI::kDrawFilled);

    const VSTGUI::CRect skirt(c.x-radius-3,c.y-radius-3,c.x+radius+3,c.y+radius+3);
    context->setFillColor({54,61,69,255});
    context->setFrameColor({103,113,124,210});
    context->setLineWidth(1.0);
    context->drawEllipse(skirt,VSTGUI::kDrawFilledAndStroked);

    const double cap=radius*0.72;
    const VSTGUI::CRect capRect(c.x-cap,c.y-cap,c.x+cap,c.y+cap);
    context->setFillColor({16,21,27,255});
    context->setFrameColor({6,8,11,255});
    context->drawEllipse(capRect,VSTGUI::kDrawFilledAndStroked);

    const double rr=radius+2.0;
    VSTGUI::CRect arc(c.x-rr,c.y-rr,c.x+rr,c.y+rr);
    if(n>0.001){
        context->setFrameColor(kAccent);
        context->setLineWidth(2.3);
        context->drawArc(arc,135.f,static_cast<float>(135.0+n*270.0));
    }

    const double p1=cap*0.15,p2=cap*0.82;
    context->setFrameColor({236,242,247,255});
    context->setLineWidth(2.0);
    context->drawLine({c.x+std::cos(angle)*p1,c.y+std::sin(angle)*p1},
                      {c.x+std::cos(angle)*p2,c.y+std::sin(angle)*p2});

    const double tx=c.x+std::cos(angle)*(radius+2.0);
    const double ty=c.y+std::sin(angle)*(radius+2.0);
    context->setFillColor({123,191,241,255});
    context->drawEllipse({tx-1.7,ty-1.7,tx+1.7,ty+1.7},VSTGUI::kDrawFilled);

    if(isEditing()) {
        char text[16]{};
        std::snprintf(text,sizeof(text),"%d%%",static_cast<int>(std::lround(n*100.0)));
        const VSTGUI::CRect badge(c.x-17,c.y-7,c.x+17,c.y+7);
        context->setFillColor({7,11,15,235});
        context->setFrameColor({86,154,220,220});
        context->drawRect(badge,VSTGUI::kDrawFilledAndStroked);
        context->setFont(VSTGUI::kNormalFont,7.0,VSTGUI::kBoldFace);
        context->setFontColor({239,245,250,255});
        context->drawString(text,badge,VSTGUI::kCenterText);
    }
    setDirty(false);
}


SelectorView::SelectorView(const VSTGUI::CRect& size,
                           VSTGUI::IControlListener* listener,
                           std::int32_t tag,
                           std::vector<std::string> labels)
: VSTGUI::CControl(size,listener,tag),
  labels_(std::move(labels)) {
    setTransparency(true);
    setWantsFocus(true);
}

SelectorView::SelectorView(const SelectorView& other)
: VSTGUI::CControl(other),
  labels_(other.labels_) {
}

void SelectorView::draw(VSTGUI::CDrawContext* context) {
    const auto r=getViewSize();
    const std::size_t count=labels_.empty()?1u:labels_.size();
    const int index=count<=1u?0:std::clamp(
        static_cast<int>(std::lround(getValueNormalized()*static_cast<double>(count-1u))),
        0,static_cast<int>(count-1u));
    const std::string label=labels_.empty()?std::string{}:labels_[static_cast<std::size_t>(index)];

    context->setDrawMode(VSTGUI::kAntiAliasing);
    VSTGUI::CRect shadow=r; shadow.offset(0.0,1.5);
    context->setFillColor({2,4,6,210});
    context->drawRect(shadow,VSTGUI::kDrawFilled);
    context->setFillColor({18,24,31,255});
    context->setFrameColor({69,83,99,230});
    context->setLineWidth(1.0);
    context->drawRect(r,VSTGUI::kDrawFilledAndStroked);

    // Small blue datum rail: recognisable Phraseator selector language.
    context->setFrameColor({86,154,220,150});
    context->setLineWidth(1.3);
    context->drawLine({r.left+5.0,r.bottom-3.0},{r.right-5.0,r.bottom-3.0});

    context->setFont(VSTGUI::kNormalFont, std::min(9.0,std::max(7.0,r.getHeight()*0.34)), VSTGUI::kBoldFace);
    context->setFontColor({232,238,244,255});
    context->drawString(VSTGUI::UTF8String(label.c_str()),r,VSTGUI::kCenterText);
    setDirty(false);
}

VSTGUI::CMouseEventResult SelectorView::onMouseDown(
    VSTGUI::CPoint& where,const VSTGUI::CButtonState& buttons) {
    if(!getViewSize().pointInside(where) || labels_.size()<2u)
        return VSTGUI::kMouseEventNotHandled;
    if(resetToDefaultOnCtrlClick(this, buttons))
        return VSTGUI::kMouseDownEventHandledButDontNeedMovedOrUpEvents;
    const bool backwards=buttons.isRightButton();
    if(!buttons.isLeftButton() && !backwards)
        return VSTGUI::kMouseEventNotHandled;

    const int last=static_cast<int>(labels_.size()-1u);
    int index=std::clamp(
        static_cast<int>(std::lround(getValueNormalized()*static_cast<double>(last))),0,last);
    index=backwards ? (index==0?last:index-1) : (index==last?0:index+1);

    beginEdit();
    setValueNormalized(static_cast<float>(index)/static_cast<float>(last));
    valueChanged();
    endEdit();
    invalid();
    return VSTGUI::kMouseDownEventHandledButDontNeedMovedOrUpEvents;
}

ToggleView::ToggleView(const VSTGUI::CRect& size,
                       VSTGUI::IControlListener* listener,
                       std::int32_t tag,
                       std::string offLabel,
                       std::string onLabel,
                       bool compact)
: VSTGUI::CControl(size,listener,tag),
  offLabel_(std::move(offLabel)),
  onLabel_(std::move(onLabel)),
  compact_(compact) {
    setTransparency(true);
    setWantsFocus(true);
}

ToggleView::ToggleView(const ToggleView& other)
: VSTGUI::CControl(other),
  offLabel_(other.offLabel_),
  onLabel_(other.onLabel_),
  compact_(other.compact_) {
}

void ToggleView::draw(VSTGUI::CDrawContext* context) {
    const auto r=getViewSize();
    const bool on=getValueNormalized()>=0.5f;
    context->setDrawMode(VSTGUI::kAntiAliasing);

    VSTGUI::CRect shadow=r; shadow.offset(0.0,1.5);
    context->setFillColor({1,3,5,220});
    context->drawRect(shadow,VSTGUI::kDrawFilled);
    context->setFillColor(on?VSTGUI::CColor{27,51,73,255}:VSTGUI::CColor{17,22,28,255});
    context->setFrameColor(on?VSTGUI::CColor{86,154,220,245}:VSTGUI::CColor{66,78,92,220});
    context->setLineWidth(on?1.4:1.0);
    context->drawRect(r,VSTGUI::kDrawFilledAndStroked);

    const double d=compact_?5.0:7.0;
    const VSTGUI::CRect led(r.left+5.0,r.getCenter().y-d*.5,r.left+5.0+d,r.getCenter().y+d*.5);
    context->setFillColor(on?VSTGUI::CColor{150,207,255,255}:VSTGUI::CColor{24,34,44,255});
    context->setFrameColor({5,8,11,255});
    context->drawEllipse(led,VSTGUI::kDrawFilledAndStroked);

    const auto& label=on?onLabel_:offLabel_;
    VSTGUI::CRect tr=r;
    // Keep compact source-mute labels clear of their LED, but center all
    // normal toggles (FREE/LOCK, OFF/ON) geometrically in the full control.
    if(compact_)
        tr.left+=11.0;
    context->setFont(VSTGUI::kNormalFont,compact_?7.0:8.2,VSTGUI::kBoldFace);
    context->setFontColor(on?VSTGUI::CColor{239,246,252,255}:VSTGUI::CColor{174,186,198,255});
    context->drawString(VSTGUI::UTF8String(label.c_str()),tr,VSTGUI::kCenterText);
    setDirty(false);
}

VSTGUI::CMouseEventResult ToggleView::onMouseDown(
    VSTGUI::CPoint& where,const VSTGUI::CButtonState& buttons) {
    if(!getViewSize().pointInside(where))
        return VSTGUI::kMouseEventNotHandled;
    if(resetToDefaultOnCtrlClick(this, buttons))
        return VSTGUI::kMouseDownEventHandledButDontNeedMovedOrUpEvents;
    if(!buttons.isLeftButton())
        return VSTGUI::kMouseEventNotHandled;
    beginEdit();
    setValueNormalized(getValueNormalized()>=0.5f?0.0f:1.0f);
    valueChanged();
    endEdit();
    invalid();
    return VSTGUI::kMouseDownEventHandledButDontNeedMovedOrUpEvents;
}

ActionButton::ActionButton(const VSTGUI::CRect& size,
                           VSTGUI::IControlListener* listener,
                           std::int32_t tag,
                           std::string label,
                           bool compact,
                           bool accent)
: VSTGUI::CControl(size,listener,tag),
  label_(std::move(label)),
  compact_(compact),
  accent_(accent) {
    setTransparency(true);
    setWantsFocus(true);
}

ActionButton::ActionButton(const ActionButton& other)
: VSTGUI::CControl(other),
  label_(other.label_),
  compact_(other.compact_),
  accent_(other.accent_) {
}

void ActionButton::draw(VSTGUI::CDrawContext* context) {
    const auto r=getViewSize();
    context->setDrawMode(VSTGUI::kAntiAliasing);
    VSTGUI::CRect shadow=r; shadow.offset(0.0,2.0);
    context->setFillColor({1,2,4,230});
    context->drawRect(shadow,VSTGUI::kDrawFilled);
    context->setFillColor(accent_?VSTGUI::CColor{31,63,91,255}:VSTGUI::CColor{24,29,36,255});
    context->setFrameColor(accent_?VSTGUI::CColor{95,173,235,255}:VSTGUI::CColor{78,88,101,230});
    context->setLineWidth(accent_?1.4:1.0);
    context->drawRect(r,VSTGUI::kDrawFilledAndStroked);
    VSTGUI::CRect inner=r; inner.inset(2.0,2.0);
    context->setFrameColor({255,255,255,18});
    context->drawRect(inner,VSTGUI::kDrawStroked);
    context->setFont(VSTGUI::kNormalFont,compact_?6.8:9.0,VSTGUI::kBoldFace);
    context->setFontColor({239,243,247,255});
    context->drawString(VSTGUI::UTF8String(label_.c_str()),r,VSTGUI::kCenterText);
    setDirty(false);
}

VSTGUI::CMouseEventResult ActionButton::onMouseDown(
    VSTGUI::CPoint& where,const VSTGUI::CButtonState& buttons) {
    if(!getViewSize().pointInside(where))
        return VSTGUI::kMouseEventNotHandled;
    // Momentary edit/load actions have no user-facing default-reset meaning.
    // Ctrl-click must not accidentally fire them.
    if(buttons.isControlSet())
        return VSTGUI::kMouseDownEventHandledButDontNeedMovedOrUpEvents;
    if(!buttons.isLeftButton())
        return VSTGUI::kMouseEventNotHandled;
    beginEdit();
    setValueNormalized(1.0f);
    valueChanged();
    endEdit();
    // Action listener deliberately returns trigger controls to zero.
    invalid();
    return VSTGUI::kMouseDownEventHandledButDontNeedMovedOrUpEvents;
}

UIScaleView::UIScaleView(const VSTGUI::CRect& size,
                         VSTGUI::VST3Editor* editor,
                         Controller* controller)
: VSTGUI::CView(size),editor_(editor),controller_(controller) {
    setTransparency(true);
    setMouseEnabled(true);
    setWantsFocus(true);
}

void UIScaleView::draw(VSTGUI::CDrawContext* context) {
    const auto r=getViewSize();
    const double zoom=editor_?editor_->getZoomFactor():1.0;
    const int percent=static_cast<int>(std::lround(zoom*100.0));
    char text[20]{};
    std::snprintf(text,sizeof(text),"UI %d%%",percent);
    context->setFillColor({16,21,27,255});
    context->setFrameColor({63,76,90,230});
    context->setLineWidth(1.0);
    context->drawRect(r,VSTGUI::kDrawFilledAndStroked);
    context->setFont(VSTGUI::kNormalFont,7.5,VSTGUI::kBoldFace);
    context->setFontColor({174,187,200,255});
    context->drawString(VSTGUI::UTF8String(text),r,VSTGUI::kCenterText);
    setDirty(false);
}

VSTGUI::CMouseEventResult UIScaleView::onMouseDown(
    VSTGUI::CPoint& where,const VSTGUI::CButtonState& buttons) {
    if(!editor_ || !buttons.isLeftButton() || !getViewSize().pointInside(where))
        return VSTGUI::kMouseEventNotHandled;
    const double next=editor_->getZoomFactor()>=1.25?1.0:1.5;
    if(controller_) controller_->setGuiZoom(next);
    else editor_->setZoomFactor(next);
    invalid();
    return VSTGUI::kMouseDownEventHandledButDontNeedMovedOrUpEvents;
}

void configureEditor(VSTGUI::VST3Editor* editor,double width,double height,double zoom){
    if(!editor) return;
    editor->setAllowedZoomFactors(std::vector<double>{1.0,1.5});
    editor->setEditorSizeConstrains({width,height},{width,height});
    editor->setZoomFactor(zoom);
}

VSTGUI::CView* createCustomView(VSTGUI::UTF8StringPtr name,
                                const VSTGUI::UIAttributes& attributes,
                                VSTGUI::VST3Editor* editor,
                                Controller* controller){
    if(!name || !editor) return nullptr;
    VSTGUI::CPoint origin{0,0}, size{80,80};
    attributes.getPointAttribute("origin",origin);
    attributes.getPointAttribute("size",size);
    const VSTGUI::CRect rect(origin.x,origin.y,origin.x+size.x,origin.y+size.y);

    if(std::strcmp(name,"PhraseFaceplate")==0) return new FaceplateView(rect);
    if(std::strcmp(name,"PhraseLogo")==0) return new LogoView(rect);

    Steinberg::int32 tag=-1;
    attributes.getIntegerAttribute("control-tag",tag);

    if(std::strcmp(name,"PhraseStep")==0 &&
       tag >= static_cast<Steinberg::int32>(kPatternViewBase) &&
       tag < static_cast<Steinberg::int32>(kPatternViewBase + kPatternViewCount))
        return new StepIndicator(rect,editor,tag,controller);

    if(std::strcmp(name,"PhraseRatchet")==0 &&
       tag >= static_cast<Steinberg::int32>(kStepRatchetBase) &&
       tag < static_cast<Steinberg::int32>(kStepRatchetBase + kStepRatchetCount))
        return new RatchetView(rect, editor, tag);

    if(std::strcmp(name,"PhraseSourceSlot")==0 &&
       tag >= static_cast<Steinberg::int32>(kSourceStatusBase) &&
       tag < static_cast<Steinberg::int32>(kSourceStatusBase + kSourceStatusCount))
        return new SourceSlotView(rect, editor, tag, controller);

    if(std::strcmp(name,"PhraseKnob")==0 && tag>=0)
        return new MacroKnob(rect,editor,tag);

    if(std::strcmp(name,"PhraseUIScale")==0)
        return new UIScaleView(rect,editor,controller);

    if(std::strcmp(name,"PhraseGenerate")==0 && tag>=0)
        return new ActionButton(rect,controller,tag,"GENERATE",false,true);
    if(std::strcmp(name,"PhraseVariate")==0 && tag>=0)
        return new ActionButton(rect,controller,tag,"VARIATE",false,false);
    if(std::strcmp(name,"PhraseLoadOne")==0 && tag>=0)
        return new ActionButton(rect,controller,tag,"LOAD",true,false);
    if(std::strcmp(name,"PhraseClear")==0 && tag>=0)
        return new ActionButton(rect,controller,tag,"X",true,false);

    if(std::strcmp(name,"PhraseMute")==0 &&
       tag>=static_cast<Steinberg::int32>(kSourceMuteBase) &&
       tag<static_cast<Steinberg::int32>(kSourceMuteBase+kSourceMuteCount))
        return new ToggleView(rect,editor,tag,"ON","MUTE",true);

    if(std::strcmp(name,"PhrasePitchToKey")==0)
        return new ToggleView(rect,editor,tag,"OFF","ON",false);
    if(std::strcmp(name,"PhraseLock")==0)
        return new ToggleView(rect,editor,tag,"FREE","LOCK",false);
    if(std::strcmp(name,"PhraseMode")==0)
        return new SelectorView(rect,editor,tag,{"CONTINUE","RETRIGGER"});
    if(std::strcmp(name,"PhraseOctave")==0)
        return new SelectorView(rect,editor,tag,{"OFF","+1","-1","+/-1"});
    if(std::strcmp(name,"PhraseKeyRoot")==0)
        return new SelectorView(rect,editor,tag,{"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"});
    if(std::strcmp(name,"PhraseScale")==0)
        return new SelectorView(rect,editor,tag,{"CHROM","MAJOR","MINOR"});
    if(std::strcmp(name,"PhraseDelayDivision")==0)
        return new SelectorView(rect,editor,tag,{"OFF","1/4","1/8","1/8D","1/8T","1/16","1/16D","1/16T"});
    if(std::strcmp(name,"PhraseCutMode")==0)
        return new SelectorView(rect,editor,tag,{"LP","HP"});

    return nullptr;
}

} // namespace phraseator::vst3::gui
