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
                             std::int32_t tag)
: VSTGUI::CControl(size, listener, tag) {
    setMouseEnabled(false);
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
        const int fragment = std::clamp(
            static_cast<int>(std::lround(normalized * kPatternViewStepCount)) - 1,
            0, kPatternViewStepCount - 1);
        char text[8] {};
        std::snprintf(text, sizeof(text), "%d", fragment + 1);
        context->setFont(VSTGUI::kNormalFontSmall);
        context->setFontColor({240, 245, 250, 255});
        context->drawString(text, r, VSTGUI::kCenterText);
    }

    setDirty(false);
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

    const char* label = "DROP WAV";
    if (normalized > 0.75)
        label = "LOOP";
    else if (normalized > 0.25)
        label = "ONE";

    if (dragActive_) {
        context->setFillColor({28, 55, 78, 255});
        context->setFrameColor({86, 154, 220, 255});
    } else {
        context->setFillColor({8, 12, 17, 255});
        context->setFrameColor({52, 65, 79, 255});
    }
    context->setLineWidth(dragActive_ ? 1.8 : 1.0);
    context->drawRect(r, VSTGUI::kDrawFilledAndStroked);
    context->setFont(VSTGUI::kNormalFontSmall);
    context->setFontColor(dragActive_
        ? VSTGUI::CColor{239, 243, 247, 255}
        : VSTGUI::CColor{154, 168, 183, 255});
    context->drawString(label, r, VSTGUI::kCenterText);
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
    const double radius=std::min(r.getWidth(),r.getHeight())*0.31;
    const double n=std::clamp(static_cast<double>(getValueNormalized()),0.0,1.0);
    const double angle=(135.0+n*270.0)*kPi/180.0;
    context->setDrawMode(VSTGUI::kAntiAliasing);

    for(int i=0;i<9;++i){
        const double t=static_cast<double>(i)/8.0;
        const double a=(135.0+270.0*t)*kPi/180.0;
        context->setFrameColor((i==0||i==4||i==8)?VSTGUI::CColor{205,213,222,210}:kTick);
        context->setLineWidth((i==0||i==4||i==8)?1.35:1.0);
        context->drawLine({c.x+std::cos(a)*(radius+5),c.y+std::sin(a)*(radius+5)},
                          {c.x+std::cos(a)*(radius+10),c.y+std::sin(a)*(radius+10)});
    }

    context->setFillColor({18,22,28,255});
    context->setFrameColor({63,73,84,255});
    context->setLineWidth(1.2);
    context->drawEllipse({c.x-radius,c.y-radius,c.x+radius,c.y+radius},VSTGUI::kDrawFilledAndStroked);

    const double rr=radius+4.0;
    VSTGUI::CRect arc(c.x-rr,c.y-rr,c.x+rr,c.y+rr);
    if(n>0.001){
        context->setFrameColor(kAccent);
        context->setLineWidth(2.5);
        context->drawArc(arc,135.f,static_cast<float>(135.0+n*270.0));
    }

    const double p1=radius*0.18, p2=radius*0.78;
    context->setFrameColor(kText);
    context->setLineWidth(2.2);
    context->drawLine({c.x+std::cos(angle)*p1,c.y+std::sin(angle)*p1},
                      {c.x+std::cos(angle)*p2,c.y+std::sin(angle)*p2});
    setDirty(false);
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

    if(std::strcmp(name,"PhraseLogo")==0) return new LogoView(rect);

    Steinberg::int32 tag=-1;
    attributes.getIntegerAttribute("control-tag",tag);

    if(std::strcmp(name,"PhraseStep")==0 &&
       tag >= static_cast<Steinberg::int32>(kPatternViewBase) &&
       tag < static_cast<Steinberg::int32>(kPatternViewBase + kPatternViewCount))
        return new StepIndicator(rect,editor,tag);

    if(std::strcmp(name,"PhraseSourceSlot")==0 &&
       tag >= static_cast<Steinberg::int32>(kSourceStatusBase) &&
       tag < static_cast<Steinberg::int32>(kSourceStatusBase + kSourceStatusCount))
        return new SourceSlotView(rect, editor, tag, controller);

    if(std::strcmp(name,"PhraseKnob")==0 && tag>=0)
        return new MacroKnob(rect,editor,tag);

    if(std::strcmp(name,"PhraseGenerate")==0 && tag>=0)
        return new VSTGUI::CTextButton(rect,controller,tag,"GENERATE");
    if(std::strcmp(name,"PhraseVariate")==0 && tag>=0)
        return new VSTGUI::CTextButton(rect,controller,tag,"VARIATE");
    if(std::strcmp(name,"PhraseLoadOne")==0 && tag>=0)
        return new VSTGUI::CTextButton(rect,controller,tag,"ONE");
    if(std::strcmp(name,"PhraseLoadLoop")==0 && tag>=0)
        return new VSTGUI::CTextButton(rect,controller,tag,"LOOP");
    if(std::strcmp(name,"PhraseClear")==0 && tag>=0)
        return new VSTGUI::CTextButton(rect,controller,tag,"X");

    return nullptr;
}

} // namespace phraseator::vst3::gui
