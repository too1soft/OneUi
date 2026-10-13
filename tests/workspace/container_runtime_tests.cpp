#include "../views/fixture.h"
#include "../support/recording_canvas.h"
using namespace viewtest;
using namespace oneui;
int main(int argc,char** argv) {
    try {
        expect(argc==2,"IR argument");auto image=load(argv[1]);Vm vm(image,"QueryRoot");
        vm.values[id(image,"QueryRoot","detailsSelected")].integer=1;
        auto runtime=std::make_shared<Runtime>(image);runtime->initialize(&vm.table,&vm);
        auto root=runtime->root->element.widget;
        auto rows=runtime->root->children[0]->children;
        auto tree=rows[0]->element.widget,browser=rows[1]->element.widget,details=rows[2]->element.widget;
        test_support::RecordingCanvas canvas;
        auto paint=[&](float width){root->setFrame({0,0,width,500});root->paint(canvas);};
        expect(runtime->root->children.size()==2,"root contains panes and toolbar");
        const auto toolbar=runtime->root->children[1];
        expect(toolbar->children.size()==3,"toolbar has three fields");
        const auto navigation=toolbar->children[0]->element.widget;
        const auto search=toolbar->children[1]->element.as<TextField>();
        const auto actions=toolbar->children[2]->element.widget;
        expect(bool(search),"toolbar search is a TextField");
        paint(399);
        expect(search->frame().y>actions->frame().y && actions->frame().y>navigation->frame().y,
               "tiny toolbar has three ordered lines");
        expect(std::abs(search->frame().width-375)<0.1f,"percentage basis fills content width");
        paint(400);
        expect(std::abs(navigation->frame().y-actions->frame().y)<0.1f && search->frame().y>actions->frame().y,
               "medium toolbar moves search to second line");
        paint(620);
        expect(navigation->frame().x<search->frame().x && search->frame().x<actions->frame().x &&
               std::abs(search->frame().width-165)<0.1f,"wide toolbar restores source-independent order and fixed basis");
        paint(399);
        expect(search==toolbar->children[1]->element.as<TextField>() && search->text()==L"retained search",
               "query reordering retains the same text field and input state");
        auto table=rows[1]->children[0]->children[1]->element.as<Table>();
        expect(bool(table),"browser contains a Table");
        table->setColumns({{L"Name",0},{L"Size",120},{L"Modified",180}});
        table->setRows({{L"a"},{L"b"}});table->setSelectedIndex(1);
        paint(619.99f);expect(table->columns().size()==2&&table->columns()[1].width==76,"narrow query projects two columns");
        paint(620);expect(table->columns().size()==3&&table->columns()[1].width==100,"exact query boundary restores three columns");
        paint(619);expect(table->columns().size()==2&&table->selectedIndex()==1,"shrinking columns retains selection");
        paint(859.99f);expect(!tree->visible()&&!browser->visible()&&details->visible(),"ABI selected detail in narrow query");
        auto leases=vm.retains;auto reads=vm.reads;
        paint(860);expect(tree->visible()&&browser->visible()&&!details->visible(),"compiled query enables two panes");
        paint(1100);expect(details->visible(),"compiled query enables details");
        expect(vm.reads==reads&&vm.retains==leases,"layout query has no VM reads, writes or new lease");
        paint(850);vm.values[id(image,"QueryRoot","detailsSelected")].integer=0;
        vm.values[id(image,"QueryRoot","treeSelected")].integer=1;
        runtime->notify(runtime->rootScope,0);runtime->flush();paint(850);
        expect(tree->visible()&&!details->visible(),"VM selection updates retained pages");
        expect(rows[0]->element.widget==tree&&runtime->styles->queryDiagnostics().empty(),"native identity and valid isolation");
        runtime->close();expect(vm.retains==vm.releases,"query VM leases balance");
        std::cout<<"PASS container query IR + VM integration\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
