// Structured hierarchy and expansion use the same guarded VM leases as rows.
void treeContract(const char* runtimeFile) {
    const auto path=std::filesystem::path(runtimeFile).parent_path()/"Lists.oneview";
    Case c(path.string().c_str());
    c.vm.lists[id(c.image,"Root","spans")]={};
    const auto parent=id(c.image,"Item","parent");
    const auto expanded=id(c.image,"Item","expanded");
    const auto selected=id(c.image,"Root","tree_selected");
    c.a.text(id(c.image,"Item","title"),"Directory");
    c.b.text(id(c.image,"Item","title"),"Child");
    c.b.text(parent,"a");c.a.values[expanded].integer=1;
    c.vm.text(selected,"a");
    c.start();
    auto tree=c.runtime->root->children[7]->element.as<oneui::TreeView>();
    tree->setFrame({0,0,300,200});
    expect(tree && tree->visibleItemCount()==2 && tree->selectedId()==L"a",
           "TreeView reads hierarchy and selected stable key");
    oneui::KeyEvent down;down.key=oneui::Key::Down;
    tree->onKeyDown(down);
    expect(c.vm.values[selected].text=="b","keyboard tree selection writes stable key");
    c.runtime->notify(c.runtime->rootScope,selected);c.runtime->flush();
    c.vm.text(selected,"a");c.runtime->notify(c.runtime->rootScope,selected);
    c.runtime->flush();
    oneui::KeyEvent left;left.key=oneui::Key::Left;
    tree->onKeyDown(left);
    expect(tree->visibleItemCount()==1 && !c.a.values[expanded].integer && c.a.sets==1,
           "collapsing tree writes expanded on its item lease exactly once");
    const auto rowScope=c.runtime->list(c.runtime->rootScope,c.rows).items[0];
    c.runtime->notify(rowScope,expanded);c.runtime->flush();
    expect(!tree->isExpanded(L"a") && c.a.sets==1,"record refresh retains collapsed state without echo");
    c.a.values[expanded].integer=1;c.runtime->notify(rowScope,expanded);c.runtime->flush();
    expect(tree->visibleItemCount()==2 && c.a.sets==1,"VM expansion updates native tree without echo");
    tree->setDisabled(true);tree->onKeyDown(left);
    expect(c.a.sets==1,"disabled tree ignores input");
    c.runtime->close();tree->setDisabled(false);tree->onKeyDown(left);
    expect(c.a.sets==1,"retained tree cannot mutate disconnected item leases");
    expect(c.a.retains==c.a.releases && c.b.retains==c.b.releases,"tree leases release on close");
}
