// A nested row below the viewport has never been painted when Tab reaches it.
// Its controls must receive real geometry and scroll into view in both directions.
void testUnpaintedFocusTargetsAreLaidOutAndRevealed() {
    auto column = std::make_shared<oneui::Stack>();
    auto first = std::make_shared<oneui::Button>(L"First");
    first->setPreferredSize({80.0f, 30.0f});
    auto spacer = std::make_shared<LayoutProbe>(oneui::Size{80.0f, 200.0f});
    auto row = std::make_shared<oneui::Stack>(oneui::StackDirection::Row);
    row->setPreferredSize({80.0f, 30.0f});
    auto last = std::make_shared<oneui::Button>(L"Last");
    last->setPreferredSize({80.0f, 30.0f});
    row->add(last);
    column->add(first);
    column->add(spacer);
    column->add(row);
    oneui::ScrollView scroll;
    scroll.setFrame({0.0f, 0.0f, 120.0f, 80.0f});
    scroll.setContent(column);
    scroll.setContentHeight(260.0f);
    NullCanvas canvas;
    expectEqual("forward focus enters scroll content", scroll.focusFirstLeaf(), true);
    expectEqual("first focus has nonzero width before paint", first->frame().width > 0.0f, true);
    expectEqual("Tab reaches the unpainted nested row", scroll.onKeyDown({oneui::Key::Tab}), true);
    expectEqual("nested target has nonzero width before paint", last->frame().width > 0.0f, true);
    expectEqual("Tab reveals the below-viewport focus", scroll.scrollOffset() > 0.0f, true);
    scroll.paint(canvas);
    expectEqual("focused bottom control is inside viewport",
                last->frame().y >= 0.0f && last->frame().y + last->frame().height <= 80.01f, true);
    expectEqual("Shift Tab returns to the first control",
                scroll.onKeyDown({oneui::Key::Tab, true}), true);
    expectNear("reverse focus scrolls back to start", scroll.scrollOffset(), 0.0f);
    scroll.setScrollOffset(0.0f);
    expectEqual("reverse scope entry succeeds", scroll.focusLastLeaf(), true);
    expectEqual("reverse entry reveals the last control", scroll.scrollOffset() > 0.0f, true);
    scroll.setScrollOffset(0.0f);
    scroll.focusFirstLeaf();
    expectEqual("programmatic descendant focus succeeds", scroll.requestFocus(last.get()), true);
    expectEqual("programmatic focus reveals the last control", scroll.scrollOffset() > 0.0f, true);
}
