#include "connection_editor.hpp"
#include "support/recording_canvas.h"
#include <iostream>
#define CHECK(x) do {if(!(x))throw std::runtime_error("Failed: " #x);}while(0)
using namespace connection_demo;
int main(){try{
    UiMailbox mailbox;VM vm(mailbox.sender());Mount ui;auto page=buildPage(ui,vm);
    oneui::test_support::RecordingCanvas canvas;
    vm.focusError=[&](const auto& field){ui.flush();page.root.widget->paint(canvas);CHECK(page.root.as<View>()->requestFocus(page.fields.at(field).get()));CHECK(revealField(page.root.widget,page.fields.at(field).get()));};
    for(bool dark:{false,true})for(float width:{1100.f,640.f,426.f}) {
        ui.styles()->replace(declarativeTheme(dark));page.root.widget->setFrame({0,0,width,700});page.root.widget->paint(canvas);
        auto issues=inspectLayout(page.root.widget,*ui.styles());if(!issues.empty())throw std::runtime_error(formatLayoutIssues(issues));
    }
    auto count=ui.styles()->size();auto name=std::dynamic_pointer_cast<TextField>(page.fields.at("name"));
    CHECK(std::dynamic_pointer_cast<SystemClipboard>(name->clipboard()));
    auto clipboard=std::make_shared<MemoryClipboard>();clipboard->setText(L"粘贴中文 B R 123");name->setClipboard(clipboard);
    name->setSelectionRange(0,name->text().size());KeyEvent paste;paste.key=Key::V;paste.control=true;CHECK(name->onKeyDown(paste));CHECK(vm.name.get()==L"粘贴中文 B R 123");
    page.root.as<View>()->requestFocus(name.get());name->setCaretIndex(2);name->setSelectionRange(1,2);name->setTextComposition(L"中文",1);
    vm.message.set(L"不相关的状态变化");ui.flush();ui.styles()->replace(declarativeTheme());CHECK(name->caretIndex()==2);CHECK(name->hasTextComposition());CHECK(name->hasSelection());name->setTextComposition(L"",0);
    for(auto value:{L"",L"https://example.com",L"999.1.1.1",L"a..b",L"bad-.example",L"12.3"}) {vm.host.set(value);CHECK(!vm.error("host").empty());}
    for(auto value:{L"localhost",L"127.0.0.1",L"demo.example.com"}){vm.host.set(value);CHECK(vm.error("host").empty());}
    for(auto value:{L"0",L"65536",L"-1",L"1.5",L" 80"}){vm.port.set(value);CHECK(!vm.error("port").empty());}vm.port.set(L"443");
    vm.timeout.set(L"abc");vm.save.execute();ui.flush();CHECK(!vm.save.running.get());CHECK(page.fields.at("timeout")->focused());CHECK(!vm.timeoutError.get().empty());
    page.root.widget->paint(canvas);
    auto field=page.fields.at("timeout")->frame();CHECK(field.y>=0 && field.y+field.height<700);
    vm.timeout.set(L"20");vm.name.set(L"中文输入 B R 123 空格");
    auto finish=[&]{for(int i=0;i<200 && vm.save.running.get();++i){std::this_thread::sleep_for(std::chrono::milliseconds(5));mailbox.drain();}CHECK(!vm.save.running.get());ui.flush();};
    vm.failNext.execute();vm.save.execute();CHECK(vm.save.running.get());finish();CHECK(!vm.save.error.get().empty());CHECK(vm.dirty.get());
    vm.save.execute();auto submitted=vm.value();vm.note.set(L"保存中继续输入");finish();CHECK(vm.saved.get()==submitted);CHECK(vm.dirty.get());CHECK(vm.note.get()==L"保存中继续输入");
    vm.reset.execute();ui.flush();CHECK(!vm.dirty.get());CHECK(vm.note.get()==submitted.note);CHECK(ui.styles()->size()==count);
    vm.save.execute();mailbox.close();for(int i=0;i<20;++i)std::this_thread::sleep_for(std::chrono::milliseconds(5));
    std::cout<<"Editor validation, snapshot save, cancellation, layout, input retention and focus reveal tests passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}
