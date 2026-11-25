static void queuedViewFlushContract(const char* path) {
    ApiCase c(path);
    c.start();
    auto label = c.view->value->root->children[0]->element.as<oneui::Label>();
    auto parent = c.view->value->root->element.as<oneui::Stack>();
    auto repeat = c.view->value->root->children[4]->element.as<oneui::Stack>();

    c.vm.text(c.title, "committed promptly");
    expect(oneui_view_notify(c.view, c.title) == 0, "schedule view update");
    expect(oneui_view_flush(c.view) == 0 && label->text() == L"committed promptly", "sync flush");
    const float changedGap = parent->gap() + 19.0f;
    repeat->setGap(changedGap);
    c.window.window->run();
    // A flush visits repeat layouts even when there are no changed bindings.
    expect(repeat->gap() == changedGap, "consumed queue entry left the layout alone");

    c.vm.text(c.title, "another notification");
    expect(oneui_view_notify(c.view, c.title) == 0, "schedule follow-up update");
    c.window.window->run();
    expect(label->text() == L"another notification" && repeat->gap() == parent->gap(),
           "a later notification still flushes the view");

    c.vm.text(c.title, "sync first");
    expect(oneui_view_notify(c.view, c.title) == 0 && oneui_view_flush(c.view) == 0,
           "first update committed ahead of queued message");
    bool latestAtNextCallback = false;
    expect(c.window.window->post([&] { latestAtNextCallback = label->text() == L"latest value"; }),
           "queue application callback");
    c.vm.text(c.title, "latest value");
    expect(oneui_view_notify(c.view, c.title) == 0, "notify while prior message is queued");
    c.window.window->run();
    expect(latestAtNextCallback, "the latest notification was committed");

    c.vm.text(c.title, "\xc0\x80");
    expect(oneui_view_notify(c.view, c.title) == 0, "schedule value that will fail validation");
    expect(oneui_view_flush(c.view) == ONEUI_VIEW_TYPE, "sync flush fails for invalid UTF-8");
    c.vm.text(c.title, "recovered by queued flush");
    c.window.window->run();
    expect(label->text() == L"recovered by queued flush", "queued entry retries failed work");

    expect(oneui_view_notify(c.view, c.title) == 0, "schedule before hot reload");
    expect(oneui_view_reload(c.view, c.bytes.data(), c.bytes.size()) == 0, "reload with queued work");
    parent = c.view->value->root->element.as<oneui::Stack>();
    repeat = c.view->value->root->children[4]->element.as<oneui::Stack>();
    repeat->setGap(parent->gap() + 19.0f);
    c.window.window->run();
    expect(repeat->gap() == parent->gap(), "queued work may sync the replacement tree");
}
