// A filtered rich table retains live model leases; omitted visibility stays true.
void richTableVisibilityContract(const char* runtimeFile) {
    const auto path=std::filesystem::path(runtimeFile).parent_path()/"Lists.oneview";
    Case c(path.string().c_str());
    auto& model=c.image.schema.models.at("Item");
    auto property=model.properties.at("selected");
    property.id=0;
    for(const auto& [_,field]:model.properties)property.id=std::max(property.id,field.id);
    ++property.id;property.name="visible";property.access=1;
    model.properties.emplace(property.name,property);model.propertyNames.clear();
    const auto visible=property.id;
    const auto hash=oneview::sha256(c.image.schema.encode());
    for(auto vm:{&c.vm,&c.a,&c.b})std::copy(hash.begin(),hash.end(),vm->table.schema_hash);
    c.a.values[visible].tag=c.b.values[visible].tag=1;
    c.a.values[visible].integer=c.b.values[visible].integer=1;
    c.vm.lists[id(c.image,"Root","spans")]={};
    Vm first(c.image,"Cell"),second(c.image,"Cell");
    first.text(first.key,"name");second.text(second.key,"name");
    const auto text=id(c.image,"Cell","text"),icon=id(c.image,"Cell","icon");
    first.text(text,"Alpha");second.text(text,"Beta");
    const auto cells=id(c.image,"Item","cells");
    c.a.lists[cells]={&first};c.b.lists[cells]={&second};
    c.runtime=std::make_shared<Runtime>(c.image);c.start();
    auto table=c.runtime->root->children[8]->element.as<oneui::Table>();
    const auto row=c.runtime->list(c.runtime->rootScope,c.rows).items[0];
    const auto cell=c.runtime->list(row,cells).items[0];
    const auto reads=first.reads,otherReads=second.reads;
    const auto releases=c.a.releases,cellReleases=first.releases;
    c.a.values[visible].integer=0;c.runtime->notify(row,visible);c.runtime->flush();
    expect(table->richRows().size()==1 && table->richRows()[0][0].text==L"Beta",
           "invisible records do not occupy native table rows");
    expect(first.reads==reads && second.reads==otherReads && c.a.releases==releases && first.releases==cellReleases,
           "visibility-only changes do not reread/release cells");
    table->setSelectedIndices({0});
    oneui::KeyEvent enter;enter.key=oneui::Key::Enter;table->onKeyDown(enter);
    expect(c.vm.invokedArgs[0].text=="b","visible native row events preserve source keys");
    first.text(text,"Updated while hidden");c.runtime->notify(cell,text);c.runtime->flush();
    expect(table->richRows().size()==1 && second.reads==otherReads,"hidden cell changes do not affect visible rows");
    c.a.values[visible].integer=1;c.runtime->notify(row,visible);c.runtime->flush();
    expect(table->richRows().size()==2 && table->richRows()[0][0].text==L"Updated while hidden" && table->selectedIndex()==1,
           "restoring visibility uses the latest values and keeps selected key identity");
    c.a.values[visible].integer=0;c.runtime->notify(row,visible);c.runtime->flush();
    first.text(icon,"InvalidVisibilityIcon");c.runtime->notify(cell,icon);
    c.a.values[visible].integer=1;c.runtime->notify(row,visible);
    try {c.runtime->flush();expect(false,"invalid hidden-cell metadata accepted");}
    catch(const Failure& error) {
        expect(error.result==ONEUI_VIEW_TYPE && table->richRows().size()==1 && table->selectedIndex()==0,
               "failed cell conversion keeps prior visibility, values and selection atomically");
    }
    first.text(icon,"folder");c.runtime->notify(cell,icon);c.runtime->flush();
    expect(table->richRows().size()==2,"a failed visibility change can recover");
    const auto cellReads=first.reads;
    auto mask=[&](bool show) {
        c.a.values[visible].integer=c.b.values[visible].integer=show;
        OneUiUtf8String keys[]={{"a",1},{"b",1}};
        OneUiViewListChange change{};change.struct_size=sizeof(change);change.kind=ONEUI_LIST_UPDATE;
        change.before_revision=c.vm.revision++;change.after_revision=c.vm.revision;
        change.count=change.key_count=2;change.keys=keys;
        change.properties=&visible;change.property_count=1;
        c.runtime->notifyList(c.runtime->rootScope,c.rows,change);c.runtime->flush();
    };
    mask(false);expect(table->richRows().empty(),"a batched update can hide every row");
    mask(true);
    expect(table->richRows().size()==2 && first.reads==cellReads && table->selectedIndex()==1,
           "batched restore retains cells and the selected business key");
    // A remove+insert with a reused key is a new scope, even when its foreign
    // VM pointer is unchanged. It must not take the UPDATE-only lease shortcut.
    Vm replacement(c.image,"Cell");
    replacement.text(replacement.key,"name");replacement.text(text,"Replacement scope");
    c.a.lists[cells]={&replacement};c.vm.lists[c.rows]={&c.b,&c.a};
    auto splice=[&](uint32_t kind,uint64_t index) {
        OneUiUtf8String key{"a",1};
        OneUiViewListChange change{};change.struct_size=sizeof(change);change.kind=kind;
        change.before_revision=c.vm.revision++;change.after_revision=c.vm.revision;
        change.index=index;change.count=change.key_count=1;change.keys=&key;
        c.runtime->notifyList(c.runtime->rootScope,c.rows,change);
    };
    splice(ONEUI_LIST_REMOVE,0);splice(ONEUI_LIST_INSERT,1);c.runtime->flush();
    expect(table->richRows()[1][0].text==L"Replacement scope" && table->selectedIndex()==0,
           "coalesced structural changes reacquire cells and preserve selected key identity");
    c.runtime->close();
    expect(first.retains==first.releases && second.retains==second.releases && replacement.retains==replacement.releases,
           "visible and hidden records release their leases at view close");
}
