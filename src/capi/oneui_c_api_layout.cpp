#include "oneui_c_api_controls_internal.h"
#include "oneui_c_api_error.h"
#include "oneui/layout/scroll_view.h"
#include "oneui/layout/split_view.h"
#include "oneui/layout/stack.h"
#include <algorithm>
#include <cmath>
#include <climits>
#include <cstring>
#include <cwchar>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace oneui::capi;

void oneui_scroll_view_set_content_width(OneUiWidget* view, float width) {
    if (auto* scroll = asWidget<oneui::ScrollView>(view)) scroll->setContentWidth(width);
}

OneUiWidget* oneui_stack_create(OneUiStackDirection direction) {
    const auto nativeDirection = direction == OneUiStackDirectionRow
        ? oneui::StackDirection::Row
        : oneui::StackDirection::Column;
    return wrap(std::make_shared<oneui::Stack>(nativeDirection));
}

void oneui_stack_add(OneUiWidget* stack, OneUiWidget* child) {
    auto* nativeStack = asWidget<oneui::Stack>(stack);
    if (!nativeStack || !child || !child->widget) {
        return;
    }
    nativeStack->add(child->widget);
}

void oneui_stack_set_direction(OneUiWidget* stack, int direction) {
    if (auto* native = asWidget<oneui::Stack>(stack)) native->setDirection(direction == 1 ? oneui::StackDirection::Row : oneui::StackDirection::Column);
}

void oneui_stack_set_gap(OneUiWidget* stack, float gap) {
    auto* nativeStack = asWidget<oneui::Stack>(stack);
    if (!nativeStack) {
        return;
    }
    nativeStack->setGap(gap);
}

void oneui_stack_set_padding(OneUiWidget* stack, OneUiInsets insets) {
    auto* nativeStack = asWidget<oneui::Stack>(stack);
    if (!nativeStack) {
        return;
    }
    nativeStack->setPadding(toNativeInsets(insets));
}

void oneui_stack_set_align(OneUiWidget* stack, OneUiStackAlign align) {
    auto* nativeStack = asWidget<oneui::Stack>(stack);
    if (!nativeStack) {
        return;
    }

    switch (align) {
    case OneUiStackAlignStart:
        nativeStack->setAlign(oneui::StackAlign::Start);
        break;
    case OneUiStackAlignCenter:
        nativeStack->setAlign(oneui::StackAlign::Center);
        break;
    case OneUiStackAlignEnd:
        nativeStack->setAlign(oneui::StackAlign::End);
        break;
    case OneUiStackAlignStretch:
    default:
        nativeStack->setAlign(oneui::StackAlign::Stretch);
        break;
    }
}

float oneui_stack_content_width(OneUiWidget* stack) {
    auto* nativeStack = asWidget<oneui::Stack>(stack);
    return nativeStack ? nativeStack->contentWidth() : 0.0f;
}

int oneui_stack_set_engine(OneUiWidget* stack, int engine) {
    auto* native = asWidget<oneui::Stack>(stack);
    if (!native || engine < 0 || engine > 1) return 0;
    try { return native->setEngine(static_cast<oneui::StackEngine>(engine)) ? 1 : 0; }
    catch (...) { oneui::capi::recordCurrentException(__func__); return 0; }
}

int oneui_stack_set_wrap(OneUiWidget* stack, int enabled) {
    auto* native = asWidget<oneui::Stack>(stack);
    if (!native || (enabled != 0 && enabled != 1)) return 0;
    return native->setWrap(enabled != 0) ? 1 : 0;
}

int oneui_stack_set_justify(OneUiWidget* stack, int justify) {
    auto* native = asWidget<oneui::Stack>(stack);
    if (!native || justify < 0 || justify > 3) return 0;
    native->setJustify(static_cast<oneui::StackJustify>(justify));
    return 1;
}

