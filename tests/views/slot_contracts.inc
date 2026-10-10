// The same IR runtime is used by generated C++ and Rust adapters.
void slotContracts(const char* runtimeFile) {
    auto directory = std::filesystem::path(runtimeFile).parent_path();
    auto single = load((directory / "SlotSingle.oneview").string().c_str());
    auto wrapped = load((directory / "SlotWrapped.oneview").string().c_str());
    Case c((directory / "SlotSingle.oneview").string().c_str());
    c.start();
    auto check = [&](bool direct) {
        auto root = c.runtime->root;
        expect(root->children.size() == 1, "slot yields one native child");
        auto child = root->children[0];
        expect(child->element.component == (direct ? "Text" : "Column"),
               "only explicit single-root slots omit the Column wrapper");
        root->element.widget->setFrame({0, 0, 400, 240});
        oneui::test_support::RecordingCanvas canvas;
        root->element.widget->paint(canvas);
        if (direct) {
            auto frame = child->element.widget->frame();
            expect(frame.width == 400 && frame.height == 240,
                   "single-root supplied/fallback grow fills parent space");
        }
        auto label = direct ? child : child->children[0];
        expect(label->element.as<oneui::Label>()->text() == L"initial", "slot preserves caller binding");
    };
    check(true);
    for (int i = 0; i < 50; ++i) {
        c.runtime->reload(wrapped);
        check(false);
        c.runtime->reload(single);
        check(true);
    }
    c.runtime->reload(load((directory / "SlotFallback.oneview").string().c_str()));
    check(true);
    expect(c.vm.connects == 1 && c.vm.retains == 1, "slot flag reload retains VM connection");
    c.runtime->close();
    expect(c.vm.retains == c.vm.releases, "slot reload releases VM exactly once");
}
