#include "oneui_c_api_controls_internal.h"
#include "oneui_c_api_error.h"
#include "oneui/controls/window_title_bar.h"
#include "oneui/layout/app_shell.h"
#include "oneui/layout/product_shell.h"
#include "oneui/layout/top_bar.h"
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

OneUiWidget* oneui_top_bar_create(void) {
    auto* wrapper = wrap(std::make_shared<oneui::TopBar>());
    if (wrapper) {
        wrapper->tag = "topbar";
    }
    return wrapper;
}

void oneui_top_bar_set_leading(OneUiWidget* top_bar, OneUiWidget* child) {
    auto* nativeTopBar = asWidget<oneui::TopBar>(top_bar);
    if (!nativeTopBar) {
        return;
    }
    nativeTopBar->setLeading(child ? child->widget : nullptr);
}

void oneui_top_bar_add_action(OneUiWidget* top_bar, OneUiWidget* child) {
    auto* nativeTopBar = asWidget<oneui::TopBar>(top_bar);
    if (!nativeTopBar || !child || !child->widget) {
        return;
    }
    nativeTopBar->addAction(child->widget);
}

void oneui_top_bar_clear_actions(OneUiWidget* top_bar) {
    if (auto* nativeTopBar = asWidget<oneui::TopBar>(top_bar)) {
        nativeTopBar->clearActions();
    }
}

void oneui_top_bar_set_gap(OneUiWidget* top_bar, float gap) {
    if (auto* nativeTopBar = asWidget<oneui::TopBar>(top_bar)) {
        nativeTopBar->setGap(gap);
    }
}

void oneui_top_bar_set_padding(OneUiWidget* top_bar, OneUiInsets insets) {
    if (auto* nativeTopBar = asWidget<oneui::TopBar>(top_bar)) {
        nativeTopBar->setPadding(toNativeInsets(insets));
    }
}

void oneui_top_bar_set_leading_width(OneUiWidget* top_bar, float width) {
    if (auto* nativeTopBar = asWidget<oneui::TopBar>(top_bar)) {
        nativeTopBar->setLeadingWidth(width);
    }
}

OneUiWidget* oneui_app_shell_create(void) {
    return wrap(std::make_shared<oneui::AppShell>());
}

void oneui_app_shell_set_sidebar(OneUiWidget* shell, OneUiWidget* child) {
    auto* nativeShell = asWidget<oneui::AppShell>(shell);
    if (!nativeShell) {
        return;
    }
    nativeShell->setSidebar(child ? child->widget : nullptr);
}

void oneui_app_shell_set_header(OneUiWidget* shell, OneUiWidget* child) {
    auto* nativeShell = asWidget<oneui::AppShell>(shell);
    if (!nativeShell) {
        return;
    }
    nativeShell->setHeader(child ? child->widget : nullptr);
}

void oneui_app_shell_set_content(OneUiWidget* shell, OneUiWidget* child) {
    auto* nativeShell = asWidget<oneui::AppShell>(shell);
    if (!nativeShell) {
        return;
    }
    nativeShell->setContent(child ? child->widget : nullptr);
}

void oneui_app_shell_set_footer(OneUiWidget* shell, OneUiWidget* child) {
    auto* nativeShell = asWidget<oneui::AppShell>(shell);
    if (!nativeShell) {
        return;
    }
    nativeShell->setFooter(child ? child->widget : nullptr);
}

void oneui_app_shell_set_sidebar_width(OneUiWidget* shell, float width) {
    if (auto* nativeShell = asWidget<oneui::AppShell>(shell)) {
        nativeShell->setSidebarWidth(width);
    }
}

void oneui_app_shell_set_header_height(OneUiWidget* shell, float height) {
    if (auto* nativeShell = asWidget<oneui::AppShell>(shell)) {
        nativeShell->setHeaderHeight(height);
    }
}

void oneui_app_shell_set_footer_height(OneUiWidget* shell, float height) {
    if (auto* nativeShell = asWidget<oneui::AppShell>(shell)) {
        nativeShell->setFooterHeight(height);
    }
}

void oneui_app_shell_set_footer_span_sidebar(OneUiWidget* shell, int span) {
    if (auto* nativeShell = asWidget<oneui::AppShell>(shell)) {
        nativeShell->setFooterSpanSidebar(span != 0);
    }
}