int oneui_stack_set_flex(OneUiWidget* stack, OneUiWidget* child,
    float grow, float shrink, int basis_mode, float basis, float min, float max) {
    auto* native = asWidget<oneui::Stack>(stack);
    if (!native || !child || !child->widget || basis_mode < 0 || basis_mode > 2 ||
        !std::isfinite(grow) || grow < 0 || !std::isfinite(shrink) || shrink < 0 ||
        !std::isfinite(min) || min < 0 || std::isnan(max) || max < min ||
        (basis_mode == 1 && (!std::isfinite(basis) || basis < 0))) return 0;
    if (std::find(native->children().begin(), native->children().end(), child->widget) == native->children().end()) return 0;
    try {
        oneui::StackFlex flex{grow, shrink, {}, min, max, basis_mode == 2};
        if (basis_mode == 1) flex.basis = basis;
        native->setFlex(child->widget, flex);
        return 1;
    } catch (...) { oneui::capi::recordCurrentException(__func__); return 0; }
}

void oneui_stack_clear_flex(OneUiWidget* stack, OneUiWidget* child) {
    if (auto* native = asWidget<oneui::Stack>(stack); native && child && child->widget)
        native->clearFlex(child->widget);
}

float oneui_stack_content_height(OneUiWidget* stack) {
    auto* nativeStack = asWidget<oneui::Stack>(stack);
    return nativeStack ? nativeStack->contentHeight() : 0.0f;
}

OneUiWidget* oneui_split_view_create(OneUiSplitOrientation orientation) {
    const auto nativeOrientation = orientation == OneUiSplitOrientationVertical
        ? oneui::SplitOrientation::Vertical
        : oneui::SplitOrientation::Horizontal;
    auto* wrapper = wrap(std::make_shared<oneui::SplitView>(nativeOrientation));
    if (wrapper) {
        wrapper->tag = "splitview";
    }
    return wrapper;
}

void oneui_split_view_set_first(OneUiWidget* split_view, OneUiWidget* child) {
    auto* nativeSplit = asWidget<oneui::SplitView>(split_view);
    if (!nativeSplit) {
        return;
    }
    nativeSplit->setFirst(child ? child->widget : nullptr);
}

void oneui_split_view_set_second(OneUiWidget* split_view, OneUiWidget* child) {
    auto* nativeSplit = asWidget<oneui::SplitView>(split_view);
    if (!nativeSplit) {
        return;
    }
    nativeSplit->setSecond(child ? child->widget : nullptr);
}

void oneui_split_view_set_orientation(
    OneUiWidget* split_view,
    OneUiSplitOrientation orientation) {
    auto* nativeSplit = asWidget<oneui::SplitView>(split_view);
    if (!nativeSplit) {
        return;
    }
    nativeSplit->setOrientation(orientation == OneUiSplitOrientationVertical
        ? oneui::SplitOrientation::Vertical
        : oneui::SplitOrientation::Horizontal);
}

void oneui_split_view_set_ratio(OneUiWidget* split_view, float ratio) {
    auto* nativeSplit = asWidget<oneui::SplitView>(split_view);
    if (nativeSplit) {
        nativeSplit->setSplitRatio(ratio);
    }
}

float oneui_split_view_ratio(OneUiWidget* split_view) {
    auto* nativeSplit = asWidget<oneui::SplitView>(split_view);
    return nativeSplit ? nativeSplit->splitRatio() : 0.0f;
}

void oneui_split_view_set_gap(OneUiWidget* split_view, float gap) {
    auto* nativeSplit = asWidget<oneui::SplitView>(split_view);
    if (nativeSplit) {
        nativeSplit->setGap(gap);
    }
}

void oneui_split_view_set_divider_hit_extent(OneUiWidget* split_view, float extent) {
    if (auto* nativeSplit = asWidget<oneui::SplitView>(split_view)) nativeSplit->setDividerHitExtent(extent);
}

void oneui_split_view_set_divider_colors(OneUiWidget* split_view, OneUiColor normal, OneUiColor active) {
    auto* nativeSplit = asWidget<oneui::SplitView>(split_view);
    if (nativeSplit) nativeSplit->setDividerColors(toNativeColor(normal), toNativeColor(active));
}

void oneui_split_view_set_padding(OneUiWidget* split_view, OneUiInsets insets) {
    auto* nativeSplit = asWidget<oneui::SplitView>(split_view);
    if (nativeSplit) {
        nativeSplit->setPadding(toNativeInsets(insets));
    }
}

void oneui_split_view_set_resizable(OneUiWidget* split_view, int resizable) {
    auto* nativeSplit = asWidget<oneui::SplitView>(split_view);
    if (nativeSplit) {
        nativeSplit->setResizable(resizable != 0);
    }
}

