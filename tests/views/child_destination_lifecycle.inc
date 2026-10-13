// Use the actual Win32 Window implementation: its empty root has no View
// ancestor, so only native-window composition ownership can reject stealing it.
void childDestinationNativeWindowContract(const char* path) {
#ifdef _WIN32
    ApiCase parent(path);
    parent.start();
    oneui::WindowOptions options;
    options.visible = false;
    options.width = 320;
    options.height = 240;
    auto otherWindow = oneui::Window::create(options);
    otherWindow->initialize();
    auto panel = std::make_shared<oneui::Panel>();
    otherWindow->setContent(panel);
    expect(panel->children().empty() && panel->hasCompositionOwner(),
           "actual other native window owns directly assigned empty Panel root");
    OneUiWidget destination;
    destination.widget = panel;
    OneUiViewMount* mount = nullptr;
    OneUiUtf8String name{"child-slot", 10};
    expect(oneui_view_mount_child_with_destination(parent.mount, name, &destination, &mount)
           == ONEUI_VIEW_BUSY && !mount && panel->hasCompositionOwner() && panel->children().empty(),
           "explicit destination cannot steal another native window's empty root");
    otherWindow->setContent(nullptr);
    expect(!panel->hasCompositionOwner(), "native setContent releases old root ownership");
    expect(oneui_view_mount_child_with_destination(parent.mount, name, &destination, &mount) == 0,
           "native root becomes eligible only after explicit owner detach");
    oneui_view_mount_destroy(mount);
    otherWindow->close();
#else
    (void)path;
#endif
}

void childDestinationTeardownContract(const char* path) {
    for (const bool windowClose : {false, true}) {
        ApiCase parent(path), child(path);
        parent.start();
        OneUiWidget destination;
        auto panel = std::make_shared<oneui::Panel>();
        destination.widget = panel;
        OneUiViewMount* mount = nullptr;
        OneUiUtf8String name{"child-slot", 10};
        expect(oneui_view_mount_child_with_destination(parent.mount, name, &destination, &mount) == 0,
               "teardown fixture attaches explicit destination");
        OneUiView* view = nullptr;
        expect(oneui_view_instantiate(mount, child.bytes.data(), child.bytes.size(),
               &child.vm.table, &child.vm, &view) == 0, "teardown fixture instantiates child");
        auto surface = mount->value->childSurface.lock();
        auto childMount = mount->value;
        int callbacks = 0;
        bool closingObserved = false;
        surface->setRectInvalidator([&](oneui::Rect) {
            ++callbacks;
            closingObserved = parent.window.window->closed() || parent.view->value->closing;
            // Reenter the same cleanup while parent View/window teardown runs.
            childMount->close();
            throw std::runtime_error("invalidation failed during teardown");
        });
        if (windowClose)
            parent.window.window->close();
        else
            expect(oneui_view_disconnect(parent.view) == 0,
                   "parent disconnect completes despite child invalidator exceptions");
        expect(callbacks >= 2 && closingObserved && childMount->closed &&
               !childMount->window && !childMount->destination && surface->children().empty() &&
               panel->children().empty() && !panel->hasCompositionOwner(),
               "reentrant throwing invalidator cannot interrupt committed child teardown");
        expect(oneui_view_flush(view) == ONEUI_VIEW_CLOSED && child.vm.disconnects == 1 &&
               child.vm.retains == child.vm.releases,
               "throwing cleanup still disconnects and releases child VM exactly once");
        oneui_view_destroy(view);
        // Keep the closed mount alive: releasing its reservation must not rely
        // on the wrapper destructor or weak-pointer expiry.
        ApiCase replacement(path);
        replacement.start();
        OneUiViewMount* remount = nullptr;
        expect(oneui_view_mount_child_with_destination(replacement.mount, name, &destination, &remount)
               == 0, "completed exceptional teardown releases Panel reservation");
        oneui_view_mount_destroy(remount);
        oneui_view_mount_destroy(mount);
    }
}
