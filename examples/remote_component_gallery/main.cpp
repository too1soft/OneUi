#include "oneui/oneui_c_api.h"

#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {

constexpr int StackColumn = 0;
constexpr int StackRow = 1;
constexpr int AlignStart = 0;
constexpr int AlignCenter = 1;
constexpr int AlignStretch = 3;

constexpr int IconBrandBloom = 0;
constexpr int IconRemoteAssist = 2;
constexpr int IconDevice = 4;
constexpr int IconToolbox = 5;
constexpr int IconCompass = 6;
constexpr int IconSettings = 7;
constexpr int IconBell = 8;
constexpr int IconKeyDots = 20;
constexpr int IconFolder = 47;
constexpr int IconOpenInNew = 49;

OneUiUtf8String utf8(const char* value) {
    return OneUiUtf8String{value, std::strlen(value)};
}

const char* kCss = R"CSS(
:root {
    --bg: #111114;
    --panel: #1f1f24;
    --panel-raised: #222228;
    --border: #373841;
    --text: #f8f8fc;
    --muted: #8e8f98;
    --accent: #ff3972;
    --blue: #2e69e7;
    --blue-hover: #3a7af8;
    --blue-active: #265bcb;
    --focus: #3e7effb4;
    --selection: #2e69e766;
}

.app-root {
    background: var(--bg);
}

.titlebar {
    background: var(--bg);
    border-color: #2c2c34;
    border-width: 1px;
    padding: 3px 8px 3px 12px;
}

.titlebar-icon {
    color: #111114;
    background: #7bd4c6;
    border-width: 1.6px;
}

.titlebar-title {
    color: #f0f1f6;
    font-size: 12px;
}

.window-button {
    background: #111114;
    border-color: #111114;
    border-width: 1px;
    border-radius: 3px;
    color: #c9cbd3;
}

.window-button:hover {
    background: #2a2a31;
    border-color: #383842;
    color: #ffffff;
}

.window-button:active {
    background: #202027;
    border-color: #30303a;
}

.window-button.close:hover {
    background: #e5484d;
    border-color: #e5484d;
    color: #ffffff;
}

.sidebar {
    background: var(--panel);
    border-color: #2e2e34;
    border-width: 1px;
    padding: 18px 12px 18px 12px;
}

.content {
    background: var(--bg);
    padding: 12px 42px 10px 58px;
}

.brand-logo {
    background: #30313a;
    border-color: #484954;
    border-width: 1px;
    border-radius: 9px;
    width: 34px;
    height: 34px;
}

.brand-mark-icon {
    color: #ffffff;
    background: #ff3972;
    border-width: 1.7px;
}

.brand-name {
    color: var(--text);
    font-size: 15px;
    font-weight: 600;
}

.brand-subtitle,
.muted-label,
.field-label,
.readonly-label {
    color: var(--muted);
    font-size: 12px;
}

.nav-item {
    background: var(--panel);
    border-color: var(--panel);
    border-width: 1px;
    border-radius: 5px;
    padding: 0px 12px 0px 12px;
    color: #dadbe1;
}

.nav-item:hover {
    background: #2b2b32;
    border-color: #41414a;
    color: #f1f2f6;
}

.nav-item:active {
    background: #33333b;
    border-color: #494952;
    color: #ffffff;
}

.nav-item.selected {
    background: #3b3843;
    border-color: #484451;
    color: var(--accent);
}

.sidebar-divider {
    background: #313137;
}

.title {
    color: var(--text);
    font-size: 24px;
    font-weight: 700;
}

.section-heading {
    color: var(--text);
    font-size: 17px;
    font-weight: 650;
}

.device-code {
    color: #f3f4f8;
    font-size: 34px;
    font-weight: 500;
}

