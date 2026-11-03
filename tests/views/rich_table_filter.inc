void richTableFilterContract(const char* runtimeFile) {
    const auto path=std::filesystem::path(runtimeFile).parent_path()/"Lists.oneview";
    Case c(path.string().c_str());
    auto& root=c.image.schema.models.at("Root");
    uint32_t next=0;for(const auto& [_,p]:root.properties)next=std::max(next,p.id);
    auto flag=root.properties.at("rows");flag.id=++next;flag.name="rows_filter_active";
    flag.tag=1;flag.item=0;flag.itemName.clear();flag.access=1;
    root.properties.emplace(flag.name,flag);
    auto keys=root.properties.at("rows");keys.id=++next;keys.name="rows_filter_keys";keys.access=1;
    root.properties.emplace(keys.name,keys);root.propertyNames.clear();
    const auto hash=oneview::sha256(c.image.schema.encode());
    for(auto vm:{&c.vm,&c.a,&c.b})std::copy(hash.begin(),hash.end(),vm->table.schema_hash);
    c.vm.values[flag.id].tag=1;c.vm.values[flag.id].integer=0;c.vm.lists[keys.id]={};
    c.vm.lists[id(c.image,"Root","spans")]={};
    Vm first(c.image,"Cell"),second(c.image,"Cell"),unknown(c.image,"Item");
    first.text(first.key,"name");second.text(second.key,"name");unknown.text(unknown.key,"missing");
    const auto text=id(c.image,"Cell","text"),cells=id(c.image,"Item","cells");
    first.text(text,"Alpha");second.text(text,"Beta");
    c.a.lists[cells]={&first};c.b.lists[cells]={&second};
    const auto selected=id(c.image,"Item","selected");
    c.b.values[selected].tag=1;c.b.values[selected].integer=1;
    c.runtime=std::make_shared<Runtime>(c.image);c.start();
    auto table=c.runtime->root->children[8]->element.as<oneui::Table>();
    const auto source=c.runtime->list(c.runtime->rootScope,c.rows).items[0];
    const auto cell=c.runtime->list(source,cells).items[0];
    const auto reads=first.reads,otherReads=second.reads;
    c.vm.values[flag.id].integer=1;c.vm.lists[keys.id]={&c.b};
    c.runtime->notify(c.runtime->rootScope,flag.id);c.runtime->notify(c.runtime->rootScope,keys.id);c.runtime->flush();
    expect(table->richRows().size()==1 && table->richRows()[0][0].text==L"Beta" &&
           first.reads==reads && second.reads==otherReads,"key filtering reuses retained cell snapshots");
    oneui::KeyEvent enter;enter.key=oneui::Key::Enter;table->onKeyDown(enter);
    expect(!c.vm.invokedArgs.empty() && c.vm.invokedArgs[0].text=="b","key-filtered events retain the business key");
    first.text(text,"Updated hidden row");c.runtime->notify(cell,text);c.runtime->flush();
    c.vm.values[flag.id].integer=0;c.runtime->notify(c.runtime->rootScope,flag.id);c.runtime->flush();
    expect(table->richRows().size()==2 && table->richRows()[0][0].text==L"Updated hidden row" && table->selectedIndex()==1,
           "clearing the key filter restores updated values and selected identity");
    c.vm.values[flag.id].integer=1;c.vm.lists[keys.id]={};
    c.runtime->notify(c.runtime->rootScope,flag.id);c.runtime->notify(c.runtime->rootScope,keys.id);c.runtime->flush();
    expect(table->richRows().empty(),"an active empty key filter hides all rows");
    c.vm.lists[keys.id]={&unknown};c.runtime->notify(c.runtime->rootScope,keys.id);
    try {c.runtime->flush();expect(false,"unknown filter key accepted");}
    catch(const Failure& error) {expect(error.result==ONEUI_VIEW_BAD_KEY && table->richRows().empty(),
                                      "invalid key filters leave the published table unchanged");}
    c.vm.lists[keys.id]={&c.a};c.runtime->notify(c.runtime->rootScope,keys.id);c.runtime->flush();
    expect(table->richRows().size()==1 && table->richRows()[0][0].text==L"Updated hidden row",
           "invalid key filters recover with corrected metadata");
    c.runtime->close();
    expect(first.retains==first.releases && second.retains==second.releases && unknown.retains==unknown.releases,
           "source and filter records release all leases");
}
