namespace {

template<class Check>
void testAccessibleSnapshot(Check& check, HWND window) {
    using namespace oneui;
    auto alive = std::make_shared<std::atomic_bool>(true);
    auto root = std::make_shared<OverlayHost>();
    root->setFrame({0, 0, 360, 260});
    auto background = std::make_shared<View>();
    background->setFrame(root->frame());
    auto button = std::make_shared<Button>(L"Main action");
    button->setFrame({25, 25, 120, 30});
    background->add(button);
    root->setContent(background);

    auto context = std::make_shared<win32_accessibility::Context>();
    context->window = window;
    context->alive = alive;
    context->root = [&root] { return root; };
    context->focus = [&root](Widget* widget) { return root->requestFocus(widget, true); };
    context->scale = [] { return 1.0f; };
    context->frameRevision = [] { return std::uint64_t{0}; };
    int rebuilds = 0;
    context->recordSnapshot = [&](double, std::size_t, std::size_t) { ++rebuilds; };
    auto* provider = new win32_accessibility::Provider(context);
    const auto count = [&] {
        LONG result = -1;
        provider->get_accChildCount(&result);
        return result;
    };

    check(count() == 1 && rebuilds == 1, "Initial accessible snapshot enumerates the main action");
    for (int i = 0; i < 24; ++i) {
        VARIANT target{};
        provider->get_accFocus(&target);
        check(count() == 1, "Unchanged accessible child count stays stable");
    }
    check(rebuilds == 1, "Repeated count and focus queries share the stored snapshot");

    // Pure movement keeps the projection. Hit testing still uses current bounds.
    button->setFrame({40, 35, 120, 30});
    POINT point{55, 45};
    ClientToScreen(window, &point);
    VARIANT hit{};
    check(provider->accHitTest(point.x, point.y, &hit) == S_OK && hit.lVal == 1 && rebuilds == 1,
        "Hit testing uses live geometry without re-enumerating stable children");

    button->setVisible(false);
    check(count() == 0 && rebuilds == 2, "A visibility change invalidates the stored snapshot");
    check(count() == 0 && rebuilds == 2, "A valid empty snapshot is reusable");
    button->setVisible(true);
    check(count() == 1 && rebuilds == 3, "A previously hidden control can rejoin the projection");

    auto popup = std::make_shared<Popup>();
    popup->setFrame({175, 25, 140, 90});
    auto popupButton = std::make_shared<Button>(L"Popup action");
    popupButton->setFrame({180, 30, 90, 28});
    popup->setContent(popupButton);
    background->add(popup);
    check(count() == 1, "Closed popup children stay out of the projection");
    popup->setOpen(true);
    check(count() == 2, "Opening a popup invalidates the snapshot");

    auto dialog = std::make_shared<Button>(L"Modal action");
    dialog->setFrame({15, 15, 130, 42});
    root->addOverlay(dialog, OverlayOptions::modal(20));
    check(count() == 1, "A modal overlay hides background accessibility children");
    VARIANT first{};
    first.vt = VT_I4;
    first.lVal = 1;
    BSTR name = nullptr;
    check(provider->get_accName(first, &name) == S_OK && name && std::wstring(name) == L"Modal action",
        "Modal traversal preserves the accessible target");
    SysFreeString(name);
    root->removeOverlay(dialog.get());
    check(count() == 2, "Closing a modal restores the background projection");
    popup->setOpen(false);
    check(count() == 1, "Closing a popup removes its children");

    auto unnamed = std::make_shared<View>();
    unnamed->setFrame({0, 80, 120, 42});
    background->add(unnamed);
    check(count() == 1, "A nonsemantic child is not exposed");
    unnamed->setAccessibleName(L"Named after mounting");
    check(count() == 2, "Changing semantic membership invalidates the snapshot");
    background->clearChildren();
    check(count() == 0, "Clearing the tree removes retained entries");
    root = std::make_shared<OverlayHost>();
    root->setFrame({0, 0, 360, 260});
    root->setAccessibleName(L"New root");
    check(count() == 1, "A replacement root has its own projection");
    provider->Release();
}

bool snapshotTracksNativeFrameChanges() {
    using namespace oneui;
    WindowOptions options;
    options.visible = false;
    auto window = Window::create(options);
    auto root = std::make_shared<View>();
    auto action = std::make_shared<Button>(L"Geometry probe");
    root->add(action);
    window->setContent(root);
    window->initialize();
    root->setFrame({0, 0, 360, 260});
    action->setFrame({25, 25, 120, 30});
    IAccessible* accessible = nullptr;
    const auto hwnd = static_cast<HWND>(window->nativeHandle());
    if (FAILED(AccessibleObjectFromWindow(hwnd, OBJID_CLIENT, IID_IAccessible,
            reinterpret_cast<void**>(&accessible))) || !accessible) {
        window->close();
        return false;
    }
    const auto only = [&](LONG expected) {
        LONG children = -1;
        return accessible->get_accChildCount(&children) == S_OK && children == expected;
    };
    bool correct = only(1);
    action->setFrame({25, 25, 0, 30});
    correct = only(0) && correct;
    action->setFrame({25, 25, 140, 48});
    correct = only(1) && correct;
    accessible->Release();
    window->close();
    return correct;
}

} // namespace
