#include "oneui/layout/virtual_template_list.h"
#include "../support/recording_canvas.h"

void virtualTemplateMetricsContract(const char* runtimeFile) {
    ApiCase c(runtimeFile);
    c.start();
    auto list = std::make_shared<oneui::VirtualTemplateList>();
    list->setAccessibleName(L"metrics-template-list");
    list->setFrame({13, 29, 320, 160});
    list->setEstimatedHeight(32);
    list->setFactory([](size_t index, const std::string&) {
        auto row = std::make_shared<oneui::Panel>();
        row->setPreferredSize({300, index == 23 ? 96.f : 32.f});
        return row;
    }, {});
    std::vector<std::string> keys;
    for (int i = 0; i < 24; ++i) keys.push_back(std::to_string(i));
    list->setKeys(keys);
    c.view->value->root->element.widget = list;
    oneui::test_support::RecordingCanvas canvas;
    list->paint(canvas);
    OneUiViewElementMetrics metrics{};
    metrics.struct_size = sizeof(metrics);
    const auto read = [&] {
        expect(oneui_view_element_metrics(c.view, {"metrics-template-list", 21}, &metrics) == 0,
               "query actual virtual template viewport");
        expect(metrics.scrollable && metrics.scroll_x == 0 &&
                   metrics.scroll_y == list->scrollOffset() &&
                   metrics.max_scroll_y == list->maxScrollOffset(),
               "virtual template metrics expose native offsets");
    };
    read();
    expect(metrics.x == 13 && metrics.y == 29 && metrics.width == 320 && metrics.height == 160 &&
               metrics.scroll_y == 0 && metrics.max_scroll_y == 608 && list->mountedCount() > 0,
           "real rows create an overflowing viewport at its actual frame");
    expect(list->onMouseWheel({{30, 50}, -2}), "wheel is consumed by overflowing virtual rows");
    list->paint(canvas);
    read();
    expect(metrics.scroll_y == 96 && list->rowFrame("3").y == 29,
           "wheel metrics match the row moved to the viewport top");
    list->setFollowEnd(true);
    oneui::KeyEvent end;
    end.key = oneui::Key::End;
    expect(list->onKeyDown(end), "End scrolls the actual virtual template list");
    list->paint(canvas);
    read();
    const auto last = list->rowFrame("23");
    expect(metrics.scroll_y == 672 && metrics.max_scroll_y == 672 && last.height == 96 &&
               last.y + last.height == metrics.y + metrics.height,
           "measured tall final row is at the reported scroll end");
    list->setKeys({});
    list->paint(canvas);
    read();
    expect(metrics.scroll_y == 0 && metrics.max_scroll_y == 0 && list->children().empty(),
           "empty virtual template content clears reported scroll range");
}
