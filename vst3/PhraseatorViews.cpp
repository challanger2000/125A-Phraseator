#include "PhraseatorViews.h"
#include "branding_master.h"

#include "vstgui/lib/cdrawcontext.h"
#include "vstgui/lib/cgraphicspath.h"
#include "vstgui/lib/controls/cbuttons.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>
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
                                VSTGUI::IControlListener* controllerListener){
    if(!name || !editor) return nullptr;
    VSTGUI::CPoint origin{0,0}, size{80,80};
    attributes.getPointAttribute("origin",origin);
    attributes.getPointAttribute("size",size);
    const VSTGUI::CRect rect(origin.x,origin.y,origin.x+size.x,origin.y+size.y);

    if(std::strcmp(name,"PhraseLogo")==0) return new LogoView(rect);

    Steinberg::int32 tag=-1;
    attributes.getIntegerAttribute("control-tag",tag);

    if(std::strcmp(name,"PhraseKnob")==0 && tag>=0)
        return new MacroKnob(rect,editor,tag);

    if(std::strcmp(name,"PhraseGenerate")==0 && tag>=0)
        return new VSTGUI::CTextButton(rect,editor,tag,"GENERATE");
    if(std::strcmp(name,"PhraseVariate")==0 && tag>=0)
        return new VSTGUI::CTextButton(rect,editor,tag,"VARIATE");
    if(std::strcmp(name,"PhraseLoadOne")==0 && tag>=0)
        return new VSTGUI::CTextButton(rect,controllerListener,tag,"ONE");
    if(std::strcmp(name,"PhraseLoadLoop")==0 && tag>=0)
        return new VSTGUI::CTextButton(rect,controllerListener,tag,"LOOP");

    return nullptr;
}

} // namespace phraseator::vst3::gui
