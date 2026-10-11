void richTableReorderContract(const char* runtimeFile) {
    const auto path=std::filesystem::path(runtimeFile).parent_path()/"Lists.oneview";
    Case c(path.string().c_str());
    c.vm.lists[id(c.image,"Root","spans")]={};
    Vm first(c.image,"Cell"),second(c.image,"Cell");
    first.text(first.key,"name");second.text(second.key,"name");
    const auto text=id(c.image,"Cell","text"),icon=id(c.image,"Cell","icon");
    first.text(text,"Alpha");second.text(text,"Beta");
    const auto cells=id(c.image,"Item","cells");
    c.a.lists[cells]={&first};c.b.lists[cells]={&second};c.start();
    auto table=c.runtime->root->children[8]->element.as<oneui::Table>();
    table->setSelectedIndices({0});
    const auto row=c.runtime->list(c.runtime->rootScope,c.rows).items[0];
    const auto cell=c.runtime->list(row,cells).items[0];
    auto reorder=[&](std::vector<std::string> names) {
        std::vector<OneUiUtf8String> keys;
        for(auto& name:names)keys.push_back({name.data(),name.size()});
        OneUiViewListChange d{};d.struct_size=sizeof(d);d.kind=ONEUI_LIST_REORDER;
        d.before_revision=c.vm.revision++;d.after_revision=c.vm.revision;
        d.count=d.key_count=keys.size();d.keys=keys.data();
        c.runtime->notifyList(c.runtime->rootScope,c.rows,d);
    };
    const auto reads=first.reads,otherReads=second.reads;
    [[maybe_unused]] const auto leases=c.vm.acquires;
    c.vm.items={&c.b,&c.a};reorder({"b","a"});c.runtime->flush();
    expect(first.reads==reads && second.reads==otherReads,
           "pure permutation never rereads/rebuilds rich cell records");
#ifdef NDEBUG
    expect(c.vm.acquires==leases,"release permutation retains existing leases without reacquiring");
#endif
    expect(c.runtime->list(c.runtime->rootScope,c.rows).items[1]==row &&
           table->richRows()[1][0].text==L"Alpha" && table->selectedIndex()==1,
           "permutation retains row lease and selection anchor identity");
    first.text(text,"Updated");c.runtime->notify(cell,text);
    c.vm.items={&c.a,&c.b};reorder({"a","b"});c.runtime->flush();
    expect(table->richRows()[0][0].text==L"Updated" && table->selectedIndex()==0,
           "same-key metadata notification survives concurrent permutation");
    for(auto keys:std::vector<std::vector<std::string>>{{"a"},{"a","b","extra"},{"a","missing"},{"a","a"}}) {
        try {reorder(keys);c.runtime->flush();expect(false,"invalid permutation accepted");}
        catch(const Failure& error) {
            expect(error.result==ONEUI_VIEW_BAD_KEY && table->richRows()[0][0].text==L"Updated" &&
                   table->richRows().size()==2 && table->selectedIndex()==0,
                   "missing/extra/unknown/duplicate permutation keys fail atomically");
        }
        c.runtime->notify(c.runtime->rootScope,c.rows);c.runtime->flush();
    }
    first.text(icon,"UnknownIconForValidation");c.runtime->notify(cell,icon);
    c.vm.items={&c.b,&c.a};reorder({"b","a"});
    try {c.runtime->flush();expect(false,"invalid cell with permutation accepted");}
    catch(const Failure& error) {
        expect(error.result==ONEUI_VIEW_TYPE && table->richRows()[0][0].text==L"Updated" &&
               table->selectedIndex()==0,"failed metadata keeps prior rows/order/selection");
    }
    first.text(icon,"folder");c.runtime->notify(c.runtime->rootScope,c.rows);c.runtime->flush();
    c.vm.items={&c.b};++c.vm.revision;c.runtime->notify(c.runtime->rootScope,c.rows);c.runtime->flush();
    expect(table->richRows().size()==1 && table->richRows()[0][0].text==L"Beta" &&
           table->selectedIndices().empty(),"removing the selected key clears its cached selection");
    c.runtime->close();
    expect(first.retains==first.releases && second.retains==second.releases,
           "reorder, rollback and deletion release all cell leases");
}
