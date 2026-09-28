#include "oneui/layout/adaptive_panes.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace oneui {
namespace {
bool hasFocus(const std::shared_ptr<Widget>& widget) {
    if(widget->focused())return true;
    if(auto view=std::dynamic_pointer_cast<View>(widget))for(const auto& child:view->children())if(hasFocus(child))return true;
    return false;
}
void positive(float value) { if(!std::isfinite(value) || value<=0)throw std::invalid_argument("Pane dimensions must be finite and positive"); }
}
AdaptivePanes::AdaptivePanes(PanePattern pattern,std::shared_ptr<Widget> first,std::shared_ptr<Widget> second)
    :pattern_(pattern),layout_(std::make_shared<Stack>(StackDirection::Row)),first_(std::move(first)),second_(std::move(second)),
     breakpoint_(pattern==PanePattern::Sidebar?960.f:800.f),paneWidth_(pattern==PanePattern::Sidebar?208.f:380.f) {
    if(!first_ || !second_ || first_==second_)throw std::invalid_argument("Pane layout needs two distinct children");
    if(!layout_->setEngine(StackEngine::Yoga))throw std::runtime_error("Pane layout requires Yoga");
    layout_->setAlign(StackAlign::Stretch);layout_->add(first_);layout_->add(second_);add(layout_);
}
void AdaptivePanes::setBreakpoint(float width) {positive(width);if(breakpoint_==width)return;breakpoint_=width;configured_=false;invalidate();}
void AdaptivePanes::setPaneWidth(float width) {positive(width);if(paneWidth_==width)return;paneWidth_=width;configured_=false;invalidate();}
void AdaptivePanes::setDetailOpen(bool open) {if(detailOpen_==open)return;detailOpen_=open;configured_=false;invalidate();}
bool AdaptivePanes::activatePane(bool detail) {
    setDetailOpen(detail);layoutChildren();
    return requestFocus((detail?second_:first_).get());
}
void AdaptivePanes::setStyleBox(StyleBox style) {style_=std::move(style);layout_->setGap(style_.gap.value_or(24));layout_->setPadding(style_.padding.value_or(Insets{}));configured_=false;invalidate();}
Size AdaptivePanes::measure(Size available) const {return {std::isfinite(available.width)?available.width:800.f,std::isfinite(available.height)?available.height:480.f};}
Rect AdaptivePanes::paintBounds() const {
    const auto a=View::paintBounds(),b=stylePaintBounds(frame(),style_);
    const float x=(std::min)(a.x,b.x),y=(std::min)(a.y,b.y);
    return {x,y,(std::max)(a.x+a.width,b.x+b.width)-x,(std::max)(a.y+a.height,b.y+b.height)-y};
}
void AdaptivePanes::configure(float width) {
    const bool narrow=width<breakpoint_;
    const auto padding=style_.padding.value_or(Insets{});
    const float usable=(std::max)(0.f,width-padding.left-padding.right-style_.gap.value_or(24.f));
    const float paneWidth=(std::min)(paneWidth_,usable*.5f);
    if(configured_ && narrow==narrow_ && paneWidth==configuredPaneWidth_)return;
    configuredPaneWidth_=paneWidth;
    narrow_=narrow;configured_=true;
    const bool firstVisible=pattern_==PanePattern::Sidebar || !narrow || !detailOpen_;
    const bool secondVisible=pattern_==PanePattern::Sidebar || !narrow || detailOpen_;
    const bool movedFocus=(!firstVisible && hasFocus(first_)) || (!secondVisible && hasFocus(second_));
    first_->setVisible(firstVisible);second_->setVisible(secondVisible);
    layout_->setDirection(pattern_==PanePattern::Sidebar && narrow?StackDirection::Column:StackDirection::Row);
    if(pattern_==PanePattern::Sidebar) {
        if(auto navigation=std::dynamic_pointer_cast<Stack>(first_)) {
            navigation->setDirection(narrow?StackDirection::Row:StackDirection::Column);navigation->setWrap(narrow);
            navigation->setAlign(narrow?StackAlign::Center:StackAlign::Stretch);
        }
        layout_->setFlex(first_,narrow?StackFlex{0,0,{},0,INFINITY,true}:StackFlex{0,0,paneWidth,0});
    } else layout_->setFlex(first_,{1,1,0,0});
    layout_->setFlex(second_,pattern_==PanePattern::MasterDetail && !narrow?StackFlex{0,1,paneWidth,0}:StackFlex{1,1,0,0});
    if(movedFocus){focusChild(layout_.get(),true);layout_->focusFirstLeaf();}
}
void AdaptivePanes::layoutChildren() {configure(frame().width);layout_->setFrame(frame());}
void AdaptivePanes::paint(Canvas& canvas) {layoutChildren();paintStyleBox(canvas,frame(),style_);View::paint(canvas);}
}
