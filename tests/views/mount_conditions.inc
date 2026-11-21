void mountConditions(const char* runtimeFile) {
    const auto path = std::filesystem::path(runtimeFile).parent_path() / "MountConditions.oneview";
    auto bytes = oneview::writeImage(load(path.string().c_str()));
    expect(bytes[10] == oneview::mountConditionMinor, "mount-only conditions use the new IR minor");
    oneview::patch(bytes, 10, 1, 2);
    bool rejected = false;
    try { oneview::readImage(bytes); }
    catch (const oneview::Error&) { rejected = true; }
    expect(rejected, "the loader rejects a mount-only condition on the old minor");

    Case c(path.string().c_str());
    const auto visible = id(c.image, "Root", "visible");
    c.vm.values[visible].integer = 0;
    c.start();
    const auto root = c.runtime->root;
    expect(root->children.size() == 2 && c.vm.reads[c.title] == 0,
           "omitted branches never construct widgets or read their bindings");
    const auto reactive = root->children.front();
    expect(!reactive->element.widget->visible(), "regular v-if retains its hidden widget");
    c.vm.values[visible].integer = 1;
    c.runtime->notify(c.runtime->rootScope, visible);
    c.runtime->flush();
    expect(root->children.size() == 2 && reactive->element.widget->visible(),
           "notifications update regular v-if without reevaluating the mount-only branch");

    c.runtime->reload(c.image);
    expect(c.runtime->root->children.size() == 3, "reload evaluates mount-only conditions afresh");
    const auto button = uniqueChild(c.runtime->root, "Button")->element.as<oneui::Button>();
    expect(button->onKeyDown({oneui::Key::Enter}) && c.vm.invokes == 1,
           "a mounted import retains its event bindings");
    c.vm.values[visible].integer = 0;
    c.runtime->notify(c.runtime->rootScope, visible);
    c.runtime->flush();
    expect(button->visible() && !reactive->element.widget->visible(),
           "a true mount condition stays fixed while a neighboring v-if remains reactive");
}
