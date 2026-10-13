// The child accepts a typed IR/VM, never arbitrary native controls.
void childMountContract(const char* path) {
    ApiCase parent(path), child(path);
    parent.start();
    auto name = [](const char* s) { return OneUiUtf8String{s, std::char_traits<char>::length(s)}; };
    expect(oneui_view_mount_set_styles(parent.mount,name("dock-workspace-part.dock-workspace-divider {background:transparent;}"),0)==0,
           "dock material part passes declarative stylesheet validation");
    OneUiViewMount* mount = nullptr;
    expect(oneui_view_mount_child(parent.mount,name("missing"),&mount)==ONEUI_VIEW_NOT_FOUND,
           "child rejects unknown slot");
    expect(oneui_view_mount_child(parent.mount,name("child-slot"),&mount)==0,
           "child reserves declared slot");
    OneUiViewMount* duplicate = nullptr;
    expect(oneui_view_mount_child(parent.mount,name("child-slot"),&duplicate)==ONEUI_VIEW_BUSY,
           "child rejects double reservation before instantiate");
    OneUiView* view = nullptr;
    expect(oneui_view_instantiate(mount,child.bytes.data(),child.bytes.size(),
        &child.vm.table,&child.vm,&view)==0,"child binds independent VM");
    expect(child.vm.retains==1,"child VM is retained");
    expect(oneui_view_flush(parent.view)==0&&oneui_view_flush(view)==0,
           "ordinary parent flush preserves independent child");
    expect(oneui_view_reload(parent.view,parent.bytes.data(),parent.bytes.size())==0,
           "parent reload succeeds");
    expect(oneui_view_flush(view)==ONEUI_VIEW_CLOSED&&child.vm.disconnects==1,
           "parent reload disconnects child before remount");
    oneui_view_destroy(view);
    oneui_view_mount_destroy(mount);
    mount = nullptr;
    expect(oneui_view_mount_child(parent.mount,name("child-slot"),&mount)==0,
           "child slot can be remounted after teardown");
    view = nullptr;
    expect(oneui_view_instantiate(mount,child.bytes.data(),child.bytes.size(),
        &child.vm.table,&child.vm,&view)==0,"child remount binds VM again");
    oneui_view_destroy(parent.view); parent.view=nullptr;
    expect(oneui_view_flush(view)==ONEUI_VIEW_CLOSED,"parent close closes child first");
    expect(child.vm.disconnects==2,"both child lifetimes disconnected exactly once");
    expect(oneui_view_mount_set_styles(mount,name(""),0)==ONEUI_VIEW_CLOSED,
           "child mount cannot outlive parent view");
    oneui_view_destroy(view);
    oneui_view_mount_destroy(mount);
    expect(child.vm.retains==child.vm.releases,"child VM retain/release balanced");
}
