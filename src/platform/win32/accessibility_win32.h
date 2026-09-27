#pragma once
// MSAA supplies Windows' built-in UIA legacy bridge and screen readers with
// the same semantics used by OneUI controls. COM marshals calls to the UI STA.
#include <windows.h>
#include <oleacc.h>
#include <atomic>
#include <cmath>
#include <functional>
#include <memory>
#include <vector>
#include "oneui/layout/overlay_host.h"
#include "oneui/controls/popup.h"
#include "oneui/controls/text_field.h"
#include "oneui/controls/window_title_bar.h"
#include <unordered_set>

namespace oneui::win32_accessibility {
struct Context {
    HWND window{};
    IAccessible* provider=nullptr;
    std::weak_ptr<std::atomic_bool> alive;
    std::function<std::shared_ptr<Widget>()> root;
    std::function<bool(Widget*)> focus;
    std::function<float()> scale;
    bool valid() const { auto value=alive.lock(); return value && value->load() && IsWindow(window); }
};
inline void collect(const std::shared_ptr<Widget>& widget,
                    std::vector<std::shared_ptr<Widget>>& output,
                    std::unordered_set<Widget*>& visited) {
    if (!widget || !widget->visible() || !visited.insert(widget.get()).second) return;
    const auto frame=widget->frame();
    if (frame.width<=0 || frame.height<=0) return;
    const auto info=widget->accessibilityInfo();
    if (info.role!=AccessibilityRole::None || !info.name.empty()) output.push_back(widget);
    if (auto host=std::dynamic_pointer_cast<OverlayHost>(widget)) {
        const OverlayEntry* modal=nullptr;
        for (const auto& entry:host->overlays()) if (entry.child && entry.child->visible() && entry.trapsFocus
            && (!modal || entry.layer>=modal->layer)) modal=&entry;
        if (modal) { collect(modal->child,output,visited); return; }
        collect(host->content(),output,visited);
        for (const auto& entry:host->overlays()) collect(entry.child,output,visited);
    }
    if (auto view=std::dynamic_pointer_cast<View>(widget))
        for (const auto& child:view->children()) collect(child,output,visited);
    if (auto popup=std::dynamic_pointer_cast<Popup>(widget); popup && popup->isOpen())
        collect(popup->content(),output,visited);
}
inline LONG role(AccessibilityRole value) {
    switch(value) {
    case AccessibilityRole::Button:return ROLE_SYSTEM_PUSHBUTTON;
    case AccessibilityRole::Text:return ROLE_SYSTEM_STATICTEXT;
    case AccessibilityRole::TextBox:return ROLE_SYSTEM_TEXT;
    case AccessibilityRole::CheckBox:return ROLE_SYSTEM_CHECKBUTTON;
    case AccessibilityRole::RadioButton:return ROLE_SYSTEM_RADIOBUTTON;
    case AccessibilityRole::RadioGroup:return ROLE_SYSTEM_GROUPING;
    case AccessibilityRole::ComboBox:return ROLE_SYSTEM_COMBOBOX;
    case AccessibilityRole::Slider:return ROLE_SYSTEM_SLIDER;
    case AccessibilityRole::ProgressBar:return ROLE_SYSTEM_PROGRESSBAR;
    case AccessibilityRole::Tab:return ROLE_SYSTEM_PAGETAB;
    case AccessibilityRole::TabList:return ROLE_SYSTEM_PAGETABLIST;
    case AccessibilityRole::List:return ROLE_SYSTEM_LIST;
    case AccessibilityRole::ListItem:return ROLE_SYSTEM_LISTITEM;
    case AccessibilityRole::Table:return ROLE_SYSTEM_TABLE;
    case AccessibilityRole::Row:return ROLE_SYSTEM_ROW;
    case AccessibilityRole::Cell:return ROLE_SYSTEM_CELL;
    case AccessibilityRole::Popup:return ROLE_SYSTEM_DIALOG;
    case AccessibilityRole::Window:return ROLE_SYSTEM_WINDOW;
    default:return ROLE_SYSTEM_CLIENT;
    }
}
class Provider final : public IAccessible, public IOleWindow {
public:
    explicit Provider(std::shared_ptr<Context> context) : context_(std::move(context)) {}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id,void** result) override {
        if(!result)return E_POINTER; *result=nullptr;
        if(id==IID_IUnknown || id==IID_IDispatch || id==IID_IAccessible){*result=static_cast<IAccessible*>(this);AddRef();return S_OK;}
        if(id==IID_IOleWindow){*result=static_cast<IOleWindow*>(this);AddRef();return S_OK;}
        return E_NOINTERFACE;
    }
    HRESULT STDMETHODCALLTYPE GetWindow(HWND* window) override {if(!window)return E_POINTER;*window=context_->valid()?context_->window:nullptr;return *window?S_OK:CO_E_OBJNOTCONNECTED;}
    HRESULT STDMETHODCALLTYPE ContextSensitiveHelp(BOOL) override{return E_NOTIMPL;}
    ULONG STDMETHODCALLTYPE AddRef() override{return ++refs_;}
    ULONG STDMETHODCALLTYPE Release() override{const auto count=--refs_;if(!count)delete this;return count;}
    HRESULT STDMETHODCALLTYPE GetTypeInfoCount(UINT* count) override {if(!count)return E_POINTER;*count=0;return S_OK;}
    HRESULT STDMETHODCALLTYPE GetTypeInfo(UINT,LCID,ITypeInfo** value) override {if(value)*value=nullptr;return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE GetIDsOfNames(REFIID,LPOLESTR*,UINT,LCID,DISPID*) override{return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE Invoke(DISPID,REFIID,LCID,WORD,DISPPARAMS*,VARIANT*,EXCEPINFO*,UINT*) override{return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE get_accParent(IDispatch** out) override {
        if(!out)return E_POINTER;*out=nullptr;if(!context_->valid())return CO_E_OBJNOTCONNECTED;
        return CreateStdAccessibleObject(context_->window,OBJID_WINDOW,IID_IDispatch,reinterpret_cast<void**>(out));
    }
    HRESULT STDMETHODCALLTYPE get_accChildCount(long* count) override {
        if(!count)return E_POINTER;*count=0;if(!context_->valid())return CO_E_OBJNOTCONNECTED;
        refresh();*count=static_cast<long>(children_.size());return S_OK;
    }
    HRESULT STDMETHODCALLTYPE get_accChild(VARIANT child,IDispatch** out) override {
        if(!out)return E_POINTER;*out=nullptr;
        if(child.vt!=VT_I4 || child.lVal<=0)return E_INVALIDARG;
        // Simple child IDs give UIA stable identities instead of fresh COM proxy IDs.
        return resolve(child)?S_FALSE:E_INVALIDARG;
    }
    HRESULT STDMETHODCALLTYPE get_accName(VARIANT child,BSTR* out) override {
        if(!context_->valid())return missing(out);
        if(self(child)){wchar_t name[512]{};GetWindowTextW(context_->window,name,512);return string(name,out);}
        auto w=resolve(child);if(!w)return missing(out);
        const auto button=caption(child);
        if(button==TitleBarButtonId::Minimize)return string(L"最小化窗口",out);
        if(button==TitleBarButtonId::Close)return string(L"关闭窗口",out);
        if(button==TitleBarButtonId::Maximize)return string(std::static_pointer_cast<WindowTitleBar>(w)->maximized()?L"还原窗口":L"最大化窗口",out);
        return string(w->accessibilityInfo().name,out);
    }
    HRESULT STDMETHODCALLTYPE get_accValue(VARIANT child,BSTR* out) override {
        auto w=resolve(child);return w?string(w->accessibilityInfo().value,out):missing(out);
    }
    HRESULT STDMETHODCALLTYPE get_accDescription(VARIANT child,BSTR* out) override {
        auto w=resolve(child);return w?string(w->accessibilityInfo().description,out):missing(out);
    }
    HRESULT STDMETHODCALLTYPE get_accRole(VARIANT child,VARIANT* out) override {
        if(!out)return E_POINTER;VariantInit(out);auto w=resolve(child);
        if(!w)return S_FALSE;out->vt=VT_I4;out->lVal=self(child)?ROLE_SYSTEM_CLIENT:caption(child)!=TitleBarButtonId::None?ROLE_SYSTEM_PUSHBUTTON:role(w->accessibilityInfo().role);return S_OK;
    }
    HRESULT STDMETHODCALLTYPE get_accState(VARIANT child,VARIANT* out) override {
        if(!out)return E_POINTER;VariantInit(out);auto w=resolve(child);if(!w)return S_FALSE;
        const auto state=w->accessibilityInfo().state;LONG flags=0;
        if(w->disabled()||state.disabled)flags|=STATE_SYSTEM_UNAVAILABLE;
        if(!w->visible())flags|=STATE_SYSTEM_INVISIBLE;
        if(w->isFocusable())flags|=STATE_SYSTEM_FOCUSABLE;
        if(caption(child)==TitleBarButtonId::None && w==focusedTarget())flags|=STATE_SYSTEM_FOCUSED;
        if(state.selected)flags|=STATE_SYSTEM_SELECTED;
        if(state.checked)flags|=STATE_SYSTEM_CHECKED;
        if(state.pressed)flags|=STATE_SYSTEM_PRESSED;
        if(state.readOnly)flags|=STATE_SYSTEM_READONLY;
        if(w->textInputState().sensitive)flags|=STATE_SYSTEM_PROTECTED;
        if(state.expanded)flags|=STATE_SYSTEM_EXPANDED;
        out->vt=VT_I4;out->lVal=flags;return S_OK;
    }
    HRESULT STDMETHODCALLTYPE get_accHelp(VARIANT,BSTR* out) override{return missing(out);}
    HRESULT STDMETHODCALLTYPE get_accHelpTopic(BSTR* out,VARIANT,long* topic) override{if(topic)*topic=0;return missing(out);}
    HRESULT STDMETHODCALLTYPE get_accKeyboardShortcut(VARIANT,BSTR* out) override{return missing(out);}
    HRESULT STDMETHODCALLTYPE get_accFocus(VARIANT* out) override {
        if(!out)return E_POINTER;VariantInit(out);if(!context_->valid())return CO_E_OBJNOTCONNECTED;
        refresh();const auto focused=focusedTarget();
        for(std::size_t i=0;i<children_.size();++i)if(auto w=children_[i].lock();w&&w==focused&&children_[i].button==TitleBarButtonId::None){
            out->vt=VT_I4;out->lVal=static_cast<LONG>(i+1);return S_OK;}
        if(focused && focused==context_->root()){out->vt=VT_I4;out->lVal=CHILDID_SELF;}
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE get_accSelection(VARIANT* out) override {if(!out)return E_POINTER;VariantInit(out);return S_FALSE;}
    HRESULT STDMETHODCALLTYPE get_accDefaultAction(VARIANT child,BSTR* out) override {
        auto w=resolve(child);if(!w)return missing(out);
        if(caption(child)!=TitleBarButtonId::None)return string(L"Activate",out);
        const auto r=w->accessibilityInfo().role;
        if(r==AccessibilityRole::Button || r==AccessibilityRole::CheckBox || r==AccessibilityRole::RadioButton || r==AccessibilityRole::ComboBox)
            return string(L"Activate",out);
        return missing(out);
    }
    HRESULT STDMETHODCALLTYPE accSelect(long flags,VARIANT child) override {
        auto w=resolve(child);if(!w || w->disabled())return E_INVALIDARG;
        return (flags&SELFLAG_TAKEFOCUS)&&context_->focus(w.get())?S_OK:S_FALSE;
    }
    HRESULT STDMETHODCALLTYPE accLocation(long* x,long* y,long* width,long* height,VARIANT child) override {
        if(!x||!y||!width||!height)return E_POINTER;*x=*y=*width=*height=0;
        auto w=resolve(child);if(!w)return S_FALSE;
        const auto r=caption(child)==TitleBarButtonId::None?w->frame():std::static_pointer_cast<WindowTitleBar>(w)->windowButtonFrame(caption(child));const float scale=context_->scale();POINT origin{};ClientToScreen(context_->window,&origin);
        *x=origin.x+static_cast<long>(std::lround(r.x*scale));*y=origin.y+static_cast<long>(std::lround(r.y*scale));
        *width=static_cast<long>(std::lround(r.width*scale));*height=static_cast<long>(std::lround(r.height*scale));return S_OK;
    }
    HRESULT STDMETHODCALLTYPE accNavigate(long direction,VARIANT start,VARIANT* out) override {
        if(!out)return E_POINTER;VariantInit(out);
        if(self(start) && (direction==NAVDIR_FIRSTCHILD||direction==NAVDIR_LASTCHILD)){
            refresh();if(children_.empty())return S_FALSE;
            out->vt=VT_I4;out->lVal=direction==NAVDIR_FIRSTCHILD?1:static_cast<LONG>(children_.size());return S_OK;
        }return S_FALSE;
    }
    HRESULT STDMETHODCALLTYPE accHitTest(long x,long y,VARIANT* out) override {
        if(!out)return E_POINTER;VariantInit(out);if(!context_->valid())return CO_E_OBJNOTCONNECTED;
        refresh();POINT origin{};ClientToScreen(context_->window,&origin);const float scale=context_->scale();
        const Point point{(x-origin.x)/scale,(y-origin.y)/scale};
        for(std::size_t i=children_.size();i>0;--i)if(auto w=children_[i-1].lock()){
            const auto button=children_[i-1].button;
            const auto rect=button==TitleBarButtonId::None?w->frame():std::static_pointer_cast<WindowTitleBar>(w)->windowButtonFrame(button);
            if(button==TitleBarButtonId::None?w->hitTest(point):(point.x>=rect.x&&point.x<rect.x+rect.width&&point.y>=rect.y&&point.y<rect.y+rect.height)){
                out->vt=VT_I4;out->lVal=static_cast<LONG>(i);return S_OK;}
        }
        return S_FALSE;
    }
    HRESULT STDMETHODCALLTYPE accDoDefaultAction(VARIANT child) override {
        auto w=resolve(child);if(!w||!w->visible()||w->disabled())return E_INVALIDARG;
        const auto button=caption(child);
        if(button!=TitleBarButtonId::None)return std::static_pointer_cast<WindowTitleBar>(w)->activateWindowButton(button)?S_OK:S_FALSE;
        const auto r=w->accessibilityInfo().role;
        if(r!=AccessibilityRole::Button&&r!=AccessibilityRole::CheckBox&&r!=AccessibilityRole::RadioButton&&r!=AccessibilityRole::ComboBox)return S_FALSE;
        if(!context_->focus(w.get()))return S_FALSE;
        return w->onKeyDown(KeyEvent{r==AccessibilityRole::ComboBox?Key::Enter:Key::Space})?S_OK:S_FALSE;
    }
    HRESULT STDMETHODCALLTYPE put_accName(VARIANT,BSTR) override{return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE put_accValue(VARIANT child,BSTR value) override {
        auto w=resolve(child);if(!w||w->disabled()||w->accessibilityInfo().state.readOnly)return E_ACCESSDENIED;
        if(w->accessibilityInfo().role!=AccessibilityRole::TextBox||!context_->focus(w.get()))return E_NOTIMPL;
        const auto state=w->textInputState();if(!state.editable)return E_ACCESSDENIED;
        const auto field=std::dynamic_pointer_cast<TextField>(w);
        return w->replaceTextRange(0,field?field->text().size():state.text.size(),value?std::wstring(value,SysStringLen(value)):std::wstring{})?S_OK:S_FALSE;
    }
private:
    std::shared_ptr<Widget> focusedTarget() const {
        // Ancestors retain routing focus while a child receives input. Follow
        // the active chain (including popups), not the first focused node in
        // the flattened accessibility tree or a covered background control.
        std::shared_ptr<Widget> target;
        std::unordered_set<Widget*> visited;
        for(auto current=context_->root();current&&visited.insert(current.get()).second;current=current->activeFocusChild()) {
            if(!current->visible()||current->disabled())break;
            const auto info=current->accessibilityInfo();
            if(current->focused() && (info.role!=AccessibilityRole::None||!info.name.empty()||current==context_->root()))target=current;
        }
        return target;
    }
    static bool self(const VARIANT& child){return child.vt==VT_I4&&child.lVal==CHILDID_SELF;}
    static HRESULT missing(BSTR* out){if(!out)return E_POINTER;*out=nullptr;return S_FALSE;}
    static HRESULT string(const std::wstring& value,BSTR* out){if(!out)return E_POINTER;*out=SysAllocStringLen(value.data(),static_cast<UINT>(value.size()));return *out?S_OK:E_OUTOFMEMORY;}
    struct Entry {
        std::weak_ptr<Widget> widget;
        TitleBarButtonId button=TitleBarButtonId::None;
        std::shared_ptr<Widget> lock() const { return widget.lock(); }
    };
    TitleBarButtonId caption(const VARIANT& child) const {
        if(child.vt!=VT_I4||child.lVal<1||static_cast<std::size_t>(child.lVal)>children_.size())return TitleBarButtonId::None;
        return children_[child.lVal-1].button;
    }
    void refresh(){
        children_.clear();if(!context_->valid())return;
        std::vector<std::shared_ptr<Widget>> found;std::unordered_set<Widget*> seen;collect(context_->root(),found,seen);
        for(auto& w:found){
            children_.push_back({w});
            if(std::dynamic_pointer_cast<WindowTitleBar>(w))
                for(auto id:{TitleBarButtonId::Minimize,TitleBarButtonId::Maximize,TitleBarButtonId::Close})children_.push_back({w,id});
        }
    }
    std::shared_ptr<Widget> resolve(const VARIANT& child){
        if(!context_->valid()||child.vt!=VT_I4)return {};
        if(self(child))return context_->root();
        if(child.lVal<1)return {};if(children_.empty())refresh();
        const auto index=static_cast<std::size_t>(child.lVal-1);return index<children_.size()?children_[index].lock():nullptr;
    }
    std::atomic<ULONG> refs_{1};
    std::shared_ptr<Context> context_;
    std::vector<Entry> children_;
};
} // namespace oneui::win32_accessibility
