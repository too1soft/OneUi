#include "fixture.h"
#include <memory>
using namespace viewtest;
void tableRecordTests(const std::filesystem::path& path) {
    oneui::SelectionModel selection;
    selection.setItemCount(6);selection.setMode(oneui::SelectionMode::Multiple);
    selection.applyPointerSelection(1,false,false);selection.applyKeyboardSelection(4,true,false);
    selection.remapItems(7,{0,2,3,4,5,6});
    expect(selection.activeIndex()==5 && selection.anchorIndex()==2 && selection.selectedIndices()==std::vector<int>{2},
           "structural remap retains independent keyboard focus and range anchor");
    selection.applyKeyboardSelection(6,false,true);
    expect(selection.selectedIndices()==std::vector<int>({2,3,4,5,6}),"shift range after structural remap");
    auto image=load(path.string().c_str());
    Vm vm(image,"Records");
    const auto rows=id(image,"Records","rows"), columns=id(image,"Records","columns");
    const auto cells=id(image,"oneui.v1.Row","cells"), value=id(image,"oneui.v1.StringItem","value");
    vm.lists[columns]={};
    auto& items=vm.lists[rows];
    std::vector<std::unique_ptr<Vm>> owned;
    auto add=[&](const std::string& key) {
        auto cell=std::make_unique<Vm>(image,"oneui.v1.StringItem");
        cell->text(cell->key,"cell");cell->text(value,key);
        auto row=std::make_unique<Vm>(image,"oneui.v1.Row");
        row->text(row->key,key);row->items={cell.get()};
        auto result=row.get();owned.push_back(std::move(cell));owned.push_back(std::move(row));return result;
    };
    for(int i=0;i<30;++i) items.push_back(add("row"+std::to_string(i)));
    auto runtime=std::make_shared<Runtime>(image);
    runtime->initialize(&vm.table,&vm);
    auto a=runtime->root->children[0]->element.as<oneui::Table>();
    auto b=runtime->root->children[1]->element.as<oneui::Table>();
    for(auto t:{a,b}) {
        t->setFrame({0,0,500,160});t->setSelectionMode(oneui::SelectionMode::Multiple);
        t->setSelectedIndices({3,7});t->onFocusChanged(true);t->setScrollOffset(80);
    }
    expect(!a->eraseRows(100,1) && !a->moveRows(0,1,100) && !a->insertRows(100,{{L"bad"}}),
           "invalid structural operations leave rows unchanged");
    auto equal=[&] {
        expect(a->focused() && b->focused(),"structural updates retain native focus");
        expect(a->rows()==b->rows(),"two bindings see identical versioned records");
        expect(a->rows().size()==items.size(),"row count");
        for(size_t i=0;i<items.size();++i)
            expect(a->rows()[i][0]==oneui::ui::wide(items[i]->items[0]->values[value].text),"row contents");
    };
    auto delta=[&](uint32_t kind,size_t index,std::vector<std::string> keys,size_t to=0,
                   std::vector<uint32_t> fields=std::vector<uint32_t>{}) {
        std::vector<OneUiUtf8String> raw;
        for(auto& key:keys) raw.push_back({key.data(),key.size()});
        OneUiViewListChange d{};d.struct_size=sizeof(d);d.kind=kind;
        d.before_revision=vm.revision++;d.after_revision=vm.revision;
        d.index=index;d.count=keys.size();d.to_index=to;d.keys=raw.data();d.key_count=raw.size();
        d.properties=fields.data();d.property_count=fields.size();
        runtime->notifyList(runtime->rootScope,rows,d);
    };
    auto flush=[&] { auto before=vm.infos[rows];runtime->flush();expect(vm.infos[rows]==before+1,"one list_info per commit across consumers");equal(); };
    items[0]->items[0]->text(value,"updated");
    auto unrelated=items[1]->items[0]->reads[value];
    auto leases=vm.leases[rows],keys=vm.keys[rows];
    delta(ONEUI_LIST_UPDATE,0,{"row0"},0,{cells});flush();
    expect(items[1]->items[0]->reads[value]==unrelated,"UPDATE does not read unrelated fields");
#ifdef NDEBUG
    expect(vm.leases[rows]==leases && vm.keys[rows]==keys,"precise UPDATE no key/acquire");
#endif
    auto extra=add("inserted");items.insert(items.begin(),extra);
    leases=vm.leases[rows];
    delta(ONEUI_LIST_INSERT,0,{"inserted"});flush();
#ifdef NDEBUG
    expect(vm.leases[rows]==leases+1,"INSERT acquires only its new row");
#endif
    expect(a->selectedIndices()==std::vector<int>({4,8}) && a->scrollOffset()==80,"INSERT selection/scroll");
    auto moving=items[4];items.erase(items.begin()+4);items.insert(items.begin()+20,moving);
    delta(ONEUI_LIST_MOVE,4,{"row3"},20);flush();
    expect(a->selectedIndices()==std::vector<int>({7,20}) && a->scrollOffset()==80,"MOVE stable selected rows");
    items.erase(items.begin());delta(ONEUI_LIST_REMOVE,0,{"inserted"});flush();
    expect(extra->releases==extra->retains,"REMOVE releases row");
    expect(a->selectedIndices()==std::vector<int>({6,19}),"REMOVE remaps selection");
    // Independent cell notifications remain supported: this is why record
    // leases have a notifier even though they do not create child Widgets.
    auto rootList=runtime->rootScope->lists.at(rows);
    auto first=rootList.items[0];auto cell=first->lists.at(cells).items[0];
    items[0]->items[0]->text(value,"cell-notify");
    auto infos=vm.infos[rows];
    runtime->notify(cell,value);runtime->flush();equal();
    expect(vm.infos[rows]==infos,"scalar child notification does not refresh parent list");
    // A failed conversion/read must not advance either native consumer/cache.
    auto previous=a->rows();items[0]->items[0]->text(value,"retry");
    items[0]->items[0]->onGet=[] {throw std::bad_alloc();};
    runtime->notify(cell,value);
    try {runtime->flush();expect(false,"fault expected");}catch(const std::bad_alloc&){}
    expect(a->rows()==previous && b->rows()==previous,"failed read preserves both tables");
    items[0]->items[0]->onGet={};runtime->flush();equal();
    // RESET same key/new VM must invalidate the typed cache, not reuse old cells.
    items[0]=add(items[0]->values[items[0]->key].text);
    items[0]->items[0]->text(value,"replacement");
    ++vm.revision;runtime->notify(runtime->rootScope,rows);runtime->flush();equal();
    int connections=0;for(auto& v:owned) connections+=v->connects;
    for(int i=0;i<50;++i) runtime->reload(image);
    int after=0;for(auto& v:owned) after+=v->connects;
    expect(connections==after,"50 reloads create no additional record connections");
    expect(runtime->root->children[0]->element.widget==a,"reload retains Table identity");
    items[0]->items[0]->text(value,"after-reload");
    delta(ONEUI_LIST_UPDATE,0,{items[0]->values[items[0]->key].text},0,{cells});flush();
    expect(a->scrollOffset()==80,"reload and update preserve scroll");
    // Insert then remove in one submission: no transient VM is acquired.
    delta(ONEUI_LIST_INSERT,0,{"transient"});delta(ONEUI_LIST_REMOVE,0,{"transient"});flush();
    runtime->close();
    for(auto& v:owned) expect(v->retains==v->releases && v->connects==v->disconnects,"record lease/notifier balance");
    std::cout << "11 typed record/delta/reload scenarios passed\n";
}
