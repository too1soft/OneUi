#pragma once
#include <oneui/mobile/presentation.h>

namespace oneui::mobile {
// A quiet full-width command row. Navigation and selection use the same native
// hit target, focus and keyboard behavior across mobile products.
class ActionRow final : public Widget {
  public:
    ActionRow(std::wstring title,IconSymbol icon,ControlPalette palette,std::function<void()> action,
              float scale=1,bool selected=false,bool navigates=true):title_(std::move(title)),icon_(icon),palette_(palette),action_(std::move(action)),scale_(scale),selected_(selected),navigates_(navigates){
        setPreferredSize({0,std::max(52.f,52*scale)});setAccessibleRole(AccessibilityRole::Button);setAccessibleName(title_);
    }
    void setGlyphPainter(std::function<void(Canvas&,Rect,Color)> painter){painter_=std::move(painter);invalidate();}
    bool isFocusable() const override{return !disabled();}
    AccessibilityInfo accessibilityInfo() const override{auto info=Widget::accessibilityInfo();info.state.selected=selected_;return info;}
    void paint(Canvas &canvas) override{
        const auto r=frame();const auto ink=disabled()?palette_.secondary:selected_?palette_.tint:palette_.ink;
        if(pressed_||focusVisible())canvas.fillRect(r,palette_.pressed,10);
        const float s=22*scale_,gap=16;
        const Rect iconRect{r.x+4,r.y+(r.height-s)/2,s,s};if(painter_)painter_(canvas,iconRect,ink);else paintIcon(canvas,icon_,iconRect,ink);
        canvas.drawTextStyledEllipsized(title_,{r.x+s+gap,r.y,r.width-s-52,r.height},ink,16*scale_,TextAlign::Left,selected_?600:450);
        if(selected_||navigates_)paintIcon(canvas,selected_?IconSymbol::Check:IconSymbol::ChevronRight,{r.x+r.width-24,r.y+(r.height-18)/2,18,18},selected_?palette_.tint:palette_.secondary);
    }
    bool onMouseDown(const MouseEvent &event) override{pressed_=!disabled()&&event.button==MouseButton::Left&&frame().contains(event.position);invalidate();return pressed_;}
    bool onMouseUp(const MouseEvent &event) override{bool activate=pressed_&&!disabled()&&frame().contains(event.position);pressed_=false;invalidate();if(activate&&action_)action_();return activate;}
    bool onKeyDown(const KeyEvent &event) override{if(disabled()||!event.pressed||(event.key!=Key::Enter&&event.key!=Key::Space))return false;if(action_)action_();return true;}
  protected:
    bool hasInteractionState() const override{return pressed_;}
    void resetInteractionState() override{pressed_=false;invalidate();}
  private:
    std::wstring title_;IconSymbol icon_;ControlPalette palette_;std::function<void()> action_;float scale_;bool selected_,navigates_,pressed_=false;
    std::function<void(Canvas&,Rect,Color)> painter_;
};
}
