// A modeless overlay is one part of its parent's focus order, not a Tab trap.
void testNestedModelessTabReturnsThroughContentAndSiblingControls() {
    using namespace oneui;
    auto before = std::make_shared<Button>(L"Before");
    auto first = std::make_shared<Button>(L"Content first");
    auto second = std::make_shared<Button>(L"Content second");
    auto low = std::make_shared<Button>(L"Overlay low");
    auto high = std::make_shared<Button>(L"Overlay high");
    auto after = std::make_shared<Button>(L"After");
    auto content = std::make_shared<Stack>();
    content->add(first);
    content->add(second);
    auto nested = std::make_shared<OverlayHost>();
    nested->setContent(content);
    nested->addOverlay(low, 1);
    nested->addOverlay(high, 2);
    auto column = std::make_shared<Stack>();
    column->add(before);
    column->add(nested);
    column->add(after);
    OverlayHost root;
    root.setContent(column);
    root.setFrame({0, 0, 480, 320});
    const std::vector<Widget*> controls{
        before.get(), first.get(), second.get(), low.get(), high.get(), after.get()};
    int step = 0;
    const auto expectFocus = [&](Widget* expected) {
        bool mismatch = false;
        for (const auto control : controls) mismatch |= control->focused() != (control == expected);
        if (mismatch) {
            std::cerr << "focus step " << step << " expected "
                << (std::find(controls.begin(), controls.end(), expected) - controls.begin()) << " got";
            for (std::size_t i = 0; i < controls.size(); ++i) if (controls[i]->focused()) std::cerr << ' ' << i;
            std::cerr << '\n';
        }
        ++step;
        for (const auto control : controls) {
            expectEqual("modeless Tab has one exact focused leaf", control->focused(), control == expected);
        }
    };
    expectEqual("programmatic focus enters nested content", root.requestFocus(first.get(), true), true);
    expectFocus(first.get());
    KeyEvent tab;
    tab.key = Key::Tab;
    tab.virtualKey = 0x09;
    for (const auto expected : {second.get(), high.get(), low.get(), after.get(), before.get(), first.get()}) {
        expectEqual("modeless forward Tab handled", root.onKeyDown(tab), true);
        expectFocus(expected);
    }
    tab.shift = true;
    for (const auto expected : {before.get(), after.get(), low.get(), high.get(), second.get(), first.get()}) {
        expectEqual("modeless reverse Tab handled", root.onKeyDown(tab), true);
        expectFocus(expected);
    }
}

void testRepeatedTabCyclesPreserveTheSurfaceActionBeforeItsChild() {
    using namespace oneui;
    auto before = std::make_shared<Button>(L"Before");
    auto surface = std::make_shared<InteractiveSurface>();
    auto nestedAction = std::make_shared<Button>(L"Nested action");
    auto after = std::make_shared<Button>(L"After");
    surface->setOnClick([] {});
    surface->setContent(nestedAction);
    auto group = std::make_shared<Stack>();
    group->add(surface);
    auto column = std::make_shared<Stack>();
    column->add(before);
    column->add(group);
    column->add(after);
    OverlayHost root;
    root.setContent(column);
    root.setFrame({0, 0, 480, 320});
    const std::vector<Widget*> stops{
        before.get(), surface.get(), nestedAction.get(), after.get()};
    const auto expectFocus = [&](Widget* expected) {
        for (const auto stop : stops) {
            const bool ownStop = stop->focused() &&
                (stop != surface.get() || !surface->activeFocusChild());
            expectEqual("repeated traversal has one semantic stop", ownStop, stop == expected);
        }
    };
    expectEqual("focus starts before the action row", root.requestFocus(before.get(), true), true);
    expectFocus(before.get());
    KeyEvent tab;
    tab.key = Key::Tab;
    tab.virtualKey = 0x09;
    const std::vector<Widget*> forward{
        surface.get(), nestedAction.get(), after.get(), before.get()};
    const std::vector<Widget*> reverse{
        after.get(), nestedAction.get(), surface.get(), before.get()};
    for (int round = 0; round < 2; ++round) {
        for (const auto expected : forward) {
            expectEqual("forward traversal returns through the row action", root.onKeyDown(tab), true);
            expectFocus(expected);
        }
    }
    tab.shift = true;
    for (int round = 0; round < 2; ++round) {
        for (const auto expected : reverse) {
            expectEqual("reverse traversal returns through the row action", root.onKeyDown(tab), true);
            expectFocus(expected);
        }
    }
}
