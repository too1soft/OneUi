#pragma once
#include <oneui/mobile/presentation.h>
#include <oneui/mobile/scroll_gesture.h>
namespace oneui::mobile {
class ScrollWheelPad final : public Widget {
  public:
    ScrollWheelPad(ControlPalette palette,std::function<void(float)> delta,bool horizontal=false):palette_(palette),gesture_(std::move(delta)),horizontal_(horizontal){
        setAccessibleName(horizontal?L"滑动横向滚轮":L"滑动滚轮");setPreferredSize(horizontal?Size{72,64}:Size{64,72});
    }
    void paint(Canvas &canvas) override{
        auto r=frame();canvas.fillRect(r,pressed_?palette_.pressed:palette_.page,28);
        for(int i=-2;i<=2;++i){float y=r.y+r.height/2+i*6;canvas.drawLine({r.x+r.width/2-9,y},{r.x+r.width/2+9,y},palette_.secondary,i==0?2.f:1.f);}
    }
    bool onMouseDown(const MouseEvent &event) override{if(disabled()||event.button!=MouseButton::Left||!frame().contains(event.position))return false;pressed_=true;gesture_.begin(horizontal_?event.position.x:event.position.y);invalidate();return true;}
    bool onMouseMove(const MouseEvent &event) override{if(!pressed_||disabled())return false;gesture_.move(horizontal_?event.position.x:event.position.y);return true;}
    bool onMouseUp(const MouseEvent &) override{bool old=pressed_;resetInteractionState();return old;}
  protected:
    bool hasInteractionState() const override{return pressed_;}
    void resetInteractionState() override{pressed_=false;gesture_.cancel();invalidate();}
  private:
    ControlPalette palette_;ScrollGesture gesture_;bool horizontal_,pressed_=false;
};
inline std::shared_ptr<Stack> scrollWheel(ControlPalette palette,std::function<void(float)> delta,bool enabled,bool horizontal=false){
    auto body=std::make_shared<Stack>(horizontal?StackDirection::Row:StackDirection::Column);body->setPadding({4,4,4,4});body->setGap(4);
    StyleBox box;box.background.color=palette.surface;box.borderWidth=0;box.radius=28;
    box.shadows.push_back(StyleShadow{Color{0,0,0,42},Point{0,8},24,0,false});body->setStyleBox(box);
    auto up=actionButton(L"",[delta]{delta(120);},palette,ActionKind::Text);up->setAccessibleName(horizontal?L"向左滚动":L"向上滚动");up->setIcon(horizontal?IconSymbol::ChevronLeft:IconSymbol::ChevronUp);up->setPreferredSize(horizontal?Size{48,64}:Size{64,48});up->setDisabled(!enabled);body->add(up);
    auto pad=std::make_shared<ScrollWheelPad>(palette,delta,horizontal);pad->setDisabled(!enabled);body->add(pad);
    auto down=actionButton(L"",[delta]{delta(-120);},palette,ActionKind::Text);down->setAccessibleName(horizontal?L"向右滚动":L"向下滚动");down->setIcon(horizontal?IconSymbol::ChevronRight:IconSymbol::ChevronDown);down->setPreferredSize(horizontal?Size{48,64}:Size{64,48});down->setDisabled(!enabled);body->add(down);
    body->setPreferredSize(horizontal?Size{184,72}:Size{72,184});return body;
}
}