.readonly-strip,
.text-field,
.search-field {
    background: #222127;
    border-color: #3a3942;
    border-width: 1px;
    border-radius: 6px;
    color: #eeeff5;
    placeholder-color: #8b8c96;
    caret-color: #ffffff;
    selection-color: var(--selection);
    padding: 0px 14px 0px 14px;
    box-shadow: inset 0px 1px 0px #ffffff0a, 0px 1px 2px #00000035;
    height: 42px;
}

.search-field {
    width: 472px;
    height: 32px;
}

.compact-field {
    width: 172px;
}

.text-field:hover,
.search-field:hover {
    background: #232229;
    border-color: #464652;
}

.text-field:focus,
.search-field:focus {
    background: #1f1e24;
    border-color: #437bff;
    outline-color: var(--focus);
    outline-width: 1px;
    outline-offset: 1px;
    box-shadow: inset 0px 1px 0px #ffffff0d, 0px 0px 0px #00000000;
}

.text-field:read-only,
.search-field:read-only {
    background: #222127;
    border-color: #3a3942;
    color: #e6e7ee;
}

.primary {
    background: var(--blue);
    border-color: var(--blue);
    border-width: 1px;
    border-radius: 6px;
    color: #ffffff;
    box-shadow: 0px 8px 18px 0px #123f9b45;
    width: 190px;
    height: 42px;
}

.primary:hover {
    background: var(--blue-hover);
    border-color: var(--blue-hover);
    box-shadow: 0px 10px 22px 0px #164bbd55;
}

.primary:active {
    background: var(--blue-active);
    border-color: var(--blue-active);
    box-shadow: inset 0px 1px 2px #00000045;
}

.top-pill,
.device-pill,
.ghost {
    background: #222228;
    border-color: #373841;
    border-width: 1px;
    border-radius: 6px;
    color: #e7e8ee;
}

.top-pill {
    height: 32px;
}

.device-pill {
    width: 108px;
    height: 30px;
}

.top-pill:hover,
.device-pill:hover,
.ghost:hover {
    background: #2b2b32;
    border-color: #464752;
}

.top-pill.selected {
    background: #1f335d;
    border-color: #326df4;
    color: #aecaed;
}

.local-switch {
    background: #41424a;
    content-background: #f2f4f8;
    border-color: #4a4b54;
    border-width: 1px;
    border-radius: 12px;
    outline-color: #6aa1ff;
    outline-width: 2px;
    outline-offset: 3px;
    width: 54px;
    height: 28px;
}

.local-switch:checked {
    background: #3d82ff;
    content-background: #ffffff;
    border-color: #5b98ff;
}

.upgrade {
    color: #ff9f38;
    width: 176px;
}

.segmented {
    background: #222228;
    content-background: #00000000;
    border-color: #373841;
    border-width: 1px;
    border-radius: 7px;
    content-radius: 6px;
    color: #d7d9e1;
    padding: 2px;
    outline-color: #3e7effb4;
    outline-width: 1px;
    outline-offset: 1px;
    width: 124px;
    height: 32px;
}

.segmented:selected {
    background: #222228;
    content-background: #1f335d;
    border-color: #326df4;
    color: #bcd2ff;
}

.mode-radio {
    background: #00000000;
    border-color: #6d7180;
    border-width: 1px;
    border-radius: 12px;
    color: #9697a0;
    content-background: #00000000;
    content-radius: 6px;
    width: 250px;
    height: 26px;
}

.mode-radio:selected {
    background: #00000000;
    color: #2e83ff;
    border-color: #2e83ff;
    content-background: #2e83ff;
}

.recent-list {
    background: #1b1b21;
    border-color: #3b3b46;
    border-width: 1px;
    border-radius: 7px;
    color: #f3f3f7;
    content-background: #282831;
    padding: 2px 4px 2px 4px;
    box-shadow: 0px 8px 18px 0px #00000050;
    height: 88px;
}

.recent-list:hover {
    content-background: #30303a;
}

.recent-list:selected {
    content-background: #34305b;
}

