#include "fixture.h"
#include "oneui/controls/data_grid.h"
#include <memory>
using namespace viewtest;
void dataGridRecordTests(const std::filesystem::path& path) {
    auto image=load(path.string().c_str());
    Vm vm(image,"Grid");
    const auto columns=id(image,"Grid","columns"), rows=id(image,"Grid","rows");
    const auto title=id(image,"GridColumn","title"), width=id(image,"GridColumn","width");
    const auto number=id(image,"GridRow","number"), rowClasses=id(image,"GridRow","classes");
    const auto cells=id(image,"GridRow","cells");
    const auto text=id(image,"GridCell","text"), cellClasses=id(image,"GridCell","classes");
    std::vector<std::unique_ptr<Vm>> owned;
    auto column=[&](const std::string& name) {
        auto c=std::make_unique<Vm>(image,"GridColumn");
        c->text(c->key,name);c->text(title,name);c->values[width].number=120;
        auto result=c.get();owned.push_back(std::move(c));return result;
    };
    auto row=[&](int index) {
        auto r=std::make_unique<Vm>(image,"GridRow");
        r->text(r->key,"r"+std::to_string(index));r->text(number,std::to_string(index+1));
        r->text(rowClasses,index==1?"modified":"");
        auto& list=r->lists[cells];
        for(int c=0;c<2;++c) {
            auto cell=std::make_unique<Vm>(image,"GridCell");
            cell->text(cell->key,"c"+std::to_string(c));
            cell->text(text,c==1&&index==0?"NULL":"v"+std::to_string(index)+"."+std::to_string(c));
            cell->text(cellClasses,c==1&&index==0?"null":"");
            list.push_back(cell.get());owned.push_back(std::move(cell));
        }
        auto result=r.get();owned.push_back(std::move(r));return result;
    };
    vm.lists[columns]={column("id"),column("name")};
    for(int i=0;i<3;++i) vm.lists[rows].push_back(row(i));
    vm.values[id(image,"Grid","total")].integer=1000;
    vm.values[id(image,"Grid","start")].integer=0;
    vm.values[id(image,"Grid","frozen")].integer=1;
    vm.values[id(image,"Grid","editable")].integer=1;
    auto runtime=std::make_shared<Runtime>(image);
    runtime->initialize(&vm.table,&vm);
    auto grid=runtime->root->children[0]->element.as<oneui::DataGrid>();
    expect(grid!=nullptr,"DataGrid is created from the template");
    expect(grid->columns().size()==2 && grid->columns()[1].title==L"name" && grid->columns()[1].width==120,
           "grid-columns projects column records");
    expect(grid->rowCount()==1000 && grid->windowSize()==3,"row-count and grid-rows are separate");
    auto first=grid->row(0);
    expect(first && first->number==L"1" && first->cells.size()==2 && first->cells[1].classes=="null",
           "grid-rows projects nested cell records");
    expect(grid->row(1)->classes=="modified","row classes survive projection");
    // The window start moves independently of the rows themselves.
    vm.values[id(image,"Grid","start")].integer=500;
    runtime->notify(runtime->rootScope,id(image,"Grid","start"));runtime->flush();
    expect(grid->windowStart()==500 && grid->row(0)==nullptr && grid->row(500)->number==L"1",
           "window-start rebases the materialized rows");
    vm.values[id(image,"Grid","start")].integer=0;
    runtime->notify(runtime->rootScope,id(image,"Grid","start"));runtime->flush();
    // Viewport requests reach the view model with typed arguments.
    const auto viewport=image.schema.model("Grid").commands.at("viewport").id;
    grid->setFrame({0,0,600,300});
    grid->scrollToCell(900,0);
    runtime->flush();
    expect(vm.lastCommand==viewport && vm.lastArgs.size()==2 && vm.lastArgs[0].tag==2 &&
           vm.lastArgs[0].integer<=900 && vm.lastArgs[0].integer+vm.lastArgs[1].integer>900,
           "viewport event carries first/count");
    grid->scrollToCell(0,0);
    // In-place edits arrive as edit(row, column, text).
    oneui::DataGridSelection selection;
    selection.anchorRow=selection.focusRow=2;selection.anchorColumn=selection.focusColumn=1;
    selection.kind=oneui::DataGridSelectKind::Cells;
    grid->setSelection(selection);
    expect(grid->beginEdit(),"editable binding enables editing");
    grid->cancelEdit();
    const std::wstring typed=L"new value";
    expect(grid->beginEdit(&typed),"edit starts with typed text");
    oneui::KeyEvent enter;enter.key=oneui::Key::Enter;
    grid->onKeyDown(enter);
    runtime->flush();
    const auto edit=image.schema.model("Grid").commands.at("edit").id;
    expect(vm.lastCommand==edit && vm.lastArgs.size()==3 && vm.lastArgs[0].integer==2 &&
           vm.lastArgs[1].integer==1 && vm.lastArgs[2].tag==4 && vm.lastArgs[2].text=="new value",
           "edit event carries row/column/text");
    // Read-only bindings switch editing off.
    vm.values[id(image,"Grid","editable")].integer=0;
    runtime->notify(runtime->rootScope,id(image,"Grid","editable"));runtime->flush();
    expect(!grid->editable() && !grid->beginEdit(),"editable=false is respected");
    // Updating one cell record re-projects only through the list path.
    vm.lists[rows][0]->lists[cells][0]->text(text,"changed");
    ++vm.revision;runtime->notify(runtime->rootScope,rows);runtime->flush();
    expect(grid->row(0)->cells[0].text==L"changed","list reset refreshes cells");
    runtime->close();
    for(auto& v:owned) expect(v->retains==v->releases,"grid record lease balance");
    std::cout << "data grid declarative binding scenarios passed\n";
}