void oneui_split_view_set_minimum_pane_extent(
    OneUiWidget* split_view,
    float first,
    float second) {
    auto* nativeSplit = asWidget<oneui::SplitView>(split_view);
    if (nativeSplit) {
        nativeSplit->setMinimumPaneExtent(first, second);
    }
}

void oneui_split_view_set_on_ratio_changed(
    OneUiWidget* split_view,
    OneUiFloatCallback callback,
    void* user_data) {
    auto* nativeSplit = asWidget<oneui::SplitView>(split_view);
    if (!nativeSplit) {
        return;
    }
    nativeSplit->setOnSplitRatioChanged(callback
        ? [callback, user_data](float ratio) { callback(ratio, user_data); }
        : std::function<void(float)>{});
}

void oneui_split_view_set_on_ratio_committed(
    OneUiWidget* split_view,
    OneUiFloatCallback callback,
    void* user_data) {
    auto* nativeSplit = asWidget<oneui::SplitView>(split_view);
    if (!nativeSplit) {
        return;
    }
    nativeSplit->setOnSplitRatioCommitted(callback
        ? [callback, user_data](float ratio) { callback(ratio, user_data); }
        : std::function<void(float)>{});
}

OneUiWidget* oneui_scroll_view_create(void) {
    return wrap(std::make_shared<oneui::ScrollView>());
}

void oneui_scroll_view_set_content(OneUiWidget* view, OneUiWidget* child) {
    auto* nativeView = asWidget<oneui::ScrollView>(view);
    if (!nativeView || !child || !child->widget) {
        return;
    }
    nativeView->setContent(child->widget);
}

void oneui_scroll_view_set_content_height(OneUiWidget* view, float height) {
    auto* nativeView = asWidget<oneui::ScrollView>(view);
    if (!nativeView) {
        return;
    }
    nativeView->setContentHeight(height);
}

void oneui_scroll_view_set_wheel_step(OneUiWidget* view, float step) {
    auto* nativeView = asWidget<oneui::ScrollView>(view);
    if (!nativeView) {
        return;
    }
    nativeView->setWheelStep(step);
}

void oneui_scroll_view_set_chrome_visible(OneUiWidget* view, int visible) {
    auto* nativeView = asWidget<oneui::ScrollView>(view);
    if (!nativeView) {
        return;
    }
    nativeView->setChromeVisible(visible != 0);
}

void oneui_scroll_view_set_scrollbar_style(OneUiWidget* view, unsigned char r, unsigned char g, unsigned char b, unsigned char a, float thickness) {
    auto* nativeView = asWidget<oneui::ScrollView>(view);
    if (!nativeView) {
        return;
    }
    nativeView->setScrollbarStyle(oneui::Color{r, g, b, a}, thickness);
}

void oneui_scroll_view_set_scroll_offset(OneUiWidget* view, float offset) {
    if (auto* nativeView = asWidget<oneui::ScrollView>(view)) {
        nativeView->setScrollOffset(offset);
    }
}

float oneui_scroll_view_scroll_offset(OneUiWidget* view) {
    if (auto* nativeView = asWidget<oneui::ScrollView>(view)) {
        return nativeView->scrollOffset();
    }
    return 0.0f;
}

float oneui_scroll_view_max_scroll_offset(OneUiWidget* view) {
    if (auto* nativeView = asWidget<oneui::ScrollView>(view)) {
        return nativeView->maxScrollOffset();
    }
    return 0.0f;
}

void oneui_scroll_view_scroll_to_bottom(OneUiWidget* view) {
    auto* nativeView = asWidget<oneui::ScrollView>(view);
    if (!nativeView) {
        return;
    }
    // 初次布局前 viewport 高度为 0，此处偏移会暂设为内容全高；
    // layoutChildren 会在布局时回夹到真正的“底部”位置。
    nativeView->setScrollOffset(nativeView->maxScrollOffset());
}

void oneui_scroll_view_set_scrollbar_inset(OneUiWidget* view, float inset) {
    if (auto* nativeView = asWidget<oneui::ScrollView>(view)) nativeView->setScrollbarInset(inset);
}
