#include "connection_workspace.hpp"
#include "support/recording_canvas.h"
#include <iostream>
#define CHECK(x) do {if(!(x))throw std::runtime_error("Failed: " #x);}while(0)
using namespace connection_demo;
using Destination=Connections::Destination;

std::set<const Widget*> widgets(const std::shared_ptr<Widget>& root) {
    std::set<const Widget*> result{root.get()};
    if(auto view=std::dynamic_pointer_cast<View>(root))for(auto& child:view->children()) {
        auto descendants=widgets(child);result.insert(descendants.begin(),descendants.end());
    }
    return result;
}
int main(int argc,char** argv){try {
    const bool useTemplate=argc>1 && std::string(argv[1])=="--template";
    UiMailbox mailbox;VM vm(mailbox.sender());Connections flow(vm);Mount ui;auto page=useTemplate?buildTemplate(ui,vm,flow):buildWorkspace(ui,vm,flow);
    auto table=std::dynamic_pointer_cast<Table>(page.fields.at("$table"));
    auto paint=[&]{ui.flush();oneui::test_support::RecordingCanvas canvas;page.root.widget->paint(canvas);return canvas.texts.size();};
    auto finish=[&]{for(int i=0;i<300 && vm.save.running.get();++i){std::this_thread::sleep_for(std::chrono::milliseconds(5));mailbox.drain();}CHECK(!vm.save.running.get());paint();};
    for(bool dark:{false,true})for(float width:{1320.f,640.f,426.f}) {
        ui.styles()->replace(declarativeTheme(dark));page.root.widget->setFrame({0,0,width,700});
        const auto drawn=paint();CHECK(drawn<100); // Native table draws visible rows, not all 1,000.
        auto issues=inspectLayout(page.root.widget,*ui.styles());if(!issues.empty())throw std::runtime_error(formatLayoutIssues(issues));
    }
    CHECK(flow.records.get().size()==1000);CHECK(table->rows().size()==1000);
    std::set<std::wstring> ids;for(auto& row:flow.records.get())CHECK(ids.insert(row.id).second);
    UiMailbox secondMailbox;VM second(secondMailbox.sender());Connections secondFlow(second);CHECK(flow.records.get()==secondFlow.records.get());
    page.root.widget->setFrame({0,0,1320,900});paint();flow.selectedKey.set(L"20");paint();table->setScrollOffset(440);
    auto original=flow.find(L"20")->config;
    const auto unrelatedRow=&table->rows()[40];
    flow.edit.execute();paint();CHECK(flow.form.get());vm.name.set(L"上海更新节点 20");vm.save.execute();finish();
    CHECK(!flow.form.get());CHECK(flow.find(L"20")->config.name==L"上海更新节点 20");
    CHECK(flow.selectedKey.get()==L"20");CHECK(table->scrollOffset()==440);CHECK(&table->rows()[40]==unrelatedRow);
    CHECK(table->rows()[19][0]==L"上海更新节点 20");
    flow.filter.set(flow.find(L"20")->online?1:2);flow.query.set(L"20");paint();CHECK(flow.selectedKey.get()==L"20");
    flow.edit.execute();vm.note.set(L"筛选中编辑");vm.save.execute();finish();CHECK(flow.query.get()==L"20");CHECK(flow.filter.get()!=0);
    flow.open(L"20");vm.name.set(L"保存时的名称");vm.save.execute();vm.name.set(L"保存之后继续输入");finish();
    CHECK(flow.form.get());CHECK(vm.dirty.get());CHECK(flow.find(L"20")->config.name==L"保存时的名称");
    flow.request(Destination::List);paint();CHECK(flow.prompt.get());flow.keep.execute();paint();CHECK(flow.form.get());CHECK(vm.name.get()==L"保存之后继续输入");
    flow.request(Destination::List);flow.discard.execute();paint();CHECK(!flow.form.get());CHECK(!flow.prompt.get());
    // Failed save keeps the draft and store untouched, retry returns to the list.
    flow.open(L"20");vm.port.set(L"8443");vm.failNext.execute();vm.save.execute();finish();
    CHECK(flow.form.get());CHECK(!vm.save.error.get().empty());CHECK(flow.find(L"20")->config.port==original.port);
    vm.save.execute();finish();CHECK(!flow.form.get());CHECK(flow.find(L"20")->config.port==L"8443");
    // A renamed record can leave the active filter without clearing the filter.
    flow.query.set(L"保存时的名称");flow.filter.set(0);paint();flow.selectedKey.set(L"20");paint();flow.edit.execute();
    vm.name.set(L"筛选之外的新名称");vm.save.execute();finish();CHECK(flow.empty.get());CHECK(flow.selectedKey.get().empty());CHECK(flow.query.get()==L"保存时的名称");
    flow.clear.execute();paint();
    const auto initialSize=flow.records.get().size();flow.add.execute();paint();CHECK(flow.needsGuard());flow.request(Destination::List);flow.discard.execute();paint();CHECK(flow.records.get().size()==initialSize);
    flow.add.execute();vm.name.set(L"新增连接");vm.save.execute();finish();CHECK(flow.records.get().size()==initialSize+1);auto newId=flow.selectedKey.get();CHECK(!newId.empty());CHECK(newId!=L"1001");
    flow.remove.execute();paint();CHECK(flow.deleting.get());flow.keep.execute();CHECK(flow.find(newId));
    flow.remove.execute();flow.confirmDelete.execute();paint();CHECK(!flow.find(newId));CHECK(flow.records.get().size()==initialSize);CHECK(!flow.selectedKey.get().empty());
    CHECK(table->scrollOffset()>=0 && table->scrollOffset()<=table->maxScrollOffset());
    // Native activation opens a stable-ID read-only detail; edits return to their origin.
    flow.selectedKey.set(L"20");paint();table->setScrollOffset(440);
    KeyEvent activate;activate.key=Key::Enter;table->onKeyDown(activate);paint();
    CHECK(flow.showDetail.get());CHECK(!flow.showList.get());CHECK(flow.detailKey.get()==L"20");
    CHECK(flow.detailName.get()==flow.find(L"20")->config.name);
    flow.edit.execute();paint();CHECK(flow.form.get());CHECK(vm.backText.get()==L"返回连接详情");
    vm.name.set(L"详情中保存的名称");vm.save.execute();finish();
    CHECK(flow.showDetail.get());CHECK(flow.detailName.get()==L"详情中保存的名称");CHECK(!flow.form.get());
    CHECK(table->scrollOffset()==440);CHECK(flow.selectedKey.get()==L"20");
    flow.edit.execute();vm.note.set(L"详情未保存草稿");vm.back.execute();paint();CHECK(flow.prompt.get());
    flow.keep.execute();paint();CHECK(flow.form.get());CHECK(vm.note.get()==L"详情未保存草稿");
    vm.back.execute();flow.discard.execute();paint();CHECK(flow.showDetail.get());CHECK(flow.detailNote.get()!=L"详情未保存草稿");
    flow.edit.execute();vm.note.set(L"详情保存失败重试");vm.failNext.execute();vm.save.execute();finish();
    CHECK(flow.showForm.get());CHECK(!vm.save.error.get().empty());vm.save.execute();finish();CHECK(flow.showDetail.get());
    flow.remove.execute();paint();flow.keep.execute();paint();CHECK(flow.showDetail.get());
    // A detail remains addressable even if the edited record leaves the retained filter.
    flow.query.set(L"详情中保存的名称");paint();flow.edit.execute();vm.name.set(L"已离开筛选的详情");vm.save.execute();finish();
    CHECK(flow.showDetail.get());CHECK(flow.detailName.get()==L"已离开筛选的详情");CHECK(flow.selectedKey.get().empty());CHECK(flow.empty.get());CHECK(flow.edit.canExecute.get());
    flow.detailBack.execute();paint();CHECK(flow.showList.get());CHECK(flow.query.get()==L"详情中保存的名称");
    flow.clear.execute();flow.selectedKey.set(L"21");paint();flow.view.execute();flow.remove.execute();flow.confirmDelete.execute();paint();
    CHECK(flow.showList.get());CHECK(!flow.find(L"21"));CHECK(flow.detailKey.get().empty());
    // All leaving paths use the same guard; closing while saving is cancellable.
    bool closed=false,back=false;flow.closeWindow=[&]{closed=true;};flow.backToLab=[&]{back=true;};
    flow.open(L"20");vm.note.set(L"未保存");flow.request(Destination::Lab);CHECK(flow.prompt.get());CHECK(!back);flow.keep.execute();
    flow.request(Destination::Close);CHECK(flow.prompt.get());CHECK(!closed);flow.keep.execute();
    flow.request(Destination::Lab);flow.saveLeave.execute();finish();CHECK(back);CHECK(!flow.form.get());
    flow.open(L"20");vm.note.set(L"关闭前失败");flow.request(Destination::Close);vm.failNext.execute();flow.saveLeave.execute();finish();CHECK(!closed);CHECK(flow.form.get());
    vm.save.execute();finish();CHECK(!closed);CHECK(!flow.form.get()); // Retry never unexpectedly closes the window.
    flow.open(L"20");vm.note.set(L"关闭过程中保存");vm.save.execute();flow.request(Destination::Close);CHECK(flow.prompt.get());finish();CHECK(flow.prompt.get());CHECK(!closed);flow.discard.execute();CHECK(closed);
    // Cycling never reconstructs pages, fields, table or mounted subscriptions.
    flow.form.set(false);flow.prompt.set(false);flow.clear.execute();paint();
    const auto identity=widgets(page.root.widget);const auto stats=ui.diagnostics();const auto styles=ui.styles()->size();
    for(int i=0;i<300;++i) {
        flow.query.set(i%2?L"节点":L"");flow.filter.set(i%3);paint();
        if(!flow.filtered.get().empty()) {
            flow.selectedKey.set(flow.filtered.get().front().id);paint();flow.view.execute();paint();CHECK(flow.showDetail.get());flow.edit.execute();paint();
            vm.note.set(L"循环草稿 "+std::to_wstring(i));vm.back.execute();paint();flow.discard.execute();paint();CHECK(flow.showDetail.get());flow.detailBack.execute();paint();
        }
        CHECK(widgets(page.root.widget)==identity);CHECK(ui.styles()->size()==styles);
        CHECK(ui.diagnostics().subscriptions==stats.subscriptions);CHECK(ui.diagnostics().ownedObjects==stats.ownedObjects);CHECK(ui.diagnostics().pendingUpdates==0);
    }
    // Duplicate IDs are rejected before a keyed table mutates.
    auto native=std::make_shared<Table>();KeyedTable keyed(native);bool rejected=false;
    try{keyed.update({{L"same",{L"a"}},{L"same",{L"b"}}});}catch(const std::invalid_argument&){rejected=true;}CHECK(rejected);
    flow.clear.execute();flow.records.set({});paint();CHECK(flow.empty.get());CHECK(flow.selectedKey.get().empty());CHECK(!flow.edit.canExecute.get());CHECK(table->scrollOffset()==0);
    flow.add.execute();vm.save.execute();mailbox.close(); // Worker owns no page or window.
    std::cout<<"Connection workflow tests passed: 300 cycles, "<<identity.size()<<" widgets, "<<stats.subscriptions<<" mount subscriptions, "<<styles<<" style nodes; no growth.\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 1;}}
