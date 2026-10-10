#include "font_picker_state.h"
#include "oneui/style_sheet.h"
#include "picker_styles.h"
#include <cmath>
namespace oneui {
namespace {
StyleBox rowColors(const Widget& w) {
    if(auto sheet=w.styleScope())return pickerColors(*sheet);
    return {};
}
}
void FontPickerRows::refresh() {
    auto next=std::clamp(offset,0.f,std::max(0.f,float(filtered.size())*48-frame().height));
    if(next!=offset){offset=next;invalidate();}
}
void FontPickerRows::paint(Canvas& c) {
    painted=0;auto r=frame();auto box=rowColors(*this);
    c.save();c.clipRect(r);c.fillRect(r,box.background.color.value_or(Color{255,255,255}));
    int first=std::max(0,int(offset/48)),last=std::min(int(filtered.size()),int(std::ceil((offset+r.height)/48)));
    for(int i=first;i<last;++i) {
        const auto& item=items[filtered[i]];Rect row{r.x,r.y+i*48-offset,r.width,48};++painted;
        if(i==highlight)c.fillRect(row,box.selectionColor.value_or(Color{213,233,215}),4);
        auto color=box.foreground.value_or(Color{28,43,35});if(!item.available)color.a=130;
        c.drawText(item.label,{row.x+8,row.y,row.width-16,20},color,12,TextAlign::Left);
        c.drawTextStyledWithNamedFont(preview,{row.x+8,row.y+20,row.width-16,28},color,size,TextAlign::Left,item.available?item.family:L"",TextFontFamily::Default);
    }
    c.restore();
}
int FontPickerRows::hit(Point p) const {
    if(!frame().contains(p))return -1;int i=int((p.y-frame().y+offset)/48);
    return i>=0&&size_t(i)<filtered.size()?i:-1;
}
bool FontPickerRows::onMouseDown(const MouseEvent& e) {
    if(!interactive()||e.button!=MouseButton::Left)return false;pressed=hit(e.position);
    if(pressed<0)return false;highlight=pressed;invalidate();return true;
}
bool FontPickerRows::onMouseUp(const MouseEvent& e) {
    int row=pressed;pressed=-1;if(row<0)return false;
    if(hit(e.position)==row&&activate)activate(row);return true;
}
bool FontPickerRows::onMouseWheel(const MouseWheelEvent& e) {offset-=e.deltaY*48;refresh();invalidate();return true;}
void FontPickerRows::move(int delta) {
    if(filtered.empty()){highlight=-1;return;}
    highlight=std::clamp(highlight+delta,0,int(filtered.size())-1);
    if(highlight*48<offset)offset=float(highlight*48);
    if((highlight+1)*48>offset+frame().height)offset=float((highlight+1)*48)-frame().height;
    refresh();invalidate();
}
bool FontPickerRows::onKeyDown(const KeyEvent& e) {
    if(!interactive()||!e.pressed)return false;
    if(e.key==Key::Up||e.key==Key::Down){move(e.key==Key::Up?-1:1);return true;}
    if(e.key==Key::Enter&&highlight>=0){if(activate)activate(highlight);return true;}return false;
}
AccessibilityInfo FontPickerRows::accessibilityInfo() const {
    auto info=Widget::accessibilityInfo();info.name=L"Font candidates";
    if(highlight>=0&&size_t(highlight)<filtered.size())info.value=items[filtered[highlight]].label;
    return info;
}
} // namespace oneui
