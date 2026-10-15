#include "fixture.h"
#include "oneui/controls/tree_view.h"
#include <memory>
using namespace viewtest;
void treeRecordTests(const std::filesystem::path& path) {
    auto image=load(path.string().c_str());
    Vm vm(image,"Navigator");
    const auto nodes=id(image,"Navigator","nodes"), selected=id(image,"Navigator","selected");
    auto field=[&](const char* name) { return id(image,"TreeNode",name); };
    std::vector<std::unique_ptr<Vm>> owned;
    auto node=[&](const std::string& key, const std::string& parent, const std::string& text, bool lazy=false) {
        auto n=std::make_unique<Vm>(image,"TreeNode");
        n->text(n->key,key);n->text(field("parent"),parent);n->text(field("text"),text);
        n->values[field("has_children")].integer=lazy;
        auto result=n.get();owned.push_back(std::move(n));return result;
    };
    auto& items=vm.lists[nodes];
    for(int i=0;i<200;++i) items.push_back(node("db"+std::to_string(i),"","database "+std::to_string(i),true));
    // A filter match on "base" with a non-BMP character before it.
    items[150]->text(field("text"),"\xF0\x9F\x93\xA6 database");
    items[150]->values[field("highlight_start")].integer=6;
    items[150]->values[field("highlight_len")].integer=4;
    items[150]->text(field("icon"),"database");
    vm.text(selected,"db150");
    auto runtime=std::make_shared<Runtime>(image);
    runtime->initialize(&vm.table,&vm);
    auto tree=runtime->root->children[0]->element.as<oneui::TreeView>();
    expect(tree!=nullptr && tree->internalScrolling(),"TreeView is created with internal scrolling");
    tree->setFrame({0,0,300,240});
    expect(tree->visibleItemCount()==200 && tree->selectedId()==L"db150","items and selected-id bind");
    // Selection made in the tree writes the model property back.
    oneui::KeyEvent down;down.key=oneui::Key::Down;
    tree->onKeyDown(down);runtime->flush();
    expect(vm.values[selected].text=="db151","selection writes selected-id");
    tree->setScrollOffset(3000);
    const float scroll=tree->scrollOffset();
    // Expanding a lazy node raises expand(id, true).
    const auto expand=image.schema.model("Navigator").commands.at("expand").id;
    oneui::KeyEvent right;right.key=oneui::Key::Right;
    tree->onKeyDown(right);runtime->flush();
    expect(vm.lastCommand==expand && vm.lastArgs.size()==2 && vm.lastArgs[0].text=="db151" &&
           vm.lastArgs[1].tag==1 && vm.lastArgs[1].integer==1,"expand event carries id/expanded");
    // The VM answers with an incremental INSERT of children (no RESET).
    std::vector<Vm*> children;
    for(int c=0;c<5;++c) children.push_back(node("db151.t"+std::to_string(c),"db151","table "+std::to_string(c)));
    items.insert(items.begin()+152,children.begin(),children.end());
    std::vector<std::string> keys;std::vector<OneUiUtf8String> raw;
    for(auto* child:children) keys.push_back(child->values[child->key].text);
    for(auto& key:keys) raw.push_back({key.data(),key.size()});
    OneUiViewListChange d{};d.struct_size=sizeof(d);d.kind=ONEUI_LIST_INSERT;
    d.before_revision=vm.revision++;d.after_revision=vm.revision;d.index=152;d.count=raw.size();
    d.keys=raw.data();d.key_count=raw.size();
    runtime->notifyList(runtime->rootScope,nodes,d);runtime->flush();
    expect(tree->visibleItemCount()==205,"inserted children are visible under the expanded node");
    expect(tree->isExpanded(L"db151"),"the expanded node stays expanded although its record says false");
    expect(tree->selectedId()==L"db151" && vm.values[selected].text=="db151","INSERT keeps the selection");
    expect(tree->scrollOffset()==scroll,"INSERT keeps the scroll position");
    // Activation and context arrive with typed arguments.
    const auto activate=image.schema.model("Navigator").commands.at("activate").id;
    oneui::KeyEvent enter;enter.key=oneui::Key::Enter;
    tree->onKeyDown(enter);runtime->flush();
    expect(vm.lastCommand==activate && vm.lastArgs.size()==1 && vm.lastArgs[0].text=="db151","activate(id)");
    const auto context=image.schema.model("Navigator").commands.at("context").id;
    oneui::KeyEvent apps;apps.key=oneui::Key::Other;apps.virtualKey=0x5d;
    tree->onKeyDown(apps);runtime->flush();
    expect(vm.lastCommand==context && vm.lastArgs.size()==3 && vm.lastArgs[1].tag==3,"context(id,x,y)");
    // selected-id from the model scrolls to the node.
    vm.text(selected,"db10");
    runtime->notify(runtime->rootScope,selected);runtime->flush();
    expect(tree->selectedId()==L"db10" && tree->scrollOffset()<=10*32.0f,"model selection scrolls into view");
    runtime->close();
    for(auto& v:owned) expect(v->retains==v->releases,"tree record lease balance");
    std::cout << "tree view declarative binding scenarios passed\n";
}
