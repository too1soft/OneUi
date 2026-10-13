#include "oneui_c_api_controls_internal.h"
#include "oneui/style_adapter.h"
#include "oneui_c_api_error.h"
#include "oneui/controls/badge.h"
#include "oneui/controls/button.h"
#include "oneui/controls/card.h"
#include "oneui/controls/checkbox.h"
#include "oneui/controls/dialog.h"
#include "oneui/controls/icon_badge.h"
#include "oneui/controls/menu.h"
#include "oneui/controls/icon_button.h"
#include "oneui/controls/icon_view.h"
#include "oneui/controls/interactive_surface.h"
#include "oneui/controls/label.h"
#include "oneui/controls/list.h"
#include "oneui/controls/virtual_list.h"
#include "oneui/controls/nav_item.h"
#include "oneui/controls/popup.h"
#include "oneui/controls/progress_bar.h"
#include "oneui/controls/slider.h"
#include "oneui/controls/sparkline.h"
#include "oneui/controls/radio_group.h"
#include "oneui/controls/realtime_frame_view.h"
#include "oneui/controls/remote_input_region.h"
#include "oneui/controls/select.h"
#include "oneui/controls/status_strip.h"
#include "oneui/controls/state_view.h"
#include "oneui/controls/switch.h"
#include "oneui/controls/table.h"
#include "oneui/controls/time_series_chart.h"
#include "oneui/controls/tabs.h"
#include "oneui/controls/text_field.h"
#include "oneui/controls/terminal_view.h"
#include "oneui/controls/tree_view.h"
#include "oneui/controls/tile.h"
#include "oneui/controls/toast.h"
#include "oneui/controls/window_title_bar.h"
#include "oneui/layout/app_shell.h"
#include "oneui/layout/overlay_host.h"
#include "oneui/layout/panel.h"
#include "oneui/layout/reorderable_grid.h"
#include "oneui/layout/scroll_view.h"
#include "oneui/layout/stack.h"
#include "oneui/layout/top_bar.h"
#include "oneui/tabs_style_adapter.h"
#include "oneui_c_api_window_internal.h"
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

