#include "oneui/plugins/status.h"
#include "oneui/native_interop.h"
#include "oneui/unicode.h"
#include "status_contract.h"
#include <cmath>

namespace {
using namespace oneui;
Color color(uint32_t rgba) { return {uint8_t(rgba>>24),uint8_t(rgba>>16),uint8_t(rgba>>8),uint8_t(rgba)}; }
class Status final : public Widget {
public:
    const OneUiComponentHostV1 *host=nullptr;
    std::wstring label;
    double value=0;
    bool pressed=false;
    OneUiComponentStyleV1 style{sizeof(style),0x246bceff,0xf0f4faff,0x8aa1bdff,14,8,1};
    Status() { setPreferredSize({240,64}); }
    void changed() { invalidate(); }
    void refreshStyle() {
        if(host)host->style(host->token,0,&style);
        invalidate();
    }
    bool isFocusable() const override { return visible() && !disabled(); }
    CursorKind cursor(Point) const override { return disabled()?CursorKind::Default:CursorKind::Pointer; }
    bool activate() {
        if(!host || !visible() || disabled())return false;
        auto *context=host;
        return context->emit(context->token,1,nullptr,0)==0;
    }
    bool onMouseDown(const MouseEvent &event) override {
        if(disabled() || !contains(event.position) || event.button!=MouseButton::Left)return false;
        pressed=true;invalidate();return true;
    }
    bool onMouseUp(const MouseEvent &event) override {
        const bool run=pressed && contains(event.position) && event.button==MouseButton::Left;
        pressed=false;invalidate();return run?activate():false;
    }
    bool onKeyDown(const KeyEvent &event) override {
        return !event.repeat && (event.key==Key::Enter || event.key==Key::Space) ? activate():false;
    }
    void paint(Canvas &canvas) override {
        auto r=frame();auto background=color(style.background);
        if(pressed)background.a=200;
        canvas.fillRect(r,background,style.radius);
        canvas.drawTextEllipsized(label,{r.x+12,r.y+4,std::max(0.f,r.width-24),std::max(0.f,r.height-20)},
                                  color(style.foreground),style.font_size,TextAlign::Left);
        const Rect track{r.x+12,r.y+std::max(0.f,r.height-12),std::max(0.f,r.width-24),4};
        canvas.fillRect(track,color(style.border),2);
        canvas.fillRect({track.x,track.y,float(track.width*value),track.height},color(style.foreground),2);
        if(focusVisible())canvas.strokeRect(r,color(style.foreground),style.radius,2);
    }
    AccessibilityInfo accessibilityInfo() const override {
        auto info=Widget::accessibilityInfo();info.role=AccessibilityRole::Button;
        if(info.name.empty())info.name=label;
        info.value=std::to_wstring(int(value*100))+L"%";return info;
    }
private:
    bool hasInteractionState() const override {return pressed;}
    void resetInteractionState() override {pressed=false;invalidate();}
};
struct Instance { std::weak_ptr<Status> widget; };
int create(void *,const OneUiComponentHostV1 *host,OneUiWidget *const *,size_t count,void **out,OneUiWidget **widget) {
    try {
        if(count)return ONEUI_VIEW_INVALID_ARGUMENT;
        auto value=std::make_shared<Status>();value->host=host;
        *out=new Instance{value};*widget=createNativeWidgetHandle(value);return 0;
    }catch(const std::bad_alloc &){return ONEUI_VIEW_OUT_OF_MEMORY;}catch(...){return ONEUI_VIEW_FOREIGN_FAILURE;}
}
int set(void *opaque,uint32_t id,const OneUiValue *value) {
    try {
        auto held=static_cast<Instance*>(opaque)->widget.lock();
        if(!held)return ONEUI_VIEW_CLOSED;
        auto &w=*held;
        if(id==1000 && value->tag==ONEUI_VALUE_UTF8)w.label=unicode::fromUtf8({value->as.utf8.data,value->as.utf8.length});
        else if(id==1001 && value->tag==ONEUI_VALUE_F64 && std::isfinite(value->as.number) && value->as.number>=0 && value->as.number<=1)w.value=value->as.number;
        else return ONEUI_VIEW_TYPE;
        w.changed();return 0;
    }catch(...){return ONEUI_VIEW_FOREIGN_FAILURE;}
}
void disconnect(void *opaque) {if(opaque)if(auto w=static_cast<Instance*>(opaque)->widget.lock())w->host=nullptr;}
void release(void *opaque) {delete static_cast<Instance*>(opaque);}
void style(void *opaque) {if(opaque)if(auto w=static_cast<Instance*>(opaque)->widget.lock())w->refreshStyle();}
const OneUiComponentFactoryV1 factory{sizeof(factory),1,{status_contract,sizeof(status_contract)-1},nullptr,nullptr,nullptr,create,set,disconnect,release,style};
}
const OneUiComponentFactoryV1 *oneui_status_component_v1(void) {return &factory;}
