void firstVisibleMetricsContract(const char* runtimeFile) {
    ApiCase c(runtimeFile);
    c.start();
    auto root = std::make_shared<oneui::View>();
    auto hidden = std::make_shared<oneui::View>();
    auto hiddenTarget = std::make_shared<oneui::Panel>();
    hiddenTarget->setAccessibleName(L"Create");
    hiddenTarget->setFrame({1, 2, 30, 40});
    hidden->add(hiddenTarget);
    hidden->setVisible(false);
    root->add(hidden);
    auto empty = std::make_shared<oneui::Panel>();
    empty->setAccessibleName(L"Create");
    root->add(empty);
    auto first = std::make_shared<oneui::Panel>();
    first->setAccessibleName(L"Create");
    first->setFrame({10, 20, 50, 60});
    root->add(first);
    auto last = std::make_shared<oneui::Panel>();
    last->setAccessibleName(L"Create");
    last->setFrame({90, 80, 70, 60});
    root->add(last);
    c.view->value->root->element.widget = root;
    OneUiViewElementMetrics metrics{};
    metrics.struct_size = sizeof(metrics);
    const auto reads = c.vm.reads;
    const auto revision = root->measureRevision();
    const auto query = [&] {
        return oneui_view_first_visible_element_metrics(c.view, {"Create", 6}, &metrics);
    };
    expect(query() == 0 && metrics.x == 10 && metrics.y == 20, "first visible duplicate bounds");
    expect(!metrics.scrollable && metrics.scroll_y == 0, "frame-only query clears scroll fields");
    expect(root->measureRevision() == revision && c.vm.reads == reads, "geometry query does not flush or invalidate");
    expect(oneui_view_element_metrics(c.view, {"Create", 6}, &metrics) == ONEUI_VIEW_INVALID_ARGUMENT,
           "legacy unique-name contract still rejects duplicates");
    first->setVisible(false);
    expect(query() == 0 && metrics.x == 90, "hidden first match falls through");
    root->setVisible(false);
    expect(query() == ONEUI_VIEW_NOT_FOUND, "hidden ancestor excludes every target");
    expect(oneui_view_first_visible_element_metrics(c.view, {"", 0}, &metrics) == ONEUI_VIEW_INVALID_ARGUMENT,
           "empty query rejected");
    expect(oneui_view_first_visible_element_metrics(c.view, {"Create", 6}, nullptr) == ONEUI_VIEW_INVALID_ARGUMENT,
           "null output rejected");
}
