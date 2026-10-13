// Included by control_behavior_tests so the configured capture target also
// exercises the virtual template viewport without a second build directory.
void testVirtualTemplateViewport() {
    using namespace oneui;
    const auto check = [](bool value, const char* message) { expectTrue(message, value); };
    auto list = std::make_shared<VirtualTemplateList>();
    std::map<std::string, std::shared_ptr<Button>> live;
    std::map<std::string, int> creates;
    int retired = 0, clicks = 0;
    std::string clicked;
    list->setFactory([&](size_t, const std::string& key) {
        auto row = std::make_shared<Button>(std::wstring(key.begin(), key.end()));
        row->setPreferredSize({100, key == "999" ? 100.0f : 32.0f});
        row->setOnClick([&, key] { ++clicks; clicked = key; });
        live[key] = row;
        ++creates[key];
        return row;
    }, [&](const std::string& key) { live.erase(key); ++retired; });
    list->setEstimatedHeight(32);
    list->setFrame({0, 0, 320, 160});
    std::vector<std::string> keys;
    for (int i = 0; i < 1000; ++i) keys.push_back(std::to_string(i));
    list->setKeys(keys);
    check(list->mountedCount() <= 9, "virtual list mounts viewport and bounded overscan");
    const auto first = live.at("0");
    list->setScrollOffset(32);
    check(live.at("0") == first && creates["0"] == 1, "overlapping rows preserve widget identity");
    list->setScrollOffset(3200);
    check(!live.count("0") && retired > 0, "offscreen rows retire");
    const auto before = list->rowFrame("100").y;
    keys.insert(keys.begin(), "prepend");
    list->setKeys(keys);
    check(std::abs(list->rowFrame("100").y - before) < 0.01f, "prepend preserves stable key scroll anchor");
    auto row = live.at("100");
    auto rect = row->frame();
    list->onMouseDown({{rect.x + 20, rect.y + 10}, MouseButton::Left});
    list->onMouseUp({{rect.x + 20, rect.y + 10}, MouseButton::Left});
    check(clicks == 1 && clicked == "100", "virtual row pointer invokes original key");
    list->setScrollOffset(10000);
    check(live.count("100") && live.at("100") == row, "focused row survives scrolling offscreen");
    // 10 viewport+overscan rows plus the one retained focused row.
    check(list->mountedCount() <= 11, "focused retention is bounded");
    keys.erase(std::find(keys.begin(), keys.end(), "100"));
    list->setKeys(keys);
    check(!live.count("100"), "deleting focused row retires it");
    list->setFollowEnd(true);
    list->setScrollOffset(list->maxScrollOffset());
    const auto last = live.at("999");
    last->setPreferredSize({100, 260});
    list->refresh();
    check(std::abs(list->scrollOffset() - list->maxScrollOffset()) < 0.01f, "stream height follows end when pinned");
    check(live.at("999") == last, "stream height retains last widget");
    list->setScrollOffset(6400);
    const auto offset = list->scrollOffset();
    keys.push_back("append");
    list->setKeys(keys);
    check(std::abs(list->scrollOffset() - offset) < 0.01f, "append does not pull reader from history");
    list->setFrame({0, 0, 180, 240});
    check(list->scrollOffset() <= list->maxScrollOffset() && list->mountedCount() <= 20, "resize clamps and bounds rows");
    const auto wheel = list->scrollOffset();
    check(list->onMouseWheel({{20, 20}, -1}) && list->scrollOffset() > wheel, "native wheel scrolls virtual rows");
    bool rejected = false;
    try { list->setKeys({"duplicate", "duplicate"}); } catch (const std::invalid_argument&) { rejected = true; }
    check(rejected && list->mountedCount() > 0, "duplicate keys reject without replacing list");
    list->setKeys({});
    check(live.empty() && list->children().empty() && list->scrollOffset() == 0, "empty list releases all rows and focus");
}
