// Exercise the serialized IR path as well as both property replay orders.
void tabsSizing(const char* runtimeFile) {
    using oneui::TabsSizingMode;
    const auto path=std::filesystem::path(runtimeFile).parent_path()/"TabsSizing.oneview";
    for(bool reverse:{false,true}) {
        int released=0;
        std::weak_ptr<oneui::ui::KeyedTabs> adapter;
        std::shared_ptr<oneui::Tabs> survivingWidget;
        {
            oneui::ui::Mount factory({},std::make_shared<oneui::ui::StyleRegistry>(),
                                     std::make_shared<oneui::MemoryClipboard>());
            auto element=factory.make("Tabs");
            adapter=element.tabs;survivingWidget=element.as<oneui::Tabs>();
            auto lease=std::shared_ptr<int>(new int(0),[&](int* value){++released;delete value;});
            element.tabs->selected=[lease](const std::wstring&){expect(bool(lease),"callback owns its lease");};
            lease.reset();
            oneui::State<std::wstring> presentation{L"segmented"}, sizing{L"compact"};
            if(reverse)factory.bind(element,"sizing",sizing);
            factory.bind(element,"presentation",presentation);
            if(!reverse)factory.bind(element,"sizing",sizing);
            auto native=element.as<oneui::Tabs>();
            expect(native->sizingMode()==TabsSizingMode::Compact,"copied C++ bindings share explicit sizing");
            presentation.set(L"document");factory.flush();
            presentation.set(L"segmented");factory.flush();
            expect(native->sizingMode()==TabsSizingMode::Compact,"C++ presentation binding cannot erase sizing");
            sizing.set(L"equal");factory.flush();
            presentation.set(L"document");factory.flush();
            expect(native->sizingMode()==TabsSizingMode::Equal,"C++ sizing update survives presentation update");
        }
        expect(adapter.expired() && released==1,"copied bindings destroy the Tabs adapter and callback lease once");
        survivingWidget->setItems({L"A",L"B"});
        survivingWidget->onKeyDown({oneui::Key::Right});
        expect(released==1,"surviving native Tabs has no retired adapter callback");
        {
            oneui::ui::Mount factory({},std::make_shared<oneui::ui::StyleRegistry>(),
                                     std::make_shared<oneui::MemoryClipboard>());
            auto native=std::make_shared<oneui::Tabs>();
            oneui::ui::Element manual(oneui::ui::Node(native),"Tabs");
            manual.tabs=std::make_shared<oneui::ui::KeyedTabs>(native);
            factory.set(manual,"presentation",L"document");
            expect(native->sizingMode()==TabsSizingMode::Compact,"manual Element keeps legacy document default");
            factory.set(manual,"presentation",L"segmented");
            expect(native->sizingMode()==TabsSizingMode::Equal,"manual Element keeps legacy segmented default");
            try {factory.set(manual,"sizing",L"compact");expect(false,"manual Element sizing accepted without state");}
            catch(const std::invalid_argument&) {}
            expect(native->sizingMode()==TabsSizingMode::Equal,"unsupported manual sizing leaves native mode unchanged");
        }
        auto image=load(path.string().c_str());
        if(reverse) {
            std::reverse(image.records[oneview::PROP].begin(),image.records[oneview::PROP].end());
            std::reverse(image.records[oneview::BIND].begin(),image.records[oneview::BIND].end());
        }
        Vm vm(image,"Root");
        const auto presentation=id(image,"Root","presentation"), sizing=id(image,"Root","sizing");
        vm.text(presentation,"segmented");vm.text(sizing,"compact");
        auto runtime=std::make_shared<Runtime>(image);
        runtime->initialize(&vm.table,&vm);
        auto tabs=[&](size_t index){return runtime->root->children[index]->element.as<oneui::Tabs>();};
        expect(tabs(0)->sizingMode()==TabsSizingMode::Compact,"omitted presentation retains compact default");
        expect(tabs(1)->sizingMode()==TabsSizingMode::Equal,"segmented retains equal default without sizing");
        for(size_t i:{2u,3u,6u,7u}) {
            expect(tabs(i)->sizingMode()==TabsSizingMode::Compact,"explicit compact wins in either property order");
            tabs(i)->setItems({L"A",L"Long compact selector label"});
            tabs(i)->setFrame({0,0,600,42});
            oneui::test_support::RecordingCanvas canvas;
            tabs(i)->paint(canvas);
            for(int item=0;item<2;++item)
                expect(tabs(i)->itemFrame(item).width>=50 && tabs(i)->itemFrame(item).width<=96,
                       "segmented compact preserves authored 50..96 item geometry");
        }
        for(size_t i:{4u,5u})
            expect(tabs(i)->sizingMode()==TabsSizingMode::Equal,"document explicit equal wins in either order");
        auto update=[&](const char* key,const char* value) {
            auto property=id(image,"Root",key);vm.text(property,value);
            runtime->notify(runtime->rootScope,property);runtime->flush();
        };
        auto checkBound=[&](TabsSizingMode mode) {
            expect(tabs(6)->sizingMode()==mode && tabs(7)->sizingMode()==mode,
                   "binding updates keep explicit sizing independent of presentation");
        };
        update("presentation","document");checkBound(TabsSizingMode::Compact);
        expect(tabs(8)->sizingMode()==TabsSizingMode::Compact,"bound document restores implicit compact");
        update("sizing","equal");checkBound(TabsSizingMode::Equal);
        tabs(6)->setFrame({0,0,600,42});tabs(7)->setFrame({0,0,600,42});
        expect(tabs(6)->itemFrame(0).width==300 && tabs(7)->itemFrame(1).width==300,
               "equal sizing divides the actual 600px strip independently of compact width limits");
        update("presentation","segmented");checkBound(TabsSizingMode::Equal);
        expect(tabs(8)->sizingMode()==TabsSizingMode::Equal,"bound segmented restores implicit equal");
        update("sizing","compact");checkBound(TabsSizingMode::Compact);
        vm.text(presentation,"document");vm.text(sizing,"equal");
        runtime->notify(runtime->rootScope,reverse?sizing:presentation);
        runtime->notify(runtime->rootScope,reverse?presentation:sizing);runtime->flush();
        checkBound(TabsSizingMode::Equal);
        vm.text(presentation,"segmented");vm.text(sizing,"compact");
        runtime->notify(runtime->rootScope,0);runtime->flush();
        auto retained=tabs(6);
        auto removed=image;
        auto& bindings=removed.records[oneview::BIND];
        bindings.erase(std::remove_if(bindings.begin(),bindings.end(),[](const auto& record){
            return record[1]==23;
        }),bindings.end());
        auto& properties=removed.records[oneview::PROP];
        properties.erase(std::remove_if(properties.begin(),properties.end(),[](const auto& record){
            return record[1]==23;
        }),properties.end());
        runtime->reload(removed);
        expect(tabs(6)==retained,"sizing removal retains native Tabs identity");
        checkBound(TabsSizingMode::Equal);
        expect(tabs(2)->sizingMode()==TabsSizingMode::Equal && tabs(4)->sizingMode()==TabsSizingMode::Compact,
               "removing static sizing restores presentation default");
        runtime->reload(image);checkBound(TabsSizingMode::Compact);
        try {update("sizing","unknown");expect(false,"invalid bound Tabs sizing accepted");}
        catch(const Failure& error) {expect(error.result==ONEUI_VIEW_TYPE,"invalid sizing type diagnostic");}
        checkBound(TabsSizingMode::Compact);
        runtime->close();
        expect(vm.retains==vm.releases,"Tabs sizing bindings release their VM lease");
    }
}
