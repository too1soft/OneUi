// Checkbox events cross the same guarded VM protocol as other inputs.
void checkboxContract(const char* runtimeFile) {
    oneui::StyleSheet styles;
    std::string error;
    expect(styles.addRulesFromCss(oneui::ui::syntax::css(
        "Checkbox:checked { background-color:#112233; content-background-color:#ffffff; }"
        "Checkbox:focus { outline-color:#224466; outline-width:2px; outline-offset:3px; }"), &error),
        "Checkbox state CSS parses");
    const auto style = oneui::checkboxStyleOverrideFromStyleSheet(
        styles, {"checkbox", {"one-Checkbox"}, oneui::StyleStateNone});
    expect(style.selected->boxBackground->r == 0x11 && style.selected->checkColor->r == 255,
           "checked colors reach native Checkbox style");
    expect(style.focusVisible->focusRing->color->b == 0x66 &&
           style.focusVisible->focusRing->width == 2 && style.focusVisible->focusRing->offset == 3,
           "keyboard focus CSS reaches native Checkbox ring");
    Case c(runtimeFile);
    const auto checked = id(c.image, "Root", "checked");
    c.vm.values[checked].integer = 1;
    c.start();
    auto checkbox = c.runtime->root->children[6]->element.as<oneui::Checkbox>();
    expect(checkbox && checkbox->checked(), "Checkbox reads its boolean VM");
    expect(checkbox->tooltip() == L"Permission toggle", "Checkbox keeps its authored tooltip");
    const auto sets = c.vm.sets;
    c.vm.values[checked].integer = 0;
    c.runtime->notify(c.runtime->rootScope, checked);
    c.runtime->flush();
    expect(!checkbox->checked() && c.vm.sets == sets,
           "programmatic checkbox updates do not echo into VM");
    oneui::KeyEvent space;
    space.key = oneui::Key::Space;
    checkbox->onKeyDown(space);
    expect(checkbox->checked() && c.vm.values[checked].integer == 1 && c.vm.sets == sets + 1,
           "native keyboard toggles the boolean VM exactly once");
    checkbox->setDisabled(true);
    checkbox->onKeyDown(space);
    expect(c.vm.sets == sets + 1, "disabled checkbox rejects input");
    c.runtime->close();
    checkbox->setDisabled(false);
    checkbox->onKeyDown(space);
    expect(c.vm.sets == sets + 1, "retained control cannot mutate a disconnected VM");

    oneui::ui::Mount mount({}, std::make_shared<oneui::ui::StyleRegistry>(), {});
    auto element = mount.make("Checkbox");
    oneui::State<bool> state(false);
    mount.model(element, state);
    element.as<oneui::Checkbox>()->onKeyDown(space);
    expect(state.get(), "legacy typed composition shares Checkbox binding behavior");
}