.status-card {
    background: #1f1f24;
    border-color: #383840;
    border-width: 1px;
    border-radius: 7px;
    padding: 12px 18px 12px 18px;
    font-weight: 600;
    height: 62px;
}

.status-strip-action {
    background: #00000000;
    border-color: #343742;
    border-width: 1px;
    border-radius: 6px;
    color: #d9dce6;
    font-size: 12px;
    font-weight: 500;
}

.status-strip-action:hover {
    background: #282a32;
    border-color: #454855;
    color: #ffffff;
}

.footer {
    color: #a0a1aa;
    font-size: 12px;
}
)CSS";

struct AppContext {
    OneUiWindow* window = nullptr;
};

std::vector<OneUiWidget*> gWidgets;

OneUiInsets insets(float top, float right, float bottom, float left) {
    return OneUiInsets{top, right, bottom, left};
}

void keep(OneUiWidget* widget) {
    if (widget) {
        gWidgets.push_back(widget);
    }
}

void setNode(OneUiWidget* widget, const char* tag, const char* classes) {
    oneui_widget_set_style_node(widget, tag, classes);
}

void size(OneUiWidget* widget, float width, float height) {
    oneui_widget_set_preferred_size(widget, width, height);
}

OneUiWidget* stack(int direction, const char* classes, float gap, int align = AlignStretch) {
    OneUiWidget* widget = oneui_stack_create(direction == StackRow ? OneUiStackDirectionRow : OneUiStackDirectionColumn);
    oneui_stack_set_gap(widget, gap);
    oneui_stack_set_align(widget, static_cast<OneUiStackAlign>(align));
    setNode(widget, "stack", classes);
    keep(widget);
    return widget;
}

OneUiWidget* label(const wchar_t* text, const char* classes, float width = 0.0f, float height = 0.0f) {
    OneUiWidget* widget = oneui_label_create(text);
    setNode(widget, "label", classes);
    size(widget, width, height);
    keep(widget);
    return widget;
}

OneUiWidget* panel(const char* classes, float width = 0.0f, float height = 0.0f) {
    OneUiWidget* widget = oneui_panel_create();
    setNode(widget, "section", classes);
    size(widget, width, height);
    keep(widget);
    return widget;
}

OneUiWidget* textField(const wchar_t* placeholder, const char* classes, float width = 0.0f, float height = 0.0f) {
    OneUiWidget* widget = oneui_text_field_create(placeholder);
    setNode(widget, "input", classes);
    size(widget, width, height);
    keep(widget);
    return widget;
}

OneUiWidget* button(const wchar_t* text, const char* classes, int variant = 0) {
    OneUiWidget* widget = oneui_button_create(text);
    oneui_button_set_variant(widget, variant == 0 ? OneUiButtonVariantPrimary : OneUiButtonVariantSecondary);
    setNode(widget, "button", classes);
    keep(widget);
    return widget;
}

OneUiWidget* navItem(const wchar_t* text, int symbol, int selected) {
    OneUiWidget* widget = oneui_nav_item_create(text, symbol, selected);
    setNode(widget, "button", selected ? "nav-item selected" : "nav-item");
    size(widget, 0.0f, 38.0f);
    keep(widget);
    return widget;
}

void onMinimize(void* userData) {
    auto* ctx = static_cast<AppContext*>(userData);
    if (ctx && ctx->window) {
        oneui_window_minimize(ctx->window);
    }
}

void onMaximize(void* userData) {
    auto* ctx = static_cast<AppContext*>(userData);
    if (ctx && ctx->window) {
        oneui_window_toggle_maximize(ctx->window);
    }
}

void onClose(void* userData) {
    auto* ctx = static_cast<AppContext*>(userData);
    if (ctx && ctx->window) {
        oneui_window_request_close(ctx->window);
    }
}

