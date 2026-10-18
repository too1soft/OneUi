void dataMenuReplay() {
    using oneui::views::DataMenu;
    using oneui::views::DataMenuEntry;
    auto menu = std::make_shared<DataMenu>();
    View root;
    root.setFrame({0,0,240,180});
    root.add(menu);
    menu->setFrame({0,0,224,180});
    std::vector<DataMenuEntry> rows;
    for (int i=0; i<20; ++i) {
        DataMenuEntry row;
        row.id=std::to_string(i); row.text=L"Row "+std::to_wstring(i);
        rows.push_back(std::move(row));
    }
    std::string selected;
    menu->setOnActivated([&](std::string id){selected="old:"+id;});
    menu->setTitle(L"Sessions"); menu->setSubtitle(L"Endpoint"); menu->setItems(rows);
    auto scroll = [&] {
        test_support::RecordingCanvas canvas;
        root.paint(canvas);
        MouseWheelEvent event; event.position={20,100}; event.deltaY=-120;
        check(root.onMouseWheel(event),"data menu root wheel handled");
    };
    auto bottom = [&] {
        test_support::RecordingCanvas canvas;
        root.paint(canvas);
        selected.clear();
        root.onMouseDown({{50,168},MouseButton::Left});
        root.onMouseUp({{50,168},MouseButton::Left});
    };
    scroll();
    menu->setItems(rows); menu->setTitle(L"Sessions"); menu->setSubtitle(L"Endpoint");
    menu->setOnActivated([&](std::string id){selected="new:"+id;});
    bottom();
    check(selected=="new:19","identical replay preserves last-row reachability and new callback");

    // Each record field participates: a real change must rebuild the rendered
    // content and reset the old scroll position, unlike an identical replay.
    const std::vector<std::function<void(std::vector<DataMenuEntry>&)>> changes{
        [](auto& r){r[0].id="renamed";},
        [](auto& r){r[0].parent=r[1].id;},
        [](auto& r){r[0].text=L"Changed";},
        [](auto& r){r[0].shortcut=L"Ctrl+X";},
        [](auto& r){r[0].icon=IconSymbol::CheckCircle;},
        [](auto& r){r[0].disabled=true;},
        [](auto& r){r[0].checked=true;},
        [](auto& r){r[0].danger=true;},
        [](auto& r){r[0].separator=true;},
        [](auto& r){r[0].header=true;},
        [](auto& r){std::swap(r[0],r[1]);},
        [](auto& r){r.pop_back();},
        [](auto& r){auto extra=r.back();extra.id="extra";r.push_back(extra);}
    };
    for (const auto& change:changes) {
        menu->setItems(rows); scroll();
        auto changed=rows; change(changed); menu->setItems(changed);
        bottom();
        check(!selected.empty() && selected!="new:19",
              "changed field/order/membership applies and resets old viewport");
    }
    menu->setItems(rows); scroll(); menu->setTitle(L"Changed title"); bottom();
    check(!selected.empty() && selected!="new:19","changed title rebuilds");
    scroll(); menu->setSubtitle(L"Changed subtitle"); bottom();
    check(!selected.empty() && selected!="new:19","changed subtitle rebuilds");

    DataMenuEntry parent; parent.id="parent"; parent.text=L"Parent";
    DataMenuEntry leaf; leaf.id="root-leaf"; leaf.text=L"Root leaf";
    DataMenuEntry child; child.id="child"; child.parent="parent"; child.text=L"Child";
    std::vector<DataMenuEntry> nested{parent,leaf,child};
    menu->setTitle(L""); menu->setSubtitle(L""); menu->setItems(nested);
    auto key=[&](Key value){KeyEvent event;event.key=value;return menu->onKeyDown(event);};
    check(root.requestFocus(menu.get(),true),"replay menu focus");
    key(Key::Home); key(Key::Right);
    menu->setItems(nested); menu->setTitle(L""); menu->setSubtitle(L"");
    key(Key::Home); key(Key::Down); key(Key::Enter);
    check(selected=="new:child","identical replay preserves submenu path");
    auto invalid=nested; invalid.back().parent="missing";
    bool rejected=false;
    try {menu->setItems(invalid);} catch(const std::invalid_argument&){rejected=true;}
    check(rejected,"invalid replay still validated");
    selected.clear();
    key(Key::Home); key(Key::Down); key(Key::Enter);
    check(selected=="new:child","invalid replay preserves submenu path");
    nested.back().text=L"New child"; menu->setItems(nested);
    key(Key::Home); key(Key::Down); key(Key::Enter);
    check(selected=="new:root-leaf","real update resets submenu to root");
    menu->setItems({}); selected.clear(); key(Key::Home); key(Key::Enter);
    check(selected.empty(),"empty update removes old actions");
}
