void fontConfirmation() {
    auto styles=std::make_shared<ui::StyleRegistry>();ui::Mount mount({},styles,{});
    auto group=mount.make("Column");group.as<Stack>()->setGap(12);
    styles->inheritTransient(std::make_shared<StyleSheet>());
    check(group.as<Stack>()->gap()==12,"popup palette inheritance preserves authored runtime flow");
    View root;root.setFrame({0,0,800,700});
    auto picker=std::make_shared<FontPicker>();root.add(picker);
    picker->setFrame({600,400,102,32});
    FontPickerConfig config;
    config.items={{"mono",L"Consolas",L"Mono",true},{"sans",L"Arial",L"Sans",true}};
    picker->setConfig(config);
    picker->setConfirmation(true,L"Apply font",L"Cancel");
    picker->setMonospaceFilter({"mono"},true,L"Monospace only");
    std::string selected;picker->setOnSelectionRequested([&](auto key){selected=key;});
    picker->setOpen(true);
    check(picker->filteredCount()==1,"default fixed-pitch catalog uses VM metadata");
    KeyEvent enter;enter.key=Key::Enter;picker->onKeyDown(enter);
    check(selected.empty()&&picker->open(),"candidate previews without committing");
    auto cancel=std::dynamic_pointer_cast<Button>(picker->children()[7]);
    auto click=[&](Widget& w){auto f=w.frame();Point p{f.x+10,f.y+10};root.onMouseDown({p});root.onMouseUp({p});};
    click(*cancel);check(selected.empty()&&!picker->open(),"cancel preserves model selection");
    picker->setOpen(true);
    click(*picker->children()[5]);
    check(picker->filteredCount()==2,"user can include proportional families");
    picker->setQuery(L"Sans");picker->onKeyDown(enter);
    check(selected.empty(),"filtered candidate still requires confirmation");
    click(*picker->children()[6]);
    check(selected=="sans"&&!picker->open(),"apply requests exactly the selected key");
    picker->setOpen(true);
    auto bounds=picker->paintBounds();
    check(bounds.x>=0&&bounds.x+bounds.width<=800&&bounds.y>=0&&bounds.y+bounds.height<=700,
          "compact trigger opens a readable popup above and aligned to its right edge");
    picker->setPopupViewportProvider([]{return Rect{60,120,715,520};});
    check(picker->children()[1]->frame().y>=120,"search stays inside its enclosing scroll viewport");
    bool retried=false;picker->setOnRetryRequested([&]{retried=true;});
    click(*picker->children()[3]);check(retried,"refresh remains available after successful enumeration");
    auto dark=std::make_shared<StyleSheet>();
    dark->setCustomProperty("--surface","#24262d");dark->setCustomProperty("--ink","#ededee");
    picker->setStyleScope(dark);picker->tickAnimations(1e12);
    test_support::RecordingCanvas painted;picker->paint(painted);
    check(!painted.fillRects.empty()&&painted.fillRects.front().color.r==36,
          "font popup paints an opaque dark backing behind search and filter");
    test_support::RecordingCanvas searchPaint;picker->children()[1]->paint(searchPaint);
    check(!searchPaint.fillRects.empty()&&searchPaint.fillRects.front().color.r==36,
          "font search consumes the dark palette without explicit input rules");
    auto colors=pickerColors(*dark);
    check(colors.selectionColor&&colors.selectionColor->r<100&&colors.foreground->r>230,
          "missing selection token keeps dark font candidates readable");
}