OneUiWidget* buildSidebar() {
    OneUiWidget* sidebar = stack(StackColumn, "sidebar", 12.0f, AlignStretch);
    oneui_stack_set_padding(sidebar, insets(18.0f, 12.0f, 18.0f, 12.0f));

    OneUiWidget* brand = stack(StackRow, "brand-row", 10.0f, AlignCenter);
    size(brand, 0.0f, 44.0f);
    OneUiWidget* logo = panel("brand-logo", 34.0f, 34.0f);
    OneUiWidget* mark = oneui_icon_create(IconBrandBloom);
    setNode(mark, "icon", "brand-mark-icon");
    keep(mark);
    oneui_panel_set_content(logo, mark);
    OneUiWidget* member = stack(StackColumn, "brand-member", 2.0f, AlignStretch);
    size(member, 0.0f, 40.0f);
    oneui_stack_add(member, label(L"-", "brand-name", 0.0f, 18.0f));
    oneui_stack_add(member, label(L"Member  Renew", "brand-subtitle", 0.0f, 18.0f));
    oneui_stack_add(brand, logo);
    oneui_stack_add(brand, member);
    oneui_stack_add(sidebar, brand);

    oneui_stack_add(sidebar, navItem(L"Remote Assist", IconRemoteAssist, 1));
    oneui_stack_add(sidebar, navItem(L"Devices", IconDevice, 0));
    oneui_stack_add(sidebar, navItem(L"Toolbox", IconToolbox, 0));
    oneui_stack_add(sidebar, navItem(L"Discover", IconCompass, 0));
    oneui_stack_add(sidebar, panel("sidebar-divider", 0.0f, 1.0f));
    oneui_stack_add(sidebar, label(L"", "spacer", 0.0f, 0.0f));
    oneui_stack_add(sidebar, navItem(L"Settings", IconSettings, 0));
    return sidebar;
}

OneUiWidget* buildTopBar() {
    OneUiWidget* topbar = oneui_top_bar_create();
    setNode(topbar, "topbar", "topbar");
    size(topbar, 0.0f, 38.0f);
    oneui_top_bar_set_leading_width(topbar, 472.0f);
    oneui_top_bar_set_gap(topbar, 10.0f);
    oneui_top_bar_set_padding(topbar, insets(0.0f, 0.0f, 0.0f, 0.0f));
    keep(topbar);

    OneUiWidget* search = oneui_search_box_create(L"Search or connect device");
    oneui_text_field_set_text(search, L"http://192.168.1.46:8080");
    oneui_text_field_set_read_only(search, 1);
    setNode(search, "search", "search-field");
    keep(search);
    oneui_top_bar_set_leading(topbar, search);

    OneUiWidget* upgrade = oneui_badge_create(L"MCP upgrade", 2);
    setNode(upgrade, "badge", "top-pill upgrade");
    keep(upgrade);
    oneui_top_bar_add_action(topbar, upgrade);

    OneUiWidget* segmented = oneui_segmented_control_create();
    oneui_segmented_control_set_items(segmented, L"Classic|New");
    oneui_segmented_control_set_selected_index(segmented, 1);
    setNode(segmented, "segmented-control", "segmented");
    keep(segmented);
    oneui_top_bar_add_action(topbar, segmented);

    OneUiWidget* notify = oneui_icon_button_create(IconBell);
    setNode(notify, "button", "icon-button");
    keep(notify);
    oneui_top_bar_add_action(topbar, notify);
    return topbar;
}