void oneui_app_shell_set_gap(OneUiWidget* shell, float gap) {
    if (auto* nativeShell = asWidget<oneui::AppShell>(shell)) {
        nativeShell->setGap(gap);
    }
}

void oneui_app_shell_set_padding(OneUiWidget* shell, OneUiInsets insets) {
    if (auto* nativeShell = asWidget<oneui::AppShell>(shell)) {
        nativeShell->setPadding(toNativeInsets(insets));
    }
}

void oneui_app_shell_set_sidebar_visible(OneUiWidget* shell, int visible) {
    if (auto* nativeShell = asWidget<oneui::AppShell>(shell)) {
        nativeShell->setSidebarVisible(visible != 0);
    }
}

OneUiWidget* oneui_product_shell_create(void) {
    auto* wrapper = wrap(std::make_shared<oneui::AppShell>());
    if (wrapper) {
        wrapper->tag = "product-shell";
    }
    return wrapper;
}

void oneui_product_shell_set_sidebar(OneUiWidget* shell, OneUiWidget* child) {
    oneui_app_shell_set_sidebar(shell, child);
}

void oneui_product_shell_set_topbar(OneUiWidget* shell, OneUiWidget* child) {
    oneui_app_shell_set_header(shell, child);
}

void oneui_product_shell_set_content(OneUiWidget* shell, OneUiWidget* child) {
    oneui_app_shell_set_content(shell, child);
}

void oneui_product_shell_set_status(OneUiWidget* shell, OneUiWidget* child) {
    oneui_app_shell_set_footer(shell, child);
}

void oneui_product_shell_set_sidebar_width(OneUiWidget* shell, float width) {
    oneui_app_shell_set_sidebar_width(shell, width);
}

void oneui_product_shell_set_topbar_height(OneUiWidget* shell, float height) {
    oneui_app_shell_set_header_height(shell, height);
}

void oneui_product_shell_set_status_height(OneUiWidget* shell, float height) {
    oneui_app_shell_set_footer_height(shell, height);
}

void oneui_product_shell_set_gap(OneUiWidget* shell, float gap) {
    oneui_app_shell_set_gap(shell, gap);
}

void oneui_product_shell_set_padding(OneUiWidget* shell, OneUiInsets insets) {
    oneui_app_shell_set_padding(shell, insets);
}

void oneui_product_shell_set_sidebar_visible(OneUiWidget* shell, int visible) {
    oneui_app_shell_set_sidebar_visible(shell, visible);
}

OneUiWidget* oneui_title_bar_create(const wchar_t* title) {
    return wrap(std::make_shared<oneui::WindowTitleBar>(wideOrEmpty(title)));
}

void oneui_title_bar_set_title(OneUiWidget* title_bar, const wchar_t* title) {
    auto* nativeTitleBar = asWidget<oneui::WindowTitleBar>(title_bar);
    if (!nativeTitleBar) {
        return;
    }
    nativeTitleBar->setTitle(wideOrEmpty(title));
}

void oneui_title_bar_set_icon_symbol(OneUiWidget* title_bar, int symbol) {
    auto* nativeTitleBar = asWidget<oneui::WindowTitleBar>(title_bar);
    if (!nativeTitleBar) {
        return;
    }
    const auto clamped = clampIconSymbol(symbol);
    nativeTitleBar->setIconSymbol(static_cast<oneui::IconSymbol>(clamped));
}

void oneui_title_bar_set_maximized(OneUiWidget* title_bar, int maximized) {
    auto* nativeTitleBar = asWidget<oneui::WindowTitleBar>(title_bar);
    if (!nativeTitleBar) {
        return;
    }
    nativeTitleBar->setMaximized(maximized != 0);
}

void oneui_title_bar_set_variant(OneUiWidget* title_bar, const char* variant) {
    auto* nativeTitleBar = asWidget<oneui::WindowTitleBar>(title_bar);
    if (!nativeTitleBar) {
        return;
    }
    nativeTitleBar->setVariant(variant ? std::string(variant) : std::string());
}

