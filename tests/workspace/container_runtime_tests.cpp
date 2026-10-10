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
