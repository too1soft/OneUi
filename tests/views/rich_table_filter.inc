void tableProjectionContract() {
    oneui::Table table;
    using Row=std::vector<oneui::TableCell>;
    std::vector<std::shared_ptr<const Row>> source;
    for(int i=0;i<3;++i) {
        oneui::TableCell cell;cell.text=std::wstring(100,static_cast<wchar_t>(L'a'+i));cell.detail=L"detail";
        source.push_back(std::make_shared<const Row>(Row{cell}));
    }
    expect(table.projectRichRows(source,{0,1,2},{}),"initial immutable row projection");
    table.setSelectionMode(oneui::SelectionMode::Multiple);table.setSelectedIndices({1});
    const auto first=table.richRows()[0].data(),last=table.richRows()[2].data();
    for(int i=0;i<30;++i) {
        expect(table.projectRichRows(source,{1},{-1,0,-1}) && table.selectedIndex()==0,
               "filter remaps selected business row");
        expect(table.projectRichRows(source,{0,1,2},{1}) && table.selectedIndex()==1,
               "restore remaps selected business row");
        expect(table.richRows()[0].data()==first && table.richRows()[2].data()==last,
               "repeated filter restore moves existing cell storage");
    }
    expect(!table.projectRichRows(source,{0,0},{0,1,-1}) &&
           !table.projectRichRows(source,{3},{-1,-1,0}) &&
           !table.projectRichRows(source,{1},{0,0,-1}) &&
           !table.projectRichRows(source,{1},{}) && table.richRows()[2].data()==last,
           "invalid projection and identity mappings preserve published storage");
    auto invalid=source;invalid[0].reset();
    expect(!table.projectRichRows(invalid,{1},{-1,0,-1}) && table.rows().size()==3,
           "null snapshots reject before mutation");
    table.projectRichRows(source,{1},{-1,0,-1});
    source[2]=std::make_shared<const Row>(Row{oneui::TableCell{L"updated while hidden"}});
    expect(table.projectRichRows(source,{0,1,2},{1}) &&
           table.rows()[2][0]==L"updated while hidden" && table.richRows()[0].data()==first,
           "changed hidden snapshots replace only their own payload");
    for(int operation=0;operation<8;++operation) {
        table.projectRichRows(source,{0,1,2},std::vector<int>(table.rows().size(),-1));
        switch(operation) {
        case 0:table.setRows({{L"manual"}});break;
        case 1:table.setRichRows({{oneui::TableCell{L"manual"}}});break;
        case 2:table.updateRow(0,{L"manual"});break;
        case 3:table.updateRichRow(0,{oneui::TableCell{L"manual"}});break;
        case 4:table.insertRows(0,{{L"manual"}});break;
        case 5:table.eraseRows(0,1);break;
        case 6:table.moveRows(0,1,2);break;
        case 7:table.reconcileRichRows({{oneui::TableCell{L"manual"}}},{0,-1,-1});break;
        }
        expect(table.projectRichRows(source,{0,1,2},std::vector<int>(table.rows().size(),-1)) &&
               table.rows()[0][0]==source[0]->at(0).text && table.rows()[2][0]==L"updated while hidden",
               "legacy row mutations invalidate retained projection payloads");
    }
    std::weak_ptr<const Row> hidden=source[2];
    table.projectRichRows(source,{0},{0,-1,-1});source.resize(1);
    expect(table.projectRichRows(source,{0},{0}) && hidden.expired(),
           "source pruning releases hidden snapshots and payloads");
    expect(table.projectRichRows({}, {}, {-1}) && table.rows().empty(),"empty source clears projection");
}
void richTableFilterContract(const char* runtimeFile) {
    tableProjectionContract();
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
