static void resizeViewFlushContract(const char* path) {
    ApiCase resizing(path), other(path);
    resizing.start();
    other.start();
    resizing.window.window->run();
    other.window.window->run();
    auto label = resizing.view->value->root->children[0]->element.as<oneui::Label>();
    auto otherLabel = other.view->value->root->children[0]->element.as<oneui::Label>();
    bool posted = false;
    expect(resizing.window.window->post([&] { posted = true; }), "post unrelated work");
    {
        oneui::internal::ResizeViewFlush pending(*resizing.window.window);
        resizing.vm.text(resizing.title, "new size");
        other.vm.text(other.title, "other window");
        expect(oneui_view_notify(resizing.view, resizing.title) == 0, "resize notify");
        expect(oneui_view_notify(other.view, other.title) == 0, "other window notify");
        expect(label->text() == L"initial", "size callback has not completed");
        pending.drain([] { return true; });
        expect(label->text() == L"new size", "same window bindings ready for paint");
        expect(otherLabel->text() == L"initial" && !posted, "unrelated work stays queued");
    }
    resizing.window.window->run();
    other.window.window->run();
    expect(posted && otherLabel->text() == L"other window", "normal queue still works");

    oneui::internal::ResizeViewFlush pending(*resizing.window.window);
    resizing.vm.text(resizing.title, "closed before paint");
    expect(oneui_view_notify(resizing.view, resizing.title) == 0, "notify before close");
    oneui_view_destroy(resizing.view);
    resizing.view = nullptr;
    pending.drain([] { return true; });
    expect(label->text() == L"new size", "a retired view never applies a queued refresh");
}