OneUiWidget* buildContent() {
    OneUiWidget* content = stack(StackColumn, "content", 8.0f, AlignStretch);
    oneui_stack_set_padding(content, insets(26.0f, 42.0f, 10.0f, 58.0f));

    OneUiWidget* titleRow = stack(StackRow, "section-title-row", 14.0f, AlignCenter);
    size(titleRow, 0.0f, 30.0f);
    oneui_stack_add(titleRow, label(L"Assist this computer", "title", 0.0f, 30.0f));
    OneUiWidget* sw = oneui_switch_create(L"");
    oneui_switch_set_checked(sw, 1);
    setNode(sw, "switch", "local-switch");
    size(sw, 54.0f, 28.0f);
    keep(sw);
    oneui_stack_add(titleRow, sw);
    oneui_stack_add(titleRow, button(L"Local device", "device-pill", 1));
    oneui_stack_add(content, titleRow);

    oneui_stack_add(content, label(L"Local ID", "muted-label", 0.0f, 20.0f));
    oneui_stack_add(content, label(L"155 336 7617", "device-code", 0.0f, 44.0f));

    OneUiWidget* readOnly = panel("readonly-strip", 0.0f, 30.0f);
    OneUiWidget* readRow = stack(StackRow, "readonly-row", 12.0f, AlignCenter);
    oneui_stack_add(readRow, label(L"Permanent code", "readonly-label", 120.0f, 20.0f));
    oneui_stack_add(readRow, label(L"-----", "readonly-code", 120.0f, 24.0f));
    oneui_stack_add(readRow, label(L"", "spacer", 0.0f, 20.0f));
    oneui_stack_add(readRow, label(L"For unattended access", "readonly-label", 200.0f, 20.0f));
    oneui_panel_set_content(readOnly, readRow);
    oneui_stack_add(content, readOnly);

    oneui_stack_add(content, label(L"Temporary code", "field-label", 0.0f, 18.0f));
    OneUiWidget* tempRow = stack(StackRow, "form-row", 12.0f, AlignStretch);
    size(tempRow, 0.0f, 46.0f);
    oneui_stack_add(tempRow, textField(L"Generate temporary code", "text-field", 0.0f, 42.0f));
    oneui_stack_add(tempRow, button(L"Generate", "primary"));
    oneui_stack_add(content, tempRow);

    oneui_stack_add(content, label(L"Assist others", "section-heading", 0.0f, 30.0f));
    OneUiWidget* connectRow = stack(StackRow, "form-row", 12.0f, AlignStretch);
    size(connectRow, 0.0f, 46.0f);
    OneUiWidget* partner = textField(L"Enter partner ID", "text-field", 0.0f, 42.0f);
    oneui_text_field_set_prefix_icon(partner, IconDevice);
    OneUiWidget* verify = textField(L"Code (optional)", "text-field compact-field", 172.0f, 42.0f);
    oneui_text_field_set_prefix_icon(verify, IconKeyDots);
    oneui_stack_add(connectRow, partner);
    oneui_stack_add(connectRow, verify);
    oneui_stack_add(connectRow, button(L"Connect", "primary"));
    oneui_stack_add(content, connectRow);

    OneUiWidget* radio = oneui_radio_group_create();
    oneui_radio_group_set_items(radio, L"Remote desktop|Remote files");
    oneui_radio_group_set_selected_index(radio, 0);
    oneui_radio_group_set_orientation(radio, 1);
    setNode(radio, "radio", "mode-radio");
    size(radio, 250.0f, 26.0f);
    keep(radio);
    oneui_stack_add(content, radio);

    oneui_stack_add(content, label(L"Recent connections", "field-label", 0.0f, 26.0f));
    OneUiWidget* recents = oneui_virtual_list_create();
    const OneUiRichListItemUtf8 recentItems[] = {
        {utf8("Office gateway"), utf8("169 510 1007"), utf8("RDP"), utf8("Online"),
         {34, 197, 94, 255}, {74, 222, 128, 255}, 1},
        {utf8("Build workstation"), utf8("164 709 2397"), utf8("SSH"), utf8("Busy"),
         {245, 158, 11, 255}, {251, 191, 36, 255}, 1},
    };
    oneui_virtual_list_set_rich_items_utf8(recents, recentItems, 2);
    oneui_virtual_list_set_row_height(recents, 44.0f);
    setNode(recents, "virtual-list", "recent-list");
    size(recents, 0.0f, 88.0f);
    keep(recents);
    oneui_stack_add(content, recents);

    OneUiWidget* status = oneui_status_strip_create(L"Status", L"OneUI component gallery is ready for visual review.");
    oneui_status_strip_set_primary_action(status, L"Copy");
    oneui_status_strip_set_primary_action_presentation(status, 1);
    oneui_status_strip_set_primary_action_trailing_icon(status, IconOpenInNew);
    oneui_status_strip_set_secondary_action(status, L"Details");
    setNode(status, "status-strip", "status-card");
    size(status, 0.0f, 62.0f);
    keep(status);
    oneui_stack_add(content, status);
    return content;
}

