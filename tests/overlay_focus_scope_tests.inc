void testContentFocusIsExclusiveWithModalAndSafeAfterReplacement() {
    oneui::OverlayHost root;
    auto content = std::make_shared<oneui::View>();
    auto field = std::make_shared<oneui::Button>(L"Document field");
    content->add(field);
    root.setContent(content);
    root.requestFocus(field.get(), true);
    auto modal = std::make_shared<oneui::View>();
    auto first = std::make_shared<oneui::Button>(L"Cancel");
    auto last = std::make_shared<oneui::Button>(L"Submit");
    modal->add(first); modal->add(last);
    root.addOverlay(modal, oneui::OverlayOptions{10, true});
    root.onKeyDown({oneui::Key::Tab});
    expectEqual("modal clears focus outside View children", field->focused(), false);
    expectEqual("modal has exactly one focused leaf", int(first->focused()) + int(last->focused()), 1);
    root.onKeyDown({oneui::Key::Tab});
    expectEqual("modal wraps without escaping to document", field->focused(), false);
    root.removeOverlay(modal.get());
    expectEqual("closing modal restores previous content leaf", field->focused(), true);
    expectEqual("closing modal clears both modal leaves", int(first->focused()) + int(last->focused()), 0);

    root.addOverlay(modal, oneui::OverlayOptions{10, true});
    root.onKeyDown({oneui::Key::Tab});
    std::weak_ptr<oneui::View> oldContent = content;
    auto replacement = std::make_shared<oneui::Button>(L"Replacement");
    root.setContent(replacement);
    content.reset();
    expectEqual("replaced content is not retained by focus history", oldContent.expired(), true);
    root.removeOverlay(modal.get());
    expectEqual("replacement does not revive detached leaf", field->focused(), false);
    expectEqual("forward entry accepts direct leaf content", root.focusFirstLeaf(), true);
    expectEqual("replacement remains focusable after modal closes", replacement->focused(), true);
    root.onFocusChanged(false);
    expectEqual("reverse entry accepts direct leaf content", root.focusLastLeaf(), true);
    expectEqual("reverse entry focuses direct leaf content", replacement->focused(), true);
    root.onFocusChanged(false);
    replacement->setDisabled(true);
    expectEqual("disabled leaf rejects forward entry", root.focusFirstLeaf(), false);
    expectEqual("disabled leaf rejects reverse entry", root.focusLastLeaf(), false);
    expectEqual("disabled leaf remains unfocused", replacement->focused(), false);
    replacement->setDisabled(false);
    replacement->setVisible(false);
    expectEqual("hidden leaf rejects forward entry", root.focusFirstLeaf(), false);
    expectEqual("hidden leaf rejects reverse entry", root.focusLastLeaf(), false);
    expectEqual("hidden leaf remains unfocused", replacement->focused(), false);
    replacement->setVisible(true);
    root.focusFirstLeaf();
    root.setContent(nullptr);
    expectEqual("removing restored content clears focus", replacement->focused(), false);
}

class ReentrantTabButton final : public oneui::Widget {
public:
    std::function<void()> onTab;
    void paint(oneui::Canvas&) override {}
    bool isFocusable() const override { return true; }
    bool onKeyDown(const oneui::KeyEvent& event) override {
        if (event.key == oneui::Key::Tab && onTab) onTab();
        return false;
    }
};

void testKeyboardDispatchScopeRestoresAfterExceptionAndReentry() {
    oneui::OverlayHost root;
    auto content = std::make_shared<oneui::View>();
    auto nested = std::make_shared<oneui::OverlayHost>();
    auto field = std::make_shared<ReentrantTabButton>();
    auto next = std::make_shared<oneui::Button>(L"Next");
    nested->setContent(field);
    content->add(nested); content->add(next); root.setContent(content);
    root.requestFocus(field.get());
    field->onTab = [] { throw std::runtime_error("dispatch fixture"); };
    bool threw = false;
    try { root.onKeyDown({oneui::Key::Tab}); }
    catch (const std::runtime_error&) { threw = true; }
    expectEqual("nested callback exception is preserved", threw, true);
    int reentered = 0;
    field->onTab = [&] { ++reentered; root.onKeyDown({oneui::Key::F2}); };
    root.onKeyDown({oneui::Key::Tab});
    expectEqual("reentrant dispatch returns to outer parent traversal", next->focused(), true);
    expectEqual("nested reentrant callback occurs once", reentered, 1);
    root.onKeyDown({oneui::Key::Tab});
    expectEqual("root still wraps after exception and reentry unwind", field->focused(), true);
    expectEqual("root wrapping clears prior sibling", next->focused(), false);
}