namespace oneui::capi {

oneui::StyleNode styleNodeFor(const OneUiWidget* widget) {
    std::string tag = widget->tag;
    if (tag.empty()) {
        if (dynamic_cast<oneui::Button*>(widget->widget.get())) {
            tag = "button";
        } else if (dynamic_cast<oneui::Badge*>(widget->widget.get())) {
            tag = "badge";
        } else if (dynamic_cast<oneui::Card*>(widget->widget.get())) {
            tag = "card";
        } else if (dynamic_cast<oneui::Tile*>(widget->widget.get())) {
            tag = "tile";
        } else if (dynamic_cast<oneui::Toast*>(widget->widget.get())) {
            tag = "toast";
        } else if (dynamic_cast<oneui::Checkbox*>(widget->widget.get())) {
            tag = "checkbox";
        } else if (dynamic_cast<oneui::RadioGroup*>(widget->widget.get())) {
            tag = "radio";
        } else if (dynamic_cast<oneui::StatusStrip*>(widget->widget.get())) {
            tag = "status-strip";
        } else if (dynamic_cast<oneui::StateView*>(widget->widget.get())) {
            tag = "state-view";
        } else if (dynamic_cast<oneui::Tabs*>(widget->widget.get())) {
            tag = "segmented-control";
        } else if (dynamic_cast<oneui::Select*>(widget->widget.get())) {
            tag = "select";
        } else if (dynamic_cast<oneui::ProgressBar*>(widget->widget.get())) {
            tag = "progress";
        } else if (dynamic_cast<oneui::Slider*>(widget->widget.get())) {
            tag = "slider";
        } else if (dynamic_cast<oneui::Sparkline*>(widget->widget.get())) {
            tag = "sparkline";
        } else if (dynamic_cast<oneui::TimeSeriesChart*>(widget->widget.get())) {
            tag = "time-series-chart";
        } else if (dynamic_cast<oneui::VirtualList*>(widget->widget.get())) {
            tag = "virtual-list";
        } else if (dynamic_cast<oneui::List*>(widget->widget.get())) {
            tag = "list";
        } else if (dynamic_cast<oneui::Table*>(widget->widget.get())) {
            tag = "table";
        } else if (dynamic_cast<oneui::TreeView*>(widget->widget.get())) {
            tag = "tree-view";
        } else if (dynamic_cast<oneui::TextField*>(widget->widget.get())) {
            tag = "input";
        } else if (dynamic_cast<oneui::IconView*>(widget->widget.get())) {
            tag = "icon";
        } else if (dynamic_cast<oneui::IconBadge*>(widget->widget.get())) {
            tag = "icon-badge";
        } else if (dynamic_cast<oneui::Menu*>(widget->widget.get())) {
            tag = "menu";
        } else if (dynamic_cast<oneui::Dialog*>(widget->widget.get())) {
            tag = "dialog";
        } else if (dynamic_cast<oneui::IconButton*>(widget->widget.get())) {
            tag = "button";
        } else if (dynamic_cast<oneui::Switch*>(widget->widget.get())) {
            tag = "switch";
        } else if (dynamic_cast<oneui::WindowTitleBar*>(widget->widget.get())) {
            tag = "titlebar";
        } else if (dynamic_cast<oneui::ScrollView*>(widget->widget.get())) {
            tag = "scroll-view";
        } else if (dynamic_cast<oneui::ReorderableGrid*>(widget->widget.get())) {
            tag = "reorderable-grid";
        } else if (dynamic_cast<oneui::NavItem*>(widget->widget.get())) {
            tag = "button";
        } else if (dynamic_cast<oneui::AppShell*>(widget->widget.get())) {
            tag = "app-shell";
        } else if (dynamic_cast<oneui::OverlayHost*>(widget->widget.get())) {
            tag = "overlay-host";
        } else if (dynamic_cast<oneui::TopBar*>(widget->widget.get())) {
            tag = "topbar";
        } else if (dynamic_cast<oneui::InteractiveSurface*>(widget->widget.get())) {
            tag = "interactive-surface";
        } else if (dynamic_cast<oneui::Label*>(widget->widget.get())) {
            tag = "label";
        } else if (dynamic_cast<oneui::Stack*>(widget->widget.get())) {
            tag = "stack";
        } else if (dynamic_cast<oneui::RealtimeFrameView*>(widget->widget.get())) {
            tag = "realtime-frame-view";
        } else if (dynamic_cast<oneui::RemoteInputRegion*>(widget->widget.get())) {
            tag = "remote-input-region";
        } else if (dynamic_cast<oneui::TerminalView*>(widget->widget.get())) {
            tag = "terminal-view";
        } else {
            tag = "section";
        }
    }
    return oneui::StyleNode{std::move(tag), widget->classes, oneui::StyleStateNone};
}

void applyPreferredSizeFromStyle(oneui::Widget& widget, const oneui::StyleBox& box) {
    if (!box.width && !box.height) {
        return;
    }
    oneui::Size size = widget.preferredSize();
    if (box.width) {
        size.width = *box.width;
    }
    if (box.height) {
        size.height = *box.height;
    }
    widget.setPreferredSize(size);
}

void applyStyleSheet(OneUiWidget* wrapper, std::shared_ptr<oneui::StyleSheet> sheet) {
    if (!wrapper || !wrapper->widget || !sheet) {
        return;
    }
    wrapper->styleSheet = std::move(sheet);
    syncStyleBinding(wrapper);
    // Transient surfaces own a palette boundary; normal descendants inherit it.
    if (dynamic_cast<oneui::Dialog*>(wrapper->widget.get()) ||
        dynamic_cast<oneui::Popup*>(wrapper->widget.get()) ||
        dynamic_cast<oneui::Menu*>(wrapper->widget.get())) {
        wrapper->widget->setStyleScope(wrapper->styleSheet->transientStyleSheet());
    }
    const auto effectiveSheet = wrapper->widget->styleScope()
        ? wrapper->widget->styleScope() : wrapper->styleSheet;
    const oneui::StyleNode node = styleNodeFor(wrapper);
    const auto resolvedBox = effectiveSheet->resolve(node);
    applyPreferredSizeFromStyle(*wrapper->widget, resolvedBox);
    wrapper->widget->setFontFamily(resolvedBox.fontFamily.value_or(L""));

    if (auto* chart = dynamic_cast<oneui::TimeSeriesChart*>(wrapper->widget.get())) {
        chart->setStyleBox(effectiveSheet->resolve(node));
        return;
    }
    if (auto* button = dynamic_cast<oneui::Button*>(wrapper->widget.get())) {
        button->setStyleOverride(oneui::buttonStyleOverrideFromStyleSheet(*effectiveSheet, node));
        return;
    }
    if (auto* badge = dynamic_cast<oneui::Badge*>(wrapper->widget.get())) {
        const oneui::StyleBox box = effectiveSheet->resolve(node);
        oneui::BadgeStyleOverride style;
        style.background = box.background.color;
        style.foreground = box.foreground;
        style.border = box.borderColor;
        style.borderWidth = box.borderWidth;
        style.radius = box.radius;
        style.padding = box.padding;
        style.fontSize = box.fontSize;
        badge->setStyleOverride(style);
        return;
    }
    if (auto* card = dynamic_cast<oneui::Card*>(wrapper->widget.get())) {
        const oneui::StyleBox box = effectiveSheet->resolve(node);
        if (box.padding) {
            card->setPadding(*box.padding);
        }
        card->setStyleBox(box);
        return;
    }
    if (auto* iconBadge = dynamic_cast<oneui::IconBadge*>(wrapper->widget.get())) {
        iconBadge->setStyleBox(effectiveSheet->resolve(node));
        return;
    }
    if (auto* popup = dynamic_cast<oneui::Popup*>(wrapper->widget.get())) {
        popup->setStyleOverride(
            oneui::popupStyleOverrideFromStyleSheet(*effectiveSheet, node));
        return;
    }
    if (auto* menu = dynamic_cast<oneui::Menu*>(wrapper->widget.get())) {
        menu->setStyleSheet(effectiveSheet, node);
        return;
    }
    if (auto* dialog = dynamic_cast<oneui::Dialog*>(wrapper->widget.get())) {
        dialog->setStyleSheet(effectiveSheet, node);
        return;
    }
    if (auto* tile = dynamic_cast<oneui::Tile*>(wrapper->widget.get())) {
        tile->setStyleSheet(effectiveSheet, node);
        return;
    }
    if (auto* toast = dynamic_cast<oneui::Toast*>(wrapper->widget.get())) {
        toast->setStyleSheet(effectiveSheet, node);
        return;
    }
    if (auto* radio = dynamic_cast<oneui::RadioGroup*>(wrapper->widget.get())) {
        auto stateOverride = [](const oneui::StyleBox& box) {
            oneui::RadioGroupStateStyleOverride state;
            state.itemBackground = box.background.color;
            state.labelColor = box.foreground;
            state.selectedLabelColor = box.foreground;
            state.indicatorBorder = box.borderColor;
            state.indicatorFill = box.foreground;
            state.indicatorBackground = box.content.backgroundColor;
            state.itemRadius = box.radius;
            state.indicatorRadius = box.content.radius;
            return state;
        };
        oneui::RadioGroupStyleOverride style;
        style.normal = stateOverride(effectiveSheet->resolve(oneui::StyleNode{node.tag, node.classes, oneui::StyleStateNone}));
        style.hovered = stateOverride(effectiveSheet->resolve(oneui::StyleNode{node.tag, node.classes, oneui::StyleStateHover}));
        style.pressed = stateOverride(effectiveSheet->resolve(oneui::StyleNode{node.tag, node.classes, oneui::StyleStateActive}));
        style.selected = stateOverride(effectiveSheet->resolve(oneui::StyleNode{node.tag, node.classes, oneui::StyleStateSelected}));
        style.disabled = stateOverride(effectiveSheet->resolve(oneui::StyleNode{node.tag, node.classes, oneui::StyleStateDisabled}));
        radio->setStyleOverride(style);
        return;
    }
    if (auto* checkbox = dynamic_cast<oneui::Checkbox*>(wrapper->widget.get())) {
        auto stateOverride = [](const oneui::StyleBox& box) {
            oneui::CheckboxStateStyleOverride state;
            state.boxBackground = box.background.color;
            state.boxBorder = box.borderColor;
            state.checkColor = box.content.backgroundColor;
            state.labelColor = box.foreground;
            state.borderWidth = box.borderWidth;
            state.radius = box.radius;
            return state;
        };
        oneui::CheckboxStyleOverride style;
        style.normal = stateOverride(effectiveSheet->resolve(oneui::StyleNode{node.tag, node.classes, oneui::StyleStateNone}));
        style.hovered = stateOverride(effectiveSheet->resolve(oneui::StyleNode{node.tag, node.classes, oneui::StyleStateHover}));
        style.pressed = stateOverride(effectiveSheet->resolve(oneui::StyleNode{node.tag, node.classes, oneui::StyleStateActive}));
        style.selected = stateOverride(effectiveSheet->resolve(oneui::StyleNode{node.tag, node.classes, oneui::StyleStateSelected}));
        style.disabled = stateOverride(effectiveSheet->resolve(oneui::StyleNode{node.tag, node.classes, oneui::StyleStateDisabled}));
        checkbox->setStyleOverride(style);
        return;
    }
    if (auto* statusStrip = dynamic_cast<oneui::StatusStrip*>(wrapper->widget.get())) {
        statusStrip->setStyleSheet(effectiveSheet, node);
        return;
    }
    if (auto* stateView = dynamic_cast<oneui::StateView*>(wrapper->widget.get())) {
        stateView->setStyleSheet(effectiveSheet, node);
        return;
    }
    if (auto* tabs = dynamic_cast<oneui::Tabs*>(wrapper->widget.get())) {
        auto style = oneui::tabsStyleOverrideFromStyleSheet(*effectiveSheet, node);
        tabs->setStyleOverride(style);
        return;
    }
    if (auto* select = dynamic_cast<oneui::Select*>(wrapper->widget.get())) {
        select->setStyleOverride(
            oneui::selectStyleOverrideFromStyleSheet(*effectiveSheet, node));
        return;
    }
    if (auto* slider = dynamic_cast<oneui::Slider*>(wrapper->widget.get())) {
        const auto stateOverride = [](const oneui::StyleBox& box) {
            oneui::SliderStateStyleOverride state;
            state.trackBackground = box.background.color;
            state.trackFill = box.foreground;
            state.thumbBackground = box.foreground;
            state.thumbBorder = box.borderColor;
            state.thumbBorderWidth = box.borderWidth;
            if (box.outlineColor || box.outlineWidth || box.outlineOffset) {
                oneui::FocusRingStyleOverride ring;
                ring.color = box.outlineColor;
                ring.width = box.outlineWidth;
                ring.offset = box.outlineOffset;
                state.focusRing = ring;
            }
            return state;
        };
        oneui::SliderStyleOverride style;
        style.normal = stateOverride(effectiveSheet->resolve({node.tag, node.classes, oneui::StyleStateNone}));
        style.hovered = stateOverride(effectiveSheet->resolve({node.tag, node.classes, oneui::StyleStateHover}));
        style.pressed = stateOverride(effectiveSheet->resolve({node.tag, node.classes, oneui::StyleStateActive}));
        style.disabled = stateOverride(effectiveSheet->resolve({node.tag, node.classes, oneui::StyleStateDisabled}));
        style.focusVisible = stateOverride(effectiveSheet->resolve({node.tag, node.classes, oneui::StyleStateFocus}));
        slider->setStyleOverride(style);
        return;
    }
    if (auto* progress = dynamic_cast<oneui::ProgressBar*>(wrapper->widget.get())) {
        progress->setStyleOverride(
            oneui::progressBarStyleOverrideFromStyleSheet(*effectiveSheet, node));
        return;
    }
    if (auto* sparkline = dynamic_cast<oneui::Sparkline*>(wrapper->widget.get())) {
        sparkline->setStyleBox(effectiveSheet->resolve(node));
        return;
    }
    if (auto* virtualList = dynamic_cast<oneui::VirtualList*>(wrapper->widget.get())) {
        virtualList->setStyleOverride(oneui::listStyleOverrideFromStyleSheet(*effectiveSheet, node));
        return;
    }
    if (auto* list = dynamic_cast<oneui::List*>(wrapper->widget.get())) {
        list->setStyleOverride(oneui::listStyleOverrideFromStyleSheet(*effectiveSheet, node));
        return;
    }
    if (auto* table = dynamic_cast<oneui::Table*>(wrapper->widget.get())) {
        table->setStyleOverride(oneui::tableStyleOverrideFromStyleSheet(*effectiveSheet, node));
        return;
    }
    if (auto* tree = dynamic_cast<oneui::TreeView*>(wrapper->widget.get())) {
        tree->setStyleOverride(oneui::treeViewStyleOverrideFromStyleSheet(*effectiveSheet, node));
        return;
    }
    if (auto* textField = dynamic_cast<oneui::TextField*>(wrapper->widget.get())) {
        textField->setStyleOverride(oneui::textFieldStyleOverrideFromStyleSheet(*effectiveSheet, node));
        return;
    }
    if (auto* terminal = dynamic_cast<oneui::TerminalView*>(wrapper->widget.get())) {
        terminal->setStyleBox(effectiveSheet->resolve(node));
        return;
    }
    if (auto* scrollView = dynamic_cast<oneui::ScrollView*>(wrapper->widget.get())) {
        const oneui::StyleBox box = effectiveSheet->resolve(node);
        scrollView->setStyleBox(box);
        if (box.foreground || box.borderWidth) {
            scrollView->setScrollbarStyle(
                box.foreground.value_or(oneui::Color{148, 163, 184, 180}),
                box.borderWidth.value_or(4.0f));
        }
        return;
    }
    if (auto* grid = dynamic_cast<oneui::ReorderableGrid*>(wrapper->widget.get())) {
        grid->setStyleBox(effectiveSheet->resolve(node));
        return;
    }
    if (auto* surface = dynamic_cast<oneui::InteractiveSurface*>(wrapper->widget.get())) {
        const oneui::StyleBox box = effectiveSheet->resolve(node);
        surface->setStyle(oneui::interactiveSurfaceStyleFromStyleSheet(*effectiveSheet, node));
        if (box.padding) {
            surface->setPadding(*box.padding);
        }
        return;
    }
    if (auto* panel = dynamic_cast<oneui::Panel*>(wrapper->widget.get())) {
        const oneui::StyleBox box = effectiveSheet->resolve(node);
        if (box.padding) {
            panel->setPadding(*box.padding);
        }
        panel->setStyleBox(box);
        return;
    }
    if (auto* label = dynamic_cast<oneui::Label*>(wrapper->widget.get())) {
        const oneui::StyleBox box = effectiveSheet->resolve(node);
        if (box.foreground) {
            label->setColor(*box.foreground);
        }
        if (box.fontSize) {
            label->setFontSize(*box.fontSize);
        }
        if (box.fontWeight) {
            label->setFontWeight(*box.fontWeight);
        }
        label->setLetterSpacing(box.letterSpacing.value_or(0.0f));
        label->setTabularNumbers(box.tabularNumbers.value_or(false));
        return;
    }
    if (auto* icon = dynamic_cast<oneui::IconView*>(wrapper->widget.get())) {
        const oneui::StyleBox box = effectiveSheet->resolve(node);
        if (box.foreground) {
            icon->setColor(*box.foreground);
        }
        if (box.background.color) {
            icon->setAccent(*box.background.color);
        }
        if (box.borderWidth) {
            icon->setStrokeWidth(*box.borderWidth);
        }
        return;
    }
    if (auto* iconButton = dynamic_cast<oneui::IconButton*>(wrapper->widget.get())) {
        iconButton->setStyleSheet(effectiveSheet, node);
        return;
    }
    if (auto* switchWidget = dynamic_cast<oneui::Switch*>(wrapper->widget.get())) {
        switchWidget->setStyleOverride(oneui::switchStyleOverrideFromStyleSheet(*effectiveSheet, node));
        return;
    }
    if (auto* titleBar = dynamic_cast<oneui::WindowTitleBar*>(wrapper->widget.get())) {
        titleBar->setStyleSheet(effectiveSheet);
        return;
    }
    if (auto* navItem = dynamic_cast<oneui::NavItem*>(wrapper->widget.get())) {
        navItem->setStyleSheet(effectiveSheet);
        return;
    }
    if (auto* appShell = dynamic_cast<oneui::AppShell*>(wrapper->widget.get())) {
        const oneui::StyleBox box = effectiveSheet->resolve(node);
        if (box.padding) {
            appShell->setPadding(*box.padding);
        }
        if (box.gap) {
            appShell->setGap(*box.gap);
        }
        appShell->setStyleBox(box);
        return;
    }
    if (auto* topBar = dynamic_cast<oneui::TopBar*>(wrapper->widget.get())) {
        const oneui::StyleBox box = effectiveSheet->resolve(node);
        if (box.padding) {
            topBar->setPadding(*box.padding);
        }
        if (box.gap) {
            topBar->setGap(*box.gap);
        }
        topBar->setStyleBox(box);
        return;
    }
    if (auto* overlayHost = dynamic_cast<oneui::OverlayHost*>(wrapper->widget.get())) {
        (void)overlayHost;
        return;
    }
    if (auto* stack = dynamic_cast<oneui::Stack*>(wrapper->widget.get())) {
        const oneui::StyleBox box = effectiveSheet->resolve(node);
        if (box.padding) {
            stack->setPadding(*box.padding);
        }
        if (box.gap) {
            stack->setGap(*box.gap);
        }
        stack->setStyleBox(box);
    }
}

} // namespace oneui::capi