int oneui_title_bar_set_presentation(OneUiWidget* title_bar, const OneUiTitleBarPresentation* input) {
    auto* bar = asWidget<oneui::WindowTitleBar>(title_bar);
    if (!bar) return 0;
    if (!input) return bar->setPresentation(nullptr) ? 1 : 0;
    if (input->struct_size != sizeof(OneUiTitleBarPresentation)) return 0;
    const auto rect = [](OneUiRect r) { return oneui::Rect{r.x,r.y,r.width,r.height}; };
    const auto color = [](OneUiColor c) { return oneui::Color{c.r,c.g,c.b,c.a}; };
    const auto flag = [](int value) { return value == 0 || value == 1; };
    const auto path = [](const OneUiCaptionPath& source, oneui::CanvasPath& target) {
        if (source.count > 64 || (source.count && !source.commands)) return false;
        for (size_t i = 0; i < source.count; ++i) {
            const auto& c = source.commands[i];
            if (!std::isfinite(c.x) || !std::isfinite(c.y)) return false;
            if (c.verb == 0) target.moveTo({c.x,c.y});
            else if (c.verb == 1) target.lineTo({c.x,c.y});
            else if (c.verb == 2) target.close();
            else return false;
        }
        return true;
    };
    try {
        oneui::TitleBarPresentation p;
        p.logo=rect(input->logo); p.logoIcon=rect(input->logo_icon); p.title=rect(input->title);
        p.leading=rect(input->leading); p.accessory=rect(input->accessory);
        p.logoStrokeWidth=input->logo_stroke_width; p.titleFontSize=input->title_font_size;
        if (!flag(input->hide_logo_border)) return 0;
        p.hideLogoBorder=input->hide_logo_border != 0;
        for (size_t i=0; i<3; ++i) {
            const auto& source=input->buttons[i]; auto& target=p.buttons[i];
            if (!flag(source.ellipse) || !flag(source.glyph_on_group_hover) || !flag(source.glyph_uses_foreground)) return 0;
            target.frame=rect(source.frame); target.visual=rect(source.visual); target.icon=rect(source.icon);
            if (!path(source.glyph,target.glyph) || !path(source.maximized_glyph,target.maximizedGlyph)) return 0;
            target.strokeWidth=source.stroke_width; target.cornerRadius=source.corner_radius;
            target.ellipse=source.ellipse != 0; target.glyphOnGroupHover=source.glyph_on_group_hover != 0;
            target.glyphUsesForeground=source.glyph_uses_foreground != 0;
            target.fill=color(source.fill); target.pressedFill=color(source.pressed_fill); target.glyphColor=color(source.glyph_color);
        }
        return bar->setPresentation(&p) ? 1 : 0;
    } catch (...) { oneui::capi::recordCurrentException(__func__); return 0; }
}

void oneui_title_bar_set_leading(OneUiWidget* title_bar, OneUiWidget* leading) {
    auto* nativeTitleBar = asWidget<oneui::WindowTitleBar>(title_bar);
    if (!nativeTitleBar) {
        return;
    }
    nativeTitleBar->setLeading(leading ? leading->widget : nullptr);
}

void oneui_title_bar_set_accessory(OneUiWidget* title_bar, OneUiWidget* accessory) {
    auto* nativeTitleBar = asWidget<oneui::WindowTitleBar>(title_bar);
    if (!nativeTitleBar) {
        return;
    }
    nativeTitleBar->setAccessory(accessory ? accessory->widget : nullptr);
}

void oneui_title_bar_set_on_minimize(OneUiWidget* title_bar, OneUiVoidCallback callback, void* user_data) {
    auto* nativeTitleBar = asWidget<oneui::WindowTitleBar>(title_bar);
    if (!nativeTitleBar) {
        return;
    }
    if (!callback) {
        nativeTitleBar->setOnMinimize(nullptr);
        return;
    }
    nativeTitleBar->setOnMinimize([callback, user_data] {
        callback(user_data);
    });
}

void oneui_title_bar_set_on_maximize(OneUiWidget* title_bar, OneUiVoidCallback callback, void* user_data) {
    auto* nativeTitleBar = asWidget<oneui::WindowTitleBar>(title_bar);
    if (!nativeTitleBar) {
        return;
    }
    if (!callback) {
        nativeTitleBar->setOnMaximize(nullptr);
        return;
    }
    nativeTitleBar->setOnMaximize([callback, user_data] {
        callback(user_data);
    });
}

void oneui_title_bar_set_on_close(OneUiWidget* title_bar, OneUiVoidCallback callback, void* user_data) {
    auto* nativeTitleBar = asWidget<oneui::WindowTitleBar>(title_bar);
    if (!nativeTitleBar) {
        return;
    }
    if (!callback) {
        nativeTitleBar->setOnClose(nullptr);
        return;
    }
    nativeTitleBar->setOnClose([callback, user_data] {
        callback(user_data);
    });
}
