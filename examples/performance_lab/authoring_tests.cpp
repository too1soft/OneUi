#include "connection_workspace.hpp"
#include "support/recording_canvas.h"
#include <iostream>
#define CHECK(x) do{if(!(x))throw std::runtime_error("Failed: " #x);}while(0)
using namespace connection_demo;
using CanvasProbe=oneui::test_support::RecordingCanvas;
static bool near(float a,float b){return std::abs(a-b)<.1f;}
static bool sameColor(Color a,Color b){return a.r==b.r && a.g==b.g && a.b==b.b && a.a==b.a;}
static void equalRect(Rect a,Rect b){CHECK(near(a.x,b.x));CHECK(near(a.y,b.y));CHECK(near(a.width,b.width));CHECK(near(a.height,b.height));}
struct Fixture {
    UiMailbox mailbox;VM vm{mailbox.sender()};Connections flow{vm};Mount ui;Page page;
    Fixture(bool compiled):page(compiled?buildTemplate(ui,vm,flow):buildWorkspace(ui,vm,flow)){}
    CanvasProbe paint(){ui.flush();CanvasProbe canvas;page.root.widget->paint(canvas);return canvas;}
};
struct Temporary {
    std::filesystem::path path=std::filesystem::temp_directory_path()/("oneui-authoring-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Temporary(){std::filesystem::create_directories(path);}
    ~Temporary(){std::error_code ignored;if(path.parent_path()==std::filesystem::temp_directory_path() && path.filename().string().rfind("oneui-authoring-",0)==0)std::filesystem::remove_all(path,ignored);}
};
static void write(const std::filesystem::path& file,const std::string& text){std::ofstream out(file,std::ios::binary);out<<text;}
int main(){try {
    Fixture code(false),compiled(true);
    for(bool dark:{false,true})for(int density:{0,1})for(float width:{1320.f,640.f,426.f})for(int scene=0;scene<7;++scene) {
        for(auto* f:{&code,&compiled}) {
            f->flow.prompt.set(false);f->flow.form.set(false);f->flow.detailKey.set({});f->flow.query.set({});
            if(scene==1)f->flow.query.set(L"no results");
            if(scene==2 || scene==3){f->flow.open(L"20");f->vm.port.set(L"70000");f->vm.attempted.set(true);}
            if(scene==3)f->flow.request(Connections::Destination::List);
            if(scene==4){f->flow.selectedKey.set(L"20");f->flow.remove.execute();}
            if(scene>=5) {
                auto data=f->flow.records.get();data[19].config.name=std::wstring(64,L'名');data[19].config.note=std::wstring(200,L'注');
                f->flow.records.set(std::move(data));f->flow.showDetails(L"20");
                if(scene==6){f->flow.edit.execute();f->vm.note.set(L"详情草稿");f->vm.back.execute();}
            }
            const auto spacing=density?Density::Compact:Density::Comfortable;
            f->ui.styles()->replace(declarativeTheme(dark,spacing),spacing);f->page.root.widget->setFrame({0,0,width,800});
        }
        auto a=code.paint(),b=compiled.paint();CHECK(a.texts.size()==b.texts.size());CHECK(a.fillRects.size()==b.fillRects.size());
        for(size_t i=0;i<a.texts.size();++i){if(a.texts[i].text!=b.texts[i].text){auto utf=[](const std::wstring& s){return std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>>{}.to_bytes(s);};throw std::runtime_error("scene="+std::to_string(scene)+" text="+std::to_string(i)+" code="+utf(a.texts[i].text)+" template="+utf(b.texts[i].text));}equalRect(a.texts[i].rect,b.texts[i].rect);CHECK(sameColor(a.texts[i].color,b.texts[i].color));CHECK(near(a.texts[i].size,b.texts[i].size));}
        for(size_t i=0;i<a.fillRects.size();++i){equalRect(a.fillRects[i].rect,b.fillRects[i].rect);CHECK(sameColor(a.fillRects[i].color,b.fillRects[i].color));CHECK(near(a.fillRects[i].radius,b.fillRects[i].radius));}
        CHECK(inspectLayout(code.page.root.widget,*code.ui.styles()).empty());CHECK(inspectLayout(compiled.page.root.widget,*compiled.ui.styles()).empty());
    }
    CHECK(code.ui.diagnostics().subscriptions==compiled.ui.diagnostics().subscriptions);
    CHECK(code.ui.styles()->size()==compiled.ui.styles()->size());
    // Scoped names and component references are shared by both entry points.
    for(auto* f:{&code,&compiled}) {
        Temporary temp;auto css=temp.path/"live.css";write(css,"");
        StyleSession styles{f->ui,f->vm,f->flow,css,true};
        for(auto& source:styles.sourceFiles){auto target=temp.path/std::filesystem::path(source.first).filename();write(target,readStyleFile(source.first));source.first=target.string();}
        CHECK(styles.apply());f->flow.prompt.set(false);f->flow.open(L"20");f->paint();
        auto input=std::dynamic_pointer_cast<TextField>(f->page.fields.at("name"));auto table=std::dynamic_pointer_cast<Table>(f->page.fields.at("$table"));
        f->page.root.as<View>()->requestFocus(input.get());input->setCaretIndex(2);input->setSelectionRange(1,2);input->setTextComposition(L"中文",1);table->setScrollOffset(440);
        const auto text=f->vm.name.get();const auto stats=f->ui.diagnostics();const auto nodes=f->ui.styles()->size();
        auto baseline=f->paint();write(css,"Input { border-radius: 11px; } Section { gap: 20px; }\n");CHECK(styles.apply());auto changed=f->paint();CHECK(styles.validCss.find("11px")!=std::string::npos);
        CHECK(input==f->ui.find("name"));CHECK(input->focused());CHECK(input->caretIndex()==2);CHECK(input->hasSelection());CHECK(input->hasTextComposition());CHECK(f->vm.name.get()==text);CHECK(table->scrollOffset()==440);
        const auto valid=styles.validCss;write(css,"Input { unsupported-property: 1px; }\n");CHECK(!styles.apply());CHECK(styles.validCss==valid);CHECK(f->flow.styleError.get().find(L"live.css")!=std::wstring::npos);
        write(css,"\n\nInput { background-color: var(--missing-token); }");CHECK(!styles.apply());CHECK(styles.validCss==valid);CHECK(f->flow.styleError.get().find(L"live.css:3:1:")!=std::wstring::npos);
        write(css,"");CHECK(styles.apply());auto restored=f->paint();CHECK(restored.fillRects.size()==baseline.fillRects.size());
        for(size_t i=0;i<baseline.fillRects.size();++i){CHECK(near(restored.fillRects[i].radius,baseline.fillRects[i].radius));equalRect(restored.fillRects[i].rect,baseline.fillRects[i].rect);}
        const auto inlineFile=std::find_if(styles.sourceFiles.begin(),styles.sourceFiles.end(),[](const auto& source){return source.second=="scope_Connections_Editor";})->first;
        const auto original=readStyleFile(inlineFile);write(inlineFile,original+"\n<style scoped>Button { border-radius: 13px; }</style>");CHECK(styles.apply());
        auto scopedForm=f->paint();CHECK(std::any_of(scopedForm.fillRects.begin(),scopedForm.fillRects.end(),[](auto& rect){return near(rect.radius,13);}));
        f->flow.form.set(false);auto scopedList=f->paint();CHECK(std::none_of(scopedList.fillRects.begin(),scopedList.fillRects.end(),[](auto& rect){return near(rect.radius,13);}));f->flow.form.set(true);
        CHECK(styles.validCss.find("scope_Connections_Editor")!=std::string::npos);write(inlineFile,original);CHECK(styles.apply());
        for(int i=0;i<50;++i){write(css,i%2?"":"Text { font-size: 15px; }\n");CHECK(styles.apply());f->paint();}
        CHECK(f->ui.styles()->size()==nodes);CHECK(f->ui.diagnostics().subscriptions==stats.subscriptions);CHECK(f->ui.diagnostics().ownedObjects==stats.ownedObjects);
        // Test the actual debounced background watcher against isolated files.
        UiMailbox mailbox;int notifications=0;{
            StyleWatcher watcher(mailbox.sender(),{css.string()},[&]{++notifications;});
            for(int i=0;i<3;++i){write(css,"Text { font-size: "+std::to_string(14+i)+"px; }");std::filesystem::last_write_time(css,std::filesystem::file_time_type::clock::now()+std::chrono::seconds(i+1));std::this_thread::sleep_for(std::chrono::milliseconds(70));mailbox.drain();CHECK(notifications==0);}
            for(int i=0;i<50 && notifications==0;++i){std::this_thread::sleep_for(std::chrono::milliseconds(10));mailbox.drain();}CHECK(notifications==1);
            std::this_thread::sleep_for(std::chrono::milliseconds(250));mailbox.drain();CHECK(notifications==1);
        }mailbox.close();
        bool duplicate=false;try{f->ui.remember("name",input);}catch(const std::invalid_argument&){duplicate=true;}CHECK(duplicate);
        // Imported detail styles use the same scope in C++ and template entries.
        f->flow.form.set(false);f->flow.detailKey.set({});f->flow.showDetails(L"20");f->paint();
        const auto detailFile=std::find_if(styles.sourceFiles.begin(),styles.sourceFiles.end(),[](const auto& source){return source.second=="scope_Connections_Details";})->first;
        const auto detailOriginal=readStyleFile(detailFile);
        write(detailFile,detailOriginal+"\n<style scoped>Section { border-radius: 17px; }</style>");CHECK(styles.apply());
        auto detailCanvas=f->paint();CHECK(std::any_of(detailCanvas.fillRects.begin(),detailCanvas.fillRects.end(),[](auto& rect){return near(rect.radius,17);}));
        CHECK(f->flow.detailKey.get()==L"20");f->flow.detailBack.execute();auto listCanvas=f->paint();
        CHECK(std::none_of(listCanvas.fillRects.begin(),listCanvas.fillRects.end(),[](auto& rect){return near(rect.radius,17);}));
        write(detailFile,detailOriginal);CHECK(styles.apply());
        CHECK(f->ui.diagnostics().subscriptions==stats.subscriptions);CHECK(f->ui.styles()->size()==nodes);
    }
    // Native activation/delete command adapters do nothing after mount teardown.
    auto table=std::make_shared<Table>();VmCommand event;int calls=0;event.setAction([&]{++calls;});table->setRows({{L"row"}});table->setSelectedIndex(0);
    {Mount mount;Element e(Node(table),"DataTable");mount.tableEvent(e,"activate",event);KeyEvent key;key.key=Key::Enter;table->onKeyDown(key);CHECK(calls==1);}
    KeyEvent key;key.key=Key::Enter;table->onKeyDown(key);CHECK(calls==1);
    std::cout<<"Authoring parity: 84 scenes (themes, densities, widths, list/detail/editor/prompt); hot CSS rollback/removal, scoped blocks, input retention, 50 reloads and debounce passed for both entries.\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}
