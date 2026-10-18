void dataMenus() {
    using oneui::views::DataMenu;
    using oneui::views::DataMenuEntry;
    auto menu = std::make_shared<DataMenu>();
    View root;
    root.setFrame({0,0,240,400});
    root.add(menu);
    menu->setFrame({0,0,224,344});
    std::vector<std::string> activated;
    menu->setOnActivated([&](std::string id) { activated.push_back(std::move(id)); });
    DataMenuEntry disabled; disabled.id="disabled"; disabled.text=L"Disabled"; disabled.disabled=true;
    DataMenuEntry separator; separator.id="separator"; separator.separator=true;
    DataMenuEntry checked; checked.id="checked"; checked.text=L"Checked"; checked.checked=true; checked.shortcut=L"Ctrl+C";
    DataMenuEntry parent; parent.id="parent"; parent.text=L"More";
    DataMenuEntry child; child.id="child"; child.parent="parent"; child.text=L"Child";
    const std::vector<DataMenuEntry> entries{disabled,separator,checked,parent,child};
    menu->setTitle(L"Host"); menu->setSubtitle(L"Endpoint"); menu->setItems(entries);
    auto key = [&](Key k) { KeyEvent e; e.key=k; return menu->onKeyDown(e); };
    check(root.requestFocus(menu.get(),true),"data menu focus");
    key(Key::Down); key(Key::Right);
    check(activated.empty(),"right on a leaf does not activate it");
    key(Key::Enter);
    check(activated==std::vector<std::string>{"checked"},"keyboard skips disabled items and separators");
    key(Key::Down); key(Key::Right);
    check(activated.size()==1,"submenu is navigation, not activation");
    key(Key::Down); key(Key::Down); key(Key::Enter);
    check(activated.back()=="child","submenu emits stable leaf ID");
    check(key(Key::Left),"left returns from submenu");
    check(!key(Key::Escape),"escape bubbles to popup");
    auto bad=entries; bad.back().parent="absent";
    bool rejected=false;
    try { menu->setItems(bad); } catch(const std::invalid_argument&) { rejected=true; }
    check(rejected,"missing parent rejected transactionally");
    key(Key::Home); key(Key::Enter);
    check(activated.back()=="checked","invalid update preserves old menu");
    bad=entries; bad.back().parent="child";
    rejected=false;
    try { menu->setItems(bad); } catch(const std::invalid_argument&) { rejected=true; }
    check(rejected,"cyclic submenu rejected");
    test_support::RecordingCanvas canvas;
    menu->paint(canvas);
    check(std::any_of(canvas.texts.begin(),canvas.texts.end(),[](const auto& text) {
        return text.text.find(L"Ctrl+C")!=std::wstring::npos && text.text.find(L"\u2713")!=std::wstring::npos;
    }),"check mark and shortcut hint rendered");

    DataMenuEntry header; header.id="group"; header.text=L"Group"; header.header=true;
    auto grouped=entries; grouped.insert(grouped.begin(),header);
    menu->setItems(grouped);
    test_support::RecordingCanvas groupedCanvas;
    menu->paint(groupedCanvas);
    check(std::any_of(groupedCanvas.texts.begin(),groupedCanvas.texts.end(),[](const auto& text) {
        return text.text==L"Group";
    }),"group header text rendered");
    key(Key::Home); key(Key::Enter);
    check(activated.back()=="checked","keyboard skips group headers");
    bad=grouped; bad.back().parent="group";
    rejected=false;
    try { menu->setItems(bad); } catch(const std::invalid_argument&) { rejected=true; }
    check(rejected,"header cannot parent submenu items");
    DataMenuEntry nestedHeader; nestedHeader.id="nested"; nestedHeader.parent="parent"; nestedHeader.text=L"Nested"; nestedHeader.header=true;
    bad=grouped; bad.push_back(nestedHeader);
    rejected=false;
    try { menu->setItems(bad); } catch(const std::invalid_argument&) { rejected=true; }
    check(rejected,"nested group header rejected");

    auto styles=std::make_shared<ui::StyleRegistry>();
    ui::Mount mount({},styles,{});
    auto themed=mount.make("DataMenu",{},"example-menu");
    auto native=themed.as<DataMenu>(); native->setItems({checked});
    native->setFrame({0,0,224,100});
    for(const bool dark:{false,true}) {
        styles->replace(ui::syntax::css(dark
            ? "menu.example-menu {background:#102030;color:#eeeeee;background-opacity:0.5;} .menu-item {color:#eeeeee;}"
            : "menu.example-menu {background:#ffffff;color:#202020;background-opacity:1;} .menu-item {color:#202020;}"));
        test_support::RecordingCanvas painted; native->paint(painted);
        check(!painted.texts.empty() && painted.texts.back().color.r==(dark?238:32),
              "mounted menu follows stylesheet replacement");
        check(!painted.fillRects.empty() && painted.fillRects.front().color.a==(dark?127:255),
              "menu material alpha follows background changes");
    }
}
