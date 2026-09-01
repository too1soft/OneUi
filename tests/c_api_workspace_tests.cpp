#include "oneui/oneui_c_api.h"

#include <cmath>
#include <iostream>
#include <string>

namespace {

int failures = 0;

void expectTrue(const char* name, bool value) {
    if (!value) {
        std::cerr << name << " failed\n";
        ++failures;
    }
}

OneUiUtf8String utf8View(const std::string& value) {
    return OneUiUtf8String{value.data(), value.size()};
}

void testOperationalWorkspacePrimitivesAbi() {
    OneUiWidget* list = oneui_virtual_list_create();
    OneUiWidget* chart = oneui_time_series_chart_create();
    OneUiWidget* titleBar = oneui_title_bar_create(L"");
    OneUiWidget* leading = oneui_panel_create();
    OneUiWidget* accessory = oneui_button_create(L"Account");
    expectTrue(
        "operational workspace primitives create",
        list != nullptr && chart != nullptr && titleBar != nullptr && leading != nullptr &&
            accessory != nullptr);
    if (!list || !chart || !titleBar || !leading || !accessory) {
        oneui_widget_destroy(accessory);
        oneui_widget_destroy(leading);
        oneui_widget_destroy(titleBar);
        oneui_widget_destroy(chart);
        oneui_widget_destroy(list);
        return;
    }

    const std::string title = "ERP management";
    const std::string detail = "erp-demo.wangyunchuan.cn";
    const std::string badge = "HTTP";
    const std::string trailing = "Running";
    const OneUiRichListItemUtf8 richItem{
        utf8View(title),
        utf8View(detail),
        utf8View(badge),
        utf8View(trailing),
        OneUiColor{34, 197, 94, 255},
        OneUiColor{22, 163, 74, 255},
        1};
    oneui_virtual_list_set_rich_items_utf8(list, &richItem, 1);
    oneui_virtual_list_set_rich_metrics(
        list,
        18.0f,
        8.0f,
        18.0f,
        4.0f,
        12.0f,
        6.0f,
        54.0f,
        6.0f);
    OneUiVirtualListRichMetrics richMetrics{};
    expectTrue(
        "virtual list rich metrics round trip",
        oneui_virtual_list_rich_metrics(list, &richMetrics) == 1 &&
            std::abs(richMetrics.trailing_width - 54.0f) < 0.001f);

    const std::string updatedTrailing = "Stopped";
    const OneUiRichListItemUtf8 updatedRichItem{
        utf8View(title),
        utf8View(detail),
        utf8View(badge),
        utf8View(updatedTrailing),
        OneUiColor{239, 68, 68, 255},
        OneUiColor{220, 38, 38, 255},
        1};
    expectTrue(
        "virtual list rich item update",
        oneui_virtual_list_update_rich_item_utf8(list, 0, &updatedRichItem) == 1);
    expectTrue(
        "virtual list rich item update rejects invalid index",
        oneui_virtual_list_update_rich_item_utf8(list, 1, &updatedRichItem) == 0);

    const std::string downloadName = "Download";
    const std::string uploadName = "Upload";
    const double download[] = {0.5, 1.0, 2.0};
    const double upload[] = {0.2, 0.6, 0.8};
    const OneUiTimeSeriesUtf8 series[] = {
        {utf8View(downloadName), OneUiColor{37, 99, 235, 255}, download, 3},
        {utf8View(uploadName), OneUiColor{34, 197, 94, 255}, upload, 3},
    };
    oneui_time_series_chart_set_series(chart, series, 2);
    oneui_time_series_chart_set_range(chart, 0.0, 3.0);
    oneui_time_series_chart_set_grid_lines(chart, 4);
    oneui_time_series_chart_set_visual_style(chart, 1, 1, 1, 1, 2.25f, 32);
    oneui_time_series_chart_set_plot_insets(chart, OneUiInsets{4.0f, 2.0f, 2.0f, 2.0f});

    oneui_title_bar_set_leading(titleBar, leading);
    oneui_title_bar_set_accessory(titleBar, accessory);

    oneui_widget_destroy(accessory);
    oneui_widget_destroy(leading);
    oneui_widget_destroy(titleBar);
    oneui_widget_destroy(chart);
    oneui_widget_destroy(list);
}

} // namespace

int main() {
    testOperationalWorkspacePrimitivesAbi();
    if (failures != 0) {
        std::cerr << failures << " workspace C ABI test(s) failed.\n";
        return 1;
    }
    return 0;
}
