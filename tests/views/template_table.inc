// Pinned table geometry, independent axes and modifier-preserving row intents.
void templateTables(const char* runtimeFile) {
    const auto path = std::filesystem::path(runtimeFile).parent_path() / "Lists.oneview";
    Case c(path.string().c_str());
    c.vm.lists[id(c.image,"Root","spans")] = {};
    static std::vector<OneUiValue> args;
    args.clear();
    c.vm.table.invoke=[](void*,uint32_t,const OneUiValue* values,size_t count) {
        args.assign(values,values+count);return ONEUI_VIEW_OK;
    };
    c.start();
    auto table = c.runtime->root->children[15]->element.as<oneui::TemplateTable>();
    auto header = c.runtime->root->children[15]->children[0]->element.widget;
    auto body = c.runtime->root->children[15]->children[1]->element.widget;
    oneui::test_support::RecordingCanvas canvas;
    table->setFrame({10,20,400,300});
    table->paint(canvas);
    expect(header->frame().height == 34 && header->frame().width >= 600,
           "TemplateTable reserves authored header height and common minimum width");
    const auto headerY = header->frame().y, bodyY = body->frame().y;
    table->setScrollOffset(160);
    table->paint(canvas);
    expect(table->scrollOffset()==160 && header->frame().y==headerY && body->frame().y<bodyY,
           "vertical scrolling moves body while header stays pinned");
    table->setHorizontalScrollOffset(100);
    table->paint(canvas);
    expect(table->horizontalScrollOffset()==100 && header->frame().x==body->frame().x,
           "horizontal scrolling keeps header and body columns aligned");
    table->setFrame({10,20,800,400});
    table->paint(canvas);
    expect(table->horizontalScrollOffset()==0 && header->frame().width>=800,
           "resize clamps horizontal scroll and retains the table width");
    expect(table->maxScrollOffset()==2000-(400-34),
           "table body stretches to all remaining height, not ScrollView's 160px default");
    table->setContentWidth(800);
    table->paint(canvas);
    expect(body->frame().width==800, "explicit row width retains header alignment across scrollbar gutter");
    table->setScrollOffset(100000);
    table->paint(canvas);
    expect(table->scrollOffset()==table->maxScrollOffset(), "last table row is reachable");
    expect(table->accessibleRole()==oneui::AccessibilityRole::Table, "table semantics are native");

    auto row=c.runtime->root->children[16]->element.as<oneui::InteractiveSurface>();
    row->setFrame({20,30,200,40});
    oneui::MouseEvent pointer{{25,35},oneui::MouseButton::Left};
    pointer.control=true;pointer.shift=true;pointer.clickCount=2;
    row->onMouseDown(pointer);row->onMouseUp(pointer);
    expect(args.size()==5 && args[0].tag==ONEUI_VALUE_F64 && args[1].tag==ONEUI_VALUE_F64,
           "pointer event preserves typed coordinates");
    expect(args[0].as.number==25 && args[1].as.number==35 && args[2].as.boolean && args[3].as.boolean && args[4].as.integer==2,
           "pointer event retains modifiers and double-click count");
    pointer.button=oneui::MouseButton::Right;
    row->onMouseDown(pointer);row->onMouseUp(pointer);
    expect(args.size()==2 && args[0].as.number==25 && args[1].as.number==35,
           "context menu command retains native pointer coordinates");
    auto caption=c.runtime->root->children[13]->element.as<oneui::Label>();
    c.runtime->styles->replace("label {font-family:Cascadia Mono;font-size:13px;}");
    expect(caption->textFontFamily()==L"Cascadia Mono", "IR Text consumes font-family");
    oneui::ui::Mount legacy({},std::make_shared<oneui::ui::StyleRegistry>(),std::make_shared<oneui::MemoryClipboard>());
    auto oldText=legacy.make("Text").as<oneui::Label>();
    legacy.styles()->replace("button {opacity:0.58;transition-duration:0ms;}");
    auto faded=legacy.make("Button");faded.widget->setFrame({0,0,100,32});
    oneui::test_support::RecordingCanvas fadedCanvas;faded.widget->paint(fadedCanvas);
    expect(!fadedCanvas.opacityLayers.empty() && std::abs(fadedCanvas.opacityLayers.front().opacity-0.58f)<0.001f,
           "declarative Button opacity reaches the native paint layer");
    auto flexChild=legacy.make("Column",{},"flex-regression");
    legacy.set(flexChild,"basis",100.f);
    legacy.set(flexChild,"shrink",0.f);
    auto flexParent=legacy.make("Row",{flexChild});
    legacy.styles()->replace("stack.flex-regression {height:33px;}");
    legacy.styles()->applyBaseLayouts();
    legacy.set(flexChild,"basis",200.f);
    flexParent.as<oneui::Stack>()->setFlex(flexChild.widget,flexChild.flex);
    legacy.styles()->applyBaseLayouts();
    flexParent.widget->setFrame({0,0,400,40});flexParent.widget->paint(canvas);
    expect(flexChild.widget->frame().width==200,
           "C++ authored flex update survives CSS dimensions as in the IR binding path");
    legacy.styles()->replace("label {font-family:Cascadia Mono;font-size:13px;}");
    expect(oldText->textFontFamily()==caption->textFontFamily(), "C++/IR font family semantics match");
    c.runtime->styles->replace("label {font-family:inherit;}");
    legacy.styles()->replace("label {font-family:inherit;}");
    expect(caption->textFontFamily().empty() && oldText->textFontFamily().empty(),
           "restyling removes a previous font override");
    auto scroll = c.runtime->root->children[17]->element.as<oneui::ScrollView>();
    auto oldScrollElement = legacy.make("Scroll", {legacy.make("Text")});
    legacy.set(oldScrollElement, "content-width", 640.f);
    legacy.set(oldScrollElement, "content-height", 800.f);
    auto oldScroll = oldScrollElement.as<oneui::ScrollView>();
    for (auto value : {scroll, oldScroll}) {
        value->setFrame({0,0,300,200}); value->paint(canvas);
        expect(value->children().front()->frame().width == 640,
               "C++ and IR Scroll retain explicit content width across the scrollbar gutter");
        value->setHorizontalScrollOffset(100); value->paint(canvas);
        expect(value->horizontalScrollOffset() == 100, "wide Scroll content is reachable");
    }
    expect(oneui::ui::namedIcon("left-outline") == oneui::IconSymbol::OutlineLeft &&
           oneui::ui::namedIcon("right-outline") == oneui::IconSymbol::OutlineChevron &&
           oneui::ui::namedIcon("key-outline") == oneui::IconSymbol::OutlineKey &&
           oneui::ui::namedIcon("tag-outline") == oneui::IconSymbol::OutlineTag &&
           oneui::ui::namedIcon("pin-outline") == oneui::IconSymbol::OutlinePin &&
           oneui::ui::namedIcon("sliders") == oneui::IconSymbol::Sliders &&
           oneui::ui::namedIcon("copy-native") == oneui::IconSymbol::Copy,
           "outline aliases retain the original native glyphs");
    auto layer = c.runtime->root->children[18]->element.as<oneui::TemplateOverlay>();
    auto field = c.runtime->root->children[18]->children[0]->element.as<oneui::TextField>();
    auto foreground = c.runtime->root->children[18]->children[1]->element.as<oneui::Button>();
    layer->setOverlayVisible(true);
    layer->setFrame({10,20,400,50}); layer->paint(canvas);
    expect(field->frame().x==10 && field->frame().width==400 && foreground->frame().x==366 &&
           foreground->frame().y==36 && foreground->frame().width==34 && foreground->frame().height==18,
           "Overlay fills its base and anchors foreground with explicit size and inset");
    auto legacyOverlay=legacy.make("Overlay",{legacy.make("Input"),legacy.make("Button")});
    for (auto entry : {std::pair{"overlay-width",34.f}, {"overlay-height",18.f}, {"inset-right",10.f}})
        legacy.set(legacyOverlay,entry.first,entry.second);
    legacy.set(legacyOverlay,"horizontal-align",L"end");
    legacy.set(legacyOverlay,"vertical-align",L"center");
    legacyOverlay.widget->setFrame({10,20,400,50});legacyOverlay.widget->paint(canvas);
    const auto oldForeground=legacyOverlay.as<oneui::TemplateOverlay>()->overlays().front().child;
    expect(oldForeground->frame().x==foreground->frame().x && oldForeground->frame().y==foreground->frame().y,
           "C++ and IR Overlay geometry match");
    layer->setFrame({10,20,200,30});layer->paint(canvas);
    expect(foreground->frame().x==166 && foreground->frame().y==26, "Overlay reanchors on resize");
    layer->setOverlayVisible(false);
    expect(!foreground->visible() && field->visible() && layer->requestFocus(field.get()),
           "hidden overlay leaves base input focusable");
    const auto withTrailing=foreground->naturalContentSize().width;
    foreground->clearTrailingIcon();
    expect(foreground->naturalContentSize().width<withTrailing,"trailing icon reserves native content space");
    oneui::test_support::RecordingCanvas withPrefix, withoutPrefix;
    field->setText(L"prefix"); field->paint(withPrefix);
    field->setPrefixIcon({});field->paint(withoutPrefix);
    expect(!withPrefix.texts.empty() && !withoutPrefix.texts.empty() &&
           withPrefix.texts.back().rect.x>withoutPrefix.texts.back().rect.x,
           "prefix icon reserves space in the native text editor");
    field->setText(L"");field->onFocusChanged(true);
    oneui::test_support::RecordingCanvas focused;
    field->paint(focused);
    expect(std::any_of(focused.texts.begin(),focused.texts.end(),[](const auto& t){return t.text==L"Search";}),
           "explicit placeholder remains visible while focused");
    c.runtime->close();
}
