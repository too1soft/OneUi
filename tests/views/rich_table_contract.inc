void richTableContract(const char* runtimeFile) {
    const auto path=std::filesystem::path(runtimeFile).parent_path()/"Lists.oneview";
    Case c(path.string().c_str());
    c.vm.lists[id(c.image,"Root","spans")]={};
    Vm first(c.image,"Cell"),second(c.image,"Cell");
    first.text(first.key,"name");second.text(second.key,"name");
    const auto text=id(c.image,"Cell","text"),icon=id(c.image,"Cell","icon");
    first.text(text,"Directory");first.text(icon,"folder");second.text(text,"File");
    const auto cells=id(c.image,"Item","cells"),selected=id(c.image,"Item","selected");
    c.a.lists[cells]={&first};c.b.lists[cells]={&second};
    c.a.values[selected].integer=1;c.start();
    auto table=c.runtime->root->children[8]->element.as<oneui::Table>();
    table->setFrame({0,0,360,240});table->setColumns({{L"Name",0}});
    expect(table->richRows().size()==2 && table->richRows()[0][0].leadingIcon.has_value(),
           "Table receives rich cells through structural records");
    expect(table->rowHeight()==42 && table->selectedIndices()==std::vector<int>{0},
           "Table applies authored row sizing and VM selection");
    expect(!table->columnDividersVisible(),"Table applies authored divider presentation");
    table->setColumns({{L"Name",0},{L"Size",100},{L"Modified",160}});
    table->setColumnLayout(std::vector<float>{0,76},40);
    expect(table->columns().size()==2 && table->columns()[1].width==76 &&
           table->selectedIndices()==std::vector<int>{0}, "compact columns preserve row selection");
    table->setColumnKind(2,oneui::TableCellKind::Percent);
    table->setColumns({{L"Name",0},{L"Bytes",104},{L"Time",180}});
    expect(table->columns().size()==2 && table->columns()[1].header==L"Bytes" && table->columns()[1].width==76,
           "data column refresh retains active query widths");
    table->setColumnLayout({});
    expect(table->columns().size()==3 && table->columns()[1].width==104 && table->columns()[2].width==180,
           "removing query restores authored columns and widths");
    table->setSelectedIndices({0,1});
    expect(c.a.sets==0 && c.b.sets==1 && c.b.values[selected].integer,
           "multiple selection writes only changed row leases");
    c.runtime->notify(c.runtime->list(c.runtime->rootScope,c.rows).items[1],selected);
    c.runtime->flush();
    const auto currentKey=table->selectedIndex()==0?"a":"b";
    oneui::KeyEvent enter;enter.key=oneui::Key::Enter;table->onKeyDown(enter);
    expect(c.vm.invokedCommand==c.image.schema.model("Root").commands.at("activate").id &&
           c.vm.invokedArgs.size()==1 && c.vm.invokedArgs[0].text==currentKey,
           "activation sends the stable key of the actual current row");
    first.text(text,"Renamed");
    auto row=c.runtime->list(c.runtime->rootScope,c.rows).items[0];
    auto cell=c.runtime->list(row,cells).items[0];
    c.runtime->notify(cell,text);c.runtime->flush();
    expect(table->richRows()[0][0].text==L"Renamed" && table->selectedIndices().size()==2,
           "cell notification preserves multi selection");
    c.vm.items={&c.b,&c.a};++c.vm.revision;
    c.runtime->notify(c.runtime->rootScope,c.rows);c.runtime->flush();
    expect(table->richRows()[1][0].text==L"Renamed" && table->selectedIndices().size()==2,
           "sorting keeps selection by record identity");
    expect(table->selectedIndex()==0,"sorting retains active row identity separately from selection set");
    oneui::KeyEvent extend;extend.key=oneui::Key::Down;extend.shift=true;
    table->onKeyDown(extend);
    expect(table->selectedIndices()==std::vector<int>({0,1}),"sorting retains the keyboard range anchor");
    const auto prior=table->richRows()[1][0].leadingIcon;
    first.text(icon,"UnknownIconForValidation");c.runtime->notify(cell,icon);
    try {c.runtime->flush();expect(false,"invalid rich icon accepted");}
    catch(const Failure& error){expect(error.result==ONEUI_VIEW_TYPE && table->richRows()[1][0].leadingIcon==prior,
                                      "invalid rich data leaves previous native rows intact");}
    c.runtime->close();const auto calls=c.a.sets+c.b.sets;
    table->setSelectedIndices({});table->onKeyDown(enter);
    expect(c.a.sets+c.b.sets==calls,"retained table cannot mutate released VMs");
    expect(first.retains==first.releases && second.retains==second.releases,"rich cell leases released");
}
