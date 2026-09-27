#include "oneui/ui_theme.h"
#include "oneui/ui_dev.h"
#include "oneui/ui_layout_diagnostics.h"
#include "manual.h"
#include "Demo.g.h"
#include <iostream>
#include <future>
#include "support/recording_canvas.h"
#define CHECK(x) do {if(!(x))throw std::runtime_error("Check failed: " #x);}while(0)
template<class F> void throws(F fn){bool caught=false;try{fn();}catch(const std::exception&){caught=true;}CHECK(caught);}
void flatten(const std::shared_ptr<Widget>& w,std::vector<std::shared_ptr<Widget>>& nodes) {
    nodes.push_back(w);if(auto v=std::dynamic_pointer_cast<View>(w))for(auto& c:v->children())flatten(c,nodes);
}
void contentLayoutTests() {
    using oneui::test_support::RecordingCanvas;
    auto near=[](float a,float b){CHECK(std::abs(a-b)<0.1f);};
    for(bool dark:{false,true}) {
        Mount ui;ui.styles()->replace(declarativeTheme(dark));
        State<std::wstring> name{L"中文工作空间"};auto input=ui.make("Input");ui.model(input,name);
        auto field=ui.make("FormRow",{input});ui.set(field,"label",L"工作空间名称");ui.set(field,"hint",L"说明文字在窄窗口中自动换行");
        auto section=ui.make("Section",{field});ui.set(section,"title",L"常规设置");
        auto caption=ui.make("Text");ui.set(caption,"text",L"保存后立即生效");
        auto content=ui.make("Content",{section,caption});
        RecordingCanvas canvas;
        input.as<TextField>()->onFocusChanged(true);input.as<TextField>()->setCaretIndex(2);
        input.as<TextField>()->setSelectionRange(1,2);
        auto count=ui.styles()->size();
        for(float width:{1100.f,640.f,320.f,1100.f}) {
            for(auto align:{L"start",L"center",L"end"}) {
                ui.set(content,"align",align);
                const auto size=content.widget->measure({width,INFINITY});
                content.widget->setFrame({17,23,width,size.height});content.widget->paint(canvas);
                const float bodyWidth=std::min(880.f,width),space=width-bodyWidth;
                near(content.content->frame().width,bodyWidth);
                near(content.content->frame().x,17+(std::wstring(align)==L"center"?space/2:std::wstring(align)==L"end"?space:0));
                auto labels=field.as<Stack>()->children().front()->frame(),control=input.widget->frame();
                if(width==320) CHECK(control.y>=labels.y+labels.height);
                if(width==1100) CHECK(control.x>=labels.x+labels.width);
                CHECK(control.x>=section.widget->frame().x);
                CHECK(control.x+control.width<=section.widget->frame().x+section.widget->frame().width+0.1f);
                CHECK(input.as<TextField>()->caretIndex()==2);CHECK(input.as<TextField>()->hasSelection());
                CHECK(ui.styles()->size()==count);
            }
        }
        ui.set(content,"max-width",560);auto size=content.widget->measure({1100,INFINITY});
        content.widget->setFrame({0,0,1100,size.height});content.widget->paint(canvas);near(content.content->frame().width,560);
        for(float gap:{32.f,0.f,12.f}) {
            ui.styles()->replace(syntax::css(gap==0?"Content {}":"Content { gap:"+std::to_string(gap)+"px; }"));
            size=content.widget->measure({1100,INFINITY});content.widget->setFrame({0,0,1100,size.height});content.widget->paint(canvas);
            near(caption.widget->frame().y-(section.widget->frame().y+section.widget->frame().height),gap);
            near(content.content->frame().width,560);CHECK(ui.styles()->size()==count);
        }
        throws([&]{ui.set(content,"max-width",0);});throws([&]{ui.set(content,"max-width",INFINITY);});
        throws([&]{ui.set(content,"max-width",-1);});throws([&]{ui.set(content,"grow",NAN);});
        throws([&]{ui.set(content,"align",L"middle");});throws([&]{ui.set(input,"max-width",560);});
    }
}
void pagePatternTests() {
    using oneui::test_support::RecordingCanvas;
    for(bool code:{false,true}) {
        UiMailbox mailbox;DemoVM vm(mailbox.sender());Mount ui;auto root=code?buildManual(vm,ui):build_Demo(vm,ui);
        RecordingCanvas canvas;
        vm.selectedKey.set(L"20");
        for(bool dark:{false,true})for(float width:{1100.f,640.f,426.f})for(int page=0;page<4;++page) {
            ui.styles()->replace(declarativeTheme(dark));
            if(page==0)vm.showSettings.execute();else if(page==2)vm.showDetails.execute();else vm.showConnections.execute();
            vm.reload.running.set(page==3);ui.flush();
            root.widget->setFrame({0,0,width,740});root.widget->paint(canvas);
            auto issues=inspectLayout(root.widget,*ui.styles());
            if(!issues.empty())throw std::runtime_error(formatLayoutIssues(issues));
        }
        vm.reload.running.set(false);vm.showConnections.execute();ui.flush();
        auto count=ui.styles()->size();vm.reload.execute();CHECK(vm.reload.running.get());ui.flush();CHECK(!vm.showTable.get());
        for(int i=0;i<150 && vm.reload.running.get();++i){std::this_thread::sleep_for(std::chrono::milliseconds(10));mailbox.drain();}
        CHECK(!vm.reload.running.get());ui.flush();CHECK(vm.selectedKey.get()==L"20");CHECK(vm.showTable.get());CHECK(ui.styles()->size()==count);
        vm.showDetails.execute();ui.flush();CHECK(vm.details.get());CHECK(vm.selectedName.get()==L"工作节点 20");
        vm.showConnections.execute();ui.flush();CHECK(vm.connections.get());CHECK(vm.selectedKey.get()==L"20");
    }
    Mount ui;auto child=ui.make("Text");ui.set(child,"text",L"A long label");child.min(200);ui.locate(child,"broken.one",42);
    auto row=ui.make("Row",{child});row.widget->setFrame({0,0,100,60});RecordingCanvas canvas;row.widget->paint(canvas);
    auto issues=inspectLayout(row.widget,*ui.styles());CHECK(!issues.empty());
    auto report=formatLayoutIssues(issues);CHECK(report.find("broken.one:42")!=report.npos);CHECK(report.find("min=200")!=report.npos);CHECK(report.find("overflow")!=report.npos);
    child.widget->setVisible(false);CHECK(inspectLayout(row.widget,*ui.styles()).empty());child.widget->setVisible(true);
    child.widget->setFrame({0,0,200,1});CHECK(formatLayoutIssues(inspectLayout(child.widget,*ui.styles())).find("text-clipped")!=std::string::npos);
    auto input=ui.make("Input");auto field=ui.make("FormRow",{input});ui.set(field,"hint",L"提示");ui.set(field,"error",L"错误");CHECK(input.widget->accessibleDescription()==L"提示 错误");
    ui.set(field,"error",L"");CHECK(!field.error->visible());CHECK(input.widget->accessibleDescription()==L"提示");
    throws([&]{ui.make("SettingsPage",{ui.make("ActionBar"),ui.make("ActionBar")});});
    auto status=ui.make("Status");throws([&]{ui.set(status,"tone",L"invalid");});
    auto registrations=ui.styles()->size();for(int i=0;i<40;++i){ui.set(status,"tone",i%2?L"error":L"success");ui.styles()->replace(declarativeTheme(i%2));}CHECK(ui.styles()->size()==registrations);
}
int main(){try{
    contentLayoutTests();
    pagePatternTests();
    State<int> a{1},b{2};int notifications=0,recomputes=0;
    auto subscription=a.subscribeScoped([&](int){++notifications;});
    Computed<int> sum([&]{++recomputes;return a.get()+b.get();},a,b);
    {Batch batch;a.set(3);a.set(3);b.set(4);a.set(5);CHECK(notifications==2);}
    CHECK(sum.get()==9);CHECK(recomputes==2);
    {Batch batch;auto temporary=std::make_unique<Computed<int>>([&]{return a.get()*2;},a);a.set(6);temporary.reset();}
    UiMailbox mailbox;auto sender=mailbox.sender();bool ran=false;
    std::thread worker([&]{CHECK(sender.post([&]{ran=true;}));});worker.join();CHECK(!ran);mailbox.drain();CHECK(ran);
    UiMailbox disposed;auto dead=disposed.sender();dead.post([]{throw std::runtime_error("must discard");});disposed.close();disposed.drain();CHECK(!dead.post([]{}));
    {auto self=std::make_unique<UiMailbox>();self->sender().post([&]{self.reset();});self->sender().post([]{throw std::runtime_error("Destroyed mailbox must stop draining");});self->drain();CHECK(!self);}
    {State<bool> value{false};auto self=std::make_unique<Mount>();self->watch(value,[&](bool on){if(on)self.reset();});value.set(true);self->flush();CHECK(!self);}
    {auto self=std::make_unique<VmCommand>();auto listener=self->running.subscribeScoped([&](bool on){if(on)self.reset();});self->runAsync(sender,[](auto){throw std::runtime_error("Destroyed command must not start work");return std::wstring{};});CHECK(!self);}
    for(bool code:{false,true}) {
        DemoVM vm(sender);int scheduled=0;Mount ui([&]{++scheduled;});ui.styles()->replace(declarativeTheme());
        auto root=code?buildManual(vm,ui):build_Demo(vm,ui);ui.flush();
        CHECK(vm.filtered.get().size()==1000);CHECK(!vm.save.canExecute.get());
        std::vector<std::shared_ptr<Widget>> nodes;flatten(root.widget,nodes);
        auto field=std::find_if(nodes.begin(),nodes.end(),[](auto& w){return w->accessibleName()==L"工作空间名称";});CHECK(field!=nodes.end());
        auto input=std::dynamic_pointer_cast<TextField>(*field);CHECK(input);
        input->onFocusChanged(true);input->setCaretIndex(2);input->setSelectionRange(1,2);
        auto position=input->caretIndex();vm.query.set(L"工作节点 1");ui.flush();CHECK(input->caretIndex()==position);CHECK(input->hasSelection());
        input->setTextComposition(L"中文",1);vm.message.set(L"提示变化");ui.flush();CHECK(input->hasTextComposition());
        ui.styles()->replace(declarativeTheme(true));CHECK(input->hasTextComposition());CHECK(input->hasSelection());
        input->setTextComposition(L"",0);input->onTextCommitted(L"新");CHECK(vm.name.get()==input->text());CHECK(vm.dirty.get());
        vm.interval.set(L"abc");CHECK(!vm.validation.get().empty());CHECK(!vm.save.canExecute.get());
        vm.interval.set(L"60");CHECK(vm.save.canExecute.get());
        vm.query.set(L"no match");ui.flush();CHECK(vm.empty.get());
        vm.add.execute();ui.flush();CHECK(!vm.empty.get());CHECK(vm.rows.get().size()==1001);CHECK(!vm.selectedKey.get().empty());
        vm.remove.execute();ui.flush();CHECK(vm.rows.get().size()==1000);
        auto tableIt=std::find_if(nodes.begin(),nodes.end(),[](auto& w){return bool(std::dynamic_pointer_cast<Table>(w));});CHECK(tableIt!=nodes.end());
        auto table=std::dynamic_pointer_cast<Table>(*tableIt);table->setFrame({0,0,700,220});vm.selectedKey.set(L"20");ui.flush();table->setScrollOffset(400);
        auto changedRows=vm.rows.get();changedRows[19].cells[3]=L"9 ms";vm.rows.set(changedRows);ui.flush();CHECK(vm.selectedKey.get()==L"20");CHECK(table->scrollOffset()==400);
        ui.styles()->replace(declarativeTheme(true));CHECK(table->scrollOffset()==400);CHECK(vm.selectedKey.get()==L"20");
        auto widgetCount=ui.styles()->size();for(int i=0;i<20;++i){ui.styles()->replace(declarativeTheme(i%2));vm.query.set(i%2?L"节点 2":L"");ui.flush();}CHECK(ui.styles()->size()==widgetCount);
        vm.query.set(L"");ui.flush();throws([&]{auto rows=vm.rows.get();rows.push_back(rows.front());vm.rows.set(rows);ui.flush();});
    }
    {Mount ui;State<std::wstring> text{L"first"};auto e=ui.make("Text");ui.bind(e,"text",text);text.set(L"second");text.set(L"last");ui.flush();CHECK(e.as<Label>()->text()==L"last");}
    {Mount ui;auto e=ui.make("Text");ui.styles()->replace(syntax::css("Text {font-size:30px;}"));CHECK(e.as<Label>()->fontSize()==30);ui.styles()->replace(syntax::css("Text {color:#123456;}"));CHECK(e.as<Label>()->fontSize()==14);throws([&]{ui.styles()->replace(syntax::css("Text { padding: 4px; }"));});CHECK(e.as<Label>()->fontSize()==14);}
    {Mount ui;State<std::vector<std::shared_ptr<Tip>>> items{{std::make_shared<Tip>(L"a",L"A"),std::make_shared<Tip>(L"b",L"B")}};
        auto e=ui.repeat(items,[](auto& i){return i->id;},[](Mount& scope,auto i){auto e=scope.make("Input");scope.model(e,i->text);return e;});
        auto stack=e.as<Stack>();auto first=stack->children()[0];CHECK(stack->requestFocus(first.get()));auto input=std::dynamic_pointer_cast<TextField>(first);input->setCaretIndex(1);
        auto values=items.get();std::reverse(values.begin(),values.end());items.set(values);ui.flush();CHECK(stack->children()[1]==first);CHECK(first->focused());CHECK(input->caretIndex()==1);
        values.pop_back();items.set(values);ui.flush();CHECK(!first->focused());throws([&]{values.push_back(values.front());items.set(values);ui.flush();});
    }
    {int scheduled=0;Mount ui([&]{++scheduled;});auto removed=std::make_shared<Tip>(L"a",L"A");State<std::vector<std::shared_ptr<Tip>>> items{{removed}};
        auto e=ui.repeat(items,[](auto& i){return i->id;},[](Mount& scope,auto i){auto e=scope.make("Text");scope.bind(e,"text",i->text);return e;});
        items.set({});ui.flush();auto before=scheduled;removed->text.set(L"Must not schedule removed binding");CHECK(scheduled==before);
        for(int i=0;i<100;++i){items.set({std::make_shared<Tip>(std::to_wstring(i),L"row")});ui.flush();}CHECK(ui.styles()->size()==2);
        throws([&]{items.set({std::make_shared<Tip>(L"",L"bad")});ui.flush();});
    }
    throws([]{syntax::css("Input { display: flex; }");});throws([]{syntax::css("Text Text { color:#fff; }");});throws([]{syntax::css("Text {font-size:}");});
    CHECK(syntax::css("Text {color:#fff;}","abc").find("label.one-Text.abc")!=std::string::npos);
    {Mount ui;ui.setScope("a");auto a=ui.make("Text");ui.setScope("b");auto b=ui.make("Text");ui.styles()->replace(syntax::css("Text {font-size:24px;}","a"));CHECK(a.as<Label>()->fontSize()==24);CHECK(b.as<Label>()->fontSize()==14);}
    {UiMailbox m;VmCommand cmd;std::atomic<int> starts{0};bool finished=false;
        std::promise<void> release;auto gate=release.get_future().share();
        cmd.runAsync(m.sender(),[&](auto){++starts;gate.wait();return std::wstring(L"expected failure");},[&](bool ok){CHECK(!ok);finished=true;});
        CHECK(cmd.running.get());cmd.runAsync(m.sender(),[&](auto){++starts;return std::wstring{};});release.set_value();
        for(int i=0;i<200 && !finished;++i){m.drain();std::this_thread::sleep_for(std::chrono::milliseconds(1));}
        CHECK(finished);CHECK(starts==1);CHECK(!cmd.running.get());CHECK(cmd.error.get()==L"expected failure");
        finished=false;cmd.runAsync(m.sender(),[](auto){return std::wstring{};},[&](bool ok){CHECK(ok);finished=true;});
        for(int i=0;i<200 && !finished;++i){m.drain();std::this_thread::sleep_for(std::chrono::milliseconds(1));}CHECK(finished);CHECK(cmd.error.get().empty());
    }
    {const auto path=std::filesystem::temp_directory_path()/("oneui-style-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".css");
        struct Cleanup{std::filesystem::path path;~Cleanup(){std::error_code ec;std::filesystem::remove(path,ec);}} cleanup{path};
        {std::ofstream f(path);f<<"Text {font-size:18px;}";}
        UiMailbox m;Mount ui;auto label=ui.make("Text");std::string error;int changes=0;
        auto reload=[&]{++changes;try{ui.styles()->replace(syntax::css(readStyleFile(path),{},path.string()));error.clear();}catch(const std::exception& e){error=e.what();}};
        reload();StyleWatcher watcher(m.sender(),{path.string()},reload);
        auto write=[&](const char* text){std::ofstream f(path);f<<text;};
        auto wait=[&](int before){for(int i=0;i<100 && changes==before;++i){std::this_thread::sleep_for(std::chrono::milliseconds(20));m.drain();}CHECK(changes==before+1);};
        write("Text {font-size:21px;}");write("Text {font-size:22px;}");wait(1);CHECK(label.as<Label>()->fontSize()==22);
        write("Text {padding:22px;}");wait(2);CHECK(!error.empty());CHECK(label.as<Label>()->fontSize()==22);
        std::filesystem::remove(path);wait(3);CHECK(!error.empty());CHECK(label.as<Label>()->fontSize()==22);
        write("Text {color:#123456;}");wait(4);CHECK(error.empty());CHECK(label.as<Label>()->fontSize()==14);CHECK(ui.styles()->size()==1);
        m.close();
    }
    {UiMailbox m;auto command=std::make_unique<VmCommand>();std::atomic<bool> completed{false};command->runAsync(m.sender(),[&](auto token){while(!token.cancelled())std::this_thread::yield();completed=true;return std::wstring{};});m.close();command.reset();for(int i=0;i<100 && !completed;++i)std::this_thread::sleep_for(std::chrono::milliseconds(1));CHECK(completed);}
    mailbox.close();std::cout<<"Declarative runtime, lifetime, input, keyed identity and theme tests passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}