OneUiWidget* buildRoot(AppContext* ctx) {
    OneUiWidget* root = stack(StackColumn, "app-root", 0.0f, AlignStretch);

    OneUiWidget* titlebar = oneui_title_bar_create(L"OneUI Remote Component Gallery");
    oneui_title_bar_set_icon_symbol(titlebar, IconBrandBloom);
    OneUiWidget* workspace = oneui_icon_button_create(IconFolder);
    setNode(workspace, "button", "icon-button");
    size(workspace, 28.0f, 28.0f);
    keep(workspace);
    oneui_title_bar_set_leading(titlebar, workspace);
    oneui_title_bar_set_on_minimize(titlebar, onMinimize, ctx);
    oneui_title_bar_set_on_maximize(titlebar, onMaximize, ctx);
    oneui_title_bar_set_on_close(titlebar, onClose, ctx);
    setNode(titlebar, "titlebar", "titlebar");
    size(titlebar, 0.0f, 34.0f);
    keep(titlebar);
    oneui_stack_add(root, titlebar);

    OneUiWidget* shell = oneui_app_shell_create();
    setNode(shell, "app-shell", "product-shell");
    oneui_app_shell_set_sidebar_width(shell, 184.0f);
    oneui_app_shell_set_header_height(shell, 54.0f);
    oneui_app_shell_set_footer_height(shell, 28.0f);
    oneui_app_shell_set_footer_span_sidebar(shell, 1);
    oneui_app_shell_set_gap(shell, 0.0f);
    keep(shell);

    oneui_app_shell_set_sidebar(shell, buildSidebar());
    oneui_app_shell_set_header(shell, buildTopBar());
    oneui_app_shell_set_content(shell, buildContent());
    oneui_app_shell_set_footer(shell, label(L"OneUI C ABI gallery / Win7+ target / no WebView", "footer", 0.0f, 22.0f));

    oneui_stack_add(root, shell);
    return root;
}

} // namespace

int main() {
    OneUiStyleSheet* sheet = oneui_style_sheet_create();
    char error[512] = {};
    if (!oneui_style_sheet_add_css(sheet, kCss, error, static_cast<int>(sizeof(error)))) {
        std::fprintf(stderr, "OneUI CSS failed: %s\n", error);
        return EXIT_FAILURE;
    }

    const wchar_t title[] = L"OneUI Remote Component Gallery";
    OneUiWindowOptions options{};
    options.title = title;
    options.width = 980;
    options.height = 720;
    options.visible = 0;
    options.borderless = 1;
    options.fullscreen = 0;
    options.topmost = 0;
    options.resizable = 1;

    AppContext ctx{};
    ctx.window = oneui_window_create(&options);
    if (!ctx.window) {
        std::fprintf(stderr, "oneui_window_create failed\n");
        return EXIT_FAILURE;
    }

    oneui_window_set_style_sheet(ctx.window, sheet);
    OneUiWidget* root = buildRoot(&ctx);
    oneui_window_set_content(ctx.window, root);
    oneui_window_show(ctx.window);
    const int code = oneui_window_run(ctx.window);
    oneui_window_destroy(ctx.window);
    oneui_style_sheet_destroy(sheet);
    return code;
}
