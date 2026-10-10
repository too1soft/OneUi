#include "diff_vm.h"
#include "DiffLegacy.g.h"
#include "DiffIr.g.h"
#include <iostream>
#include <sstream>
#include "support/recording_canvas.h"
using namespace oneui;
using namespace oneui::ui;
void check(bool ok,const char *message) { if(!ok)throw std::runtime_error(message); }
void snapshot(const std::shared_ptr<Widget> &widget,std::wostream &out) {
    out << L'(';
    if(auto label=std::dynamic_pointer_cast<Label>(widget))out << L"Text:" << label->text();
    else if(auto input=std::dynamic_pointer_cast<TextField>(widget))out << L"Input:" << input->text();
    else if(auto button=std::dynamic_pointer_cast<Button>(widget))out << L"Button:" << button->text();
    else if(auto stack=std::dynamic_pointer_cast<Stack>(widget))out << L"Stack";
    else out << typeid(*widget).name();
    out << L':' << widget->visible() << L':' << widget->disabled();
    if(auto view=std::dynamic_pointer_cast<View>(widget))for(auto &child:view->children())snapshot(child,out);
    out << L')';
}
std::wstring snapshot(const Element &root) { std::wostringstream out;snapshot(root.widget,out);return out.str(); }
template<class T> std::shared_ptr<T> find(const std::shared_ptr<Widget> &widget) {
    if(auto found=std::dynamic_pointer_cast<T>(widget))return found;
    if(auto view=std::dynamic_pointer_cast<View>(widget))for(auto &child:view->children())if(auto result=find<T>(child))return result;
    return {};
}
void buttonBorderDifferential() {
    DiffVm a,b; Mount legacy,ir;
    const std::string css="button { background-color:#e7eee8; color:#21302b; padding:8px; border-radius:6px; }";
    legacy.styles()->replace(css);ir.styles()->replace(css);
    auto old=build_DiffLegacy(a,legacy),next=build_DiffIr(b,ir);
    auto left=find<Button>(old.widget),right=find<Button>(next.widget);
    left->setFrame({0,0,160,40});right->setFrame({0,0,160,40});
    auto compare=[&](Color color) {
        test_support::RecordingCanvas x,y;left->paint(x);right->paint(y);
        check(!left->focusVisible() && !right->focusVisible(),"idle buttons are not focused");
        check(x.strokeRects.size()==1 && y.strokeRects.size()==1,"idle border is not a focus ring");
        for(auto c:{x.strokeRects.front().color,y.strokeRects.front().color})
            check(c.r==color.r && c.g==color.g && c.b==color.b,"legacy/IR idle border color");
    };
    compare(colors::Primary); // The native default variant supplies the blue border.
    legacy.styles()->replace(css+"button { border-color:#c6d0c8; border-width:1px; }");
    ir.styles()->replace(css+"button { border-color:#c6d0c8; border-width:1px; }");
    compare({198,208,200});
}
int main() {
    try {
        DiffVm a,b;
        Mount oldMount,newMount;
        setMotionEnabled(false);
        buttonBorderDifferential();
        // Legacy imports lack the caller's CSS scope. Use the same external
        // class rule for that newly-supported CALL case; scoped button rules
        // are still compared against the expanded legacy root below.
        const std::string commonCss="stack.one-Column.caller { padding:7px; }\n";
        oldMount.styles()->replace(commonCss+styles_DiffLegacy());
        newMount.styles()->replace(commonCss+styles_DiffIr());
        auto old=build_DiffLegacy(a,oldMount),next=build_DiffIr(b,newMount);
        auto compare=[&] { oldMount.flush();newMount.flush();check(snapshot(old)==snapshot(next),"native tree/default/value differential"); };
        compare();
        old.widget->setFrame({0,0,900,1000});next.widget->setFrame({0,0,900,1000});
        oneui::test_support::RecordingCanvas oldCanvas,newCanvas;
        old.widget->paint(oldCanvas);next.widget->paint(newCanvas);
        check(oldCanvas.texts.size()==newCanvas.texts.size(),"styled paint text count");
        for(size_t i=0;i<oldCanvas.texts.size();++i) {
            const auto &a=oldCanvas.texts[i],&b=newCanvas.texts[i];
            check(a.text==b.text && a.size==b.size && a.color.r==b.color.r && a.color.g==b.color.g &&
                a.color.b==b.color.b,"CALL class merge and scoped typography differential");
            check(std::abs(a.rect.x-b.rect.x)<0.1f && std::abs(a.rect.y-b.rect.y)<0.1f,
                "styled geometry differential");
        }
        auto oldInput=find<TextField>(old.widget),newInput=find<TextField>(next.widget);
        oldInput->onFocusChanged(true);newInput->onFocusChanged(true);
        oldInput->setTextComposition(L"拼",1);newInput->setTextComposition(L"拼",1);
        a.enabled.set(false);b.enabled.set(false);compare();
        check(oldInput->hasTextComposition() && newInput->hasTextComposition(),"unrelated update preserves IME");
        oldInput->setTextComposition({},0);newInput->setTextComposition({},0);
        oldInput->onTextCommitted(L"输入");newInput->onTextCommitted(L"输入");compare();
        check(a.title.get()==b.title.get(),"input writeback differential");
        check(oldInput->textInputState().anchor==newInput->textInputState().anchor,"selection differential");
        KeyEvent enter{};enter.key=Key::Enter;
        find<Button>(old.widget)->onKeyDown(enter);find<Button>(next.widget)->onKeyDown(enter);
        compare();check(a.trace==b.trace && a.trace.size()==1,"UI event command trace differential");
        auto oldAction=std::dynamic_pointer_cast<Button>(old.as<View>()->children().back());
        auto newAction=std::dynamic_pointer_cast<Button>(next.as<View>()->children().back());
        check(oldAction->text()==newAction->text(),"CALL binding overrides root static text");
        oldAction->onKeyDown(enter);newAction->onKeyDown(enter);compare();
        check(a.trace==b.trace && a.trace.size()==2,"CALL event forwarding uses caller scope");
        auto oldScroll=find<ScrollView>(old.widget),newScroll=find<ScrollView>(next.widget);
        auto longText=std::wstring(500,L'x');
        find<Label>(oldScroll)->setText(longText);find<Label>(newScroll)->setText(longText);
        oldScroll->setFrame({0,0,200,40});newScroll->setFrame({0,0,200,40});
        oldScroll->setContentHeight(300);newScroll->setContentHeight(300);
        oldScroll->setScrollOffset(12);newScroll->setScrollOffset(12);
        check(oldScroll->scrollOffset()>0 && oldScroll->scrollOffset()==newScroll->scrollOffset(),"nonzero scroll differential");
        auto oldRow=std::dynamic_pointer_cast<View>(old.widget)->children()[4];
        auto newRow=std::dynamic_pointer_cast<View>(next.widget)->children()[4];
        auto oldFirst=std::dynamic_pointer_cast<View>(oldRow)->children().front();
        auto newFirst=std::dynamic_pointer_cast<View>(newRow)->children().front();
        auto ar=a.rows.get(),br=b.rows.get();std::reverse(ar.begin(),ar.end());std::reverse(br.begin(),br.end());
        a.rows.set(ar);b.rows.set(br);compare();
        check(std::dynamic_pointer_cast<View>(oldRow)->children().back()==oldFirst,"legacy keyed identity");
        check(std::dynamic_pointer_cast<View>(newRow)->children().back()==newFirst,"IR keyed identity");
        ar.back()->text.set(L"updated");br.back()->text.set(L"updated");compare();
        a.choices.set({L"changed",L"two"});b.choices.set({L"changed",L"two"});compare();
        check(newMount.find("input")==newInput,"Mount ref compatibility");
        std::cout << "PASS C++ shared IR differential: tree/defaults/slot/CALL/input/IME/selection/events/lists\n";
    } catch(const std::exception &e) { std::cerr << e.what() << '\n';return 1; }
}
