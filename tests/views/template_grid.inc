// The same native grid backs C++ authoring and language-neutral keyed repeats.
void templateGrids(const char* runtimeFile) {
    const auto path=std::filesystem::path(runtimeFile).parent_path()/"Lists.oneview";
    Case c(path.string().c_str());
    c.vm.lists[id(c.image,"Root","spans")]={};
    std::vector<oneui::views::Value> delivered;
    uint32_t command=0;
    c.vm.table.invoke=[](void* p,uint32_t id,const OneUiValue* args,size_t count) {
        auto& vm=*static_cast<Vm*>(p);
        vm.values[0].integer=id;
        for(size_t i=0;i<count;++i)vm.values[uint32_t(0x80000000u+i)]=oneui::views::Value::copy(args[i]);
        if(vm.onInvoke)vm.onInvoke();
        return 0;
    };
    c.vm.onInvoke=[&] {
        command=uint32_t(c.vm.values[0].integer);
        delivered.clear();
        const size_t count=command==c.image.schema.model("Root").commands.at("reorder").id?2:4;
        for(size_t i=0;i<count;++i)delivered.push_back(c.vm.values[uint32_t(0x80000000u+i)]);
    };
    c.start();
    const auto instance=c.runtime->root->children[17];
    auto grid=instance->element.as<oneui::ReorderableGrid>();
    expect(grid && grid->itemCount()==2 && grid->children().size()==2,"grid repeat inserts direct native items");
    auto first=grid->children()[0];
    auto second=grid->children()[1];
    expect(std::dynamic_pointer_cast<oneui::TemplateGridItem>(first)->key()==L"a","grid preserves model keys");
    oneui::test_support::RecordingCanvas canvas;
    grid->setFrame({10,20,440,180});grid->paint(canvas);
    expect(first->frame().width==216 && first->frame().height==70 && second->frame().x==234,"native grid owns responsive geometry");

    oneui::ui::Mount cpp({},std::make_shared<oneui::ui::StyleRegistry>(),std::make_shared<oneui::MemoryClipboard>());
    auto a=cpp.make("GridItem",{cpp.make("Text")}), b=cpp.make("GridItem",{cpp.make("Text")});
    cpp.set(a,"grid-key",L"a");cpp.set(b,"grid-key",L"b");
    auto old=cpp.make("ReorderableGrid",{a,b});
    cpp.set(old,"column-count",2);cpp.set(old,"column-gap",8.0f);cpp.set(old,"row-gap",10.0f);cpp.set(old,"item-height",70.0f);
    old.widget->setFrame(grid->frame());old.widget->paint(canvas);
    const auto sameFrame=[](oneui::Rect a,oneui::Rect b) {
        return a.x==b.x && a.y==b.y && a.width==b.width && a.height==b.height;
    };
    expect(sameFrame(a.widget->frame(),first->frame()) && sameFrame(b.widget->frame(),second->frame()),"C++ and IR grid geometry agree");
    const std::string gridCss="reorderable-grid {grid-min-column-width:280px;gap:8px;height:70px; padding:0px;outline-width:2px;outline-color:#123456;}";
    c.runtime->styles->replace(gridCss);cpp.styles()->replace(gridCss);
    grid->paint(canvas);old.widget->paint(canvas);
    expect(first->frame().width==440 && second->frame().y==98 && sameFrame(a.widget->frame(),first->frame()),
           "typed grid CSS drives the same responsive column limit in C++ and IR");
    c.runtime->styles->replace("");cpp.styles()->replace("");grid->paint(canvas);old.widget->paint(canvas);
    cpp.set(b,"grid-key",L"a");
    try { cpp.make("ReorderableGrid",{a,b}); expect(false,"duplicate C++ grid keys accepted"); }
    catch(const std::invalid_argument&) {}

    grid->onMouseDown({{20,30},oneui::MouseButton::Left});
    grid->onMouseMove({{400,30},oneui::MouseButton::Left});
    // A label update during a drag must retain the native gesture and widgets.
    c.a.text(id(c.image,"Item","text"),"Renamed");
    auto scope=instance->children[0]->rows.at("a")->scopes.front();
    c.runtime->notify(scope,id(c.image,"Item","text"));c.runtime->flush();
    grid->onMouseUp({{400,30},oneui::MouseButton::Left});
    expect(delivered.size()==2 && delivered[0].text=="a" && delivered[1].integer==1,"typed reorder survives child update");
    grid->onMouseDown({{20,30},oneui::MouseButton::Left});
    grid->onMouseMove({{0,30},oneui::MouseButton::Left});
    grid->onMouseUp({{0,30},oneui::MouseButton::Left});
    expect(delivered.size()==4 && delivered[0].text=="a" && delivered[1].integer==2 && delivered[2].number==0 && delivered[3].number==30,"external drag retains source, phase and coordinates");
    c.vm.items={&c.b,&c.a};++c.vm.revision;
    c.runtime->notify(c.runtime->rootScope,c.rows);c.runtime->flush();
    expect(grid->children()[0]==second && grid->children()[1]==first,"keyed reorder retains native item identities");
    grid->setFrame({10,20,250,180});grid->paint(canvas);
    expect(grid->contentHeight()>0,"resized grid publishes scroll content height");
    c.runtime->reload(c.image);
    auto reloaded=c.runtime->root->children[17]->element.as<oneui::ReorderableGrid>();
    expect(reloaded && reloaded->itemCount()==2,"grid reload restores committed keyed projection");
    c.vm.items={&c.a};++c.vm.revision;
    c.runtime->notify(c.runtime->rootScope,c.rows);c.runtime->flush();
    expect(reloaded->itemCount()==1,"grid list removal retires the native item");
    c.runtime->close();
    expect(c.a.retains==c.a.releases && c.b.retains==c.b.releases,"grid render-scope leases balance");
}
