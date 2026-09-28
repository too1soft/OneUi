#include "platform/win32/accessibility_win32.h"
#include "oneui/controls/button.h"
#include "oneui/controls/text_field.h"
#include <iostream>

int main(){
    using namespace oneui;
    int failures=0;
    auto check=[&](bool value,const char* name){if(!value){std::cerr<<name<<"\n";++failures;}};
    CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    const HWND window=CreateWindowExW(0,L"STATIC",L"OneUI accessibility test",WS_OVERLAPPED,0,0,400,300,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    auto alive=std::make_shared<std::atomic_bool>(true);
    auto root=std::make_shared<View>();root->setFrame({0,0,400,300});
    auto button=std::make_shared<Button>(L"Action");button->setFrame({10,10,100,32});
    int clicks=0;button->setOnClick([&]{++clicks;});root->add(button);
    auto field=std::make_shared<TextField>(L"Private field");field->setPasswordMode(true);field->setText(L"secret-value");field->setFrame({10,50,180,32});root->add(field);
    auto hidden=std::make_shared<Button>(L"Hidden");hidden->setFrame({10,90,100,32});hidden->setVisible(false);root->add(hidden);
    auto context=std::make_shared<win32_accessibility::Context>();context->window=window;context->alive=alive;
    context->root=[root]{return root;};context->scale=[]{return 1.5f;};context->focus=[root](Widget* w){return root->requestFocus(w,true);};
    auto* provider=new win32_accessibility::Provider(context);context->provider=provider;
    VARIANT self{};self.vt=VT_I4;self.lVal=CHILDID_SELF;
    LONG count=0;check(SUCCEEDED(provider->get_accChildCount(&count))&&count==2,"visible semantic controls only");
    VARIANT first{};first.vt=VT_I4;first.lVal=1;
    IDispatch* one=nullptr;IDispatch* two=nullptr;provider->get_accChild(first,&one);provider->get_accChild(first,&two);
    check(one==nullptr&&two==nullptr,"simple child IDs remain stable across observations");
    auto* action=provider;check(SUCCEEDED(action->accDoDefaultAction(first))&&clicks==1,"default action reaches real button callback");
    button->setDisabled(true);check(FAILED(action->accDoDefaultAction(first))&&clicks==1,"disabled action is rejected");
    LONG x=0,y=0,w=0,h=0;action->accLocation(&x,&y,&w,&h,first);check(w==150&&h==48,"physical accessibility bounds follow DPI");
    VARIANT second{};second.vt=VT_I4;second.lVal=2;IDispatch* edit=nullptr;provider->get_accChild(second,&edit);
    auto* accessibleEdit=provider;BSTR value=nullptr;accessibleEdit->get_accValue(second,&value);
    check(value && std::wstring(value)!=L"secret-value","password value remains masked");SysFreeString(value);
    BSTR replacement=SysAllocString(L"new value");check(SUCCEEDED(accessibleEdit->put_accValue(second,replacement))&&field->text()==L"new value","value edit uses native text replacement");SysFreeString(replacement);
    field->setReadOnly(true);replacement=SysAllocString(L"not allowed");check(FAILED(accessibleEdit->put_accValue(second,replacement))&&field->text()==L"new value","read-only values cannot be overwritten");SysFreeString(replacement);
    auto title=std::make_shared<WindowTitleBar>(L"Client");title->setFrame({0,0,400,38});title->setVariant("terminal");root->add(title);
    int closes=0, maxes=0;title->setOnClose([&]{++closes;});title->setOnMaximize([&]{++maxes;});
    provider->get_accChildCount(&count);check(count==6,"caption exposes three actual window controls");
    VARIANT captionClose{};captionClose.vt=VT_I4;captionClose.lVal=6;
    BSTR name=nullptr;provider->get_accName(captionClose,&name);check(name&&std::wstring(name)==L"关闭窗口","caption has accessible name");SysFreeString(name);
    provider->accLocation(&x,&y,&w,&h,captionClose);check(w==69,"Windows caption bounds use full button width at DPI");
    check(provider->accDoDefaultAction(captionClose)==S_OK&&closes==1,"caption action reaches real close callback");
    // Skin variants affect paint only; explicit presentation owns geometry.
    title->setVariant("custom-left");
    provider->accLocation(&x,&y,&w,&h,captionClose);check(w==69,"skin does not change accessible geometry");
    TitleBarPresentation leftCaption;
    leftCaption.buttons[0].frame={32,5,24,28};
    leftCaption.buttons[1].frame={56,5,24,28};
    leftCaption.buttons[2].frame={8,5,24,28};
    check(title->setPresentation(&leftCaption),"custom caption presentation accepted");
    provider->accLocation(&x,&y,&w,&h,captionClose);
    POINT clientOrigin{};ClientToScreen(window,&clientOrigin);
    check(x==clientOrigin.x+12 && w==36 && h==42,"custom caption keeps same identity with left geometry");
    VARIANT captionMax=captionClose;captionMax.lVal=5;title->setMaximized(true);provider->get_accName(captionMax,&name);check(name&&std::wstring(name)==L"还原窗口","maximize action name reflects restore state");SysFreeString(name);
    check(provider->accDoDefaultAction(captionMax)==S_OK&&maxes==1,"custom caption uses real maximize callback");
    title->setDisabled(true);check(FAILED(provider->accDoDefaultAction(captionClose))&&closes==1,"disabled caption cannot activate");
    auto branch=std::make_shared<View>();branch->setAccessibleName(L"Semantic parent");branch->setFrame({0,100,200,100});
    auto nested=std::make_shared<Button>(L"Focused leaf");nested->setFrame({10,110,100,32});branch->add(nested);root->add(branch);
    root->requestFocus(nested.get(),true);
    VARIANT focus{};check(provider->get_accFocus(&focus)==S_OK&&focus.vt==VT_I4,"nested control exposes focus");
    name=nullptr;provider->get_accName(focus,&name);check(name&&std::wstring(name)==L"Focused leaf","focus resolves to leaf instead of routing parent");SysFreeString(name);
    VARIANT parentId=focus;--parentId.lVal;VARIANT state{};provider->get_accState(parentId,&state);check(!(state.lVal&STATE_SYSTEM_FOCUSED),"routing parent does not claim accessible keyboard focus");
    provider->get_accState(focus,&state);check(state.lVal&STATE_SYSTEM_FOCUSED,"focused leaf reports focused state");
    alive->store(false);check(FAILED(action->accDoDefaultAction(first)),"stale window action fails closed");
    provider->Release();DestroyWindow(window);CoUninitialize();
    if(!failures)std::cout<<"Windows accessibility behavior passed\n";
    return failures?1:0;
}
