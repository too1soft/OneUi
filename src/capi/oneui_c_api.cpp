#include "oneui/oneui_c_api.h"

#include "oneui/controls/badge.h"
#include "oneui/controls/button.h"
#include "oneui/controls/card.h"
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
#include "oneui/controls/radio_group.h"
#include "oneui/controls/realtime_frame_view.h"
#include "oneui/controls/remote_input_region.h"
#include "oneui/controls/select.h"
#include "oneui/controls/status_strip.h"
#include "oneui/controls/state_view.h"
#include "oneui/controls/switch.h"
#include "oneui/controls/table.h"
#include "oneui/controls/tabs.h"
#include "oneui/controls/text_field.h"
#include "oneui/controls/terminal_view.h"
#include "oneui/controls/tree_view.h"
#include "oneui/controls/tile.h"
#include "oneui/controls/toast.h"
#include "oneui/controls/window_title_bar.h"
#include "oneui/layout/app_shell.h"
#include "oneui/layout/overlay_host.h"
#include "oneui/controls/log_view.h"
#include "oneui/layout/panel.h"
#include "oneui/layout/reorderable_grid.h"
#include "oneui/layout/scroll_view.h"
#include "oneui/layout/stack.h"
#include "oneui/layout/top_bar.h"
#include "oneui/platform/window.h"
#include "oneui/style_adapter.h"
#include "oneui/style_sheet.h"

#include <algorithm>
#include <atomic>
#include <climits>
#include <cstdint>
#include <cwchar>
#include <cstdio>
#include <fstream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <shobjidl.h>
#include <shellapi.h>
#endif

struct OneUiWindow {
    std::unique_ptr<oneui::Window> window;
    std::shared_ptr<oneui::StyleSheet> styleSheet;
};

struct OneUiWidget {
    std::shared_ptr<oneui::Widget> widget;
    std::shared_ptr<oneui::StyleSheet> styleSheet;
    std::string tag;
    std::vector<std::string> classes;
};

struct OneUiStyleSheet {
    std::shared_ptr<oneui::StyleSheet> sheet = std::make_shared<oneui::StyleSheet>();
};

struct OneUiTray {
#ifdef _WIN32
    HWND hwnd = nullptr;
    UINT id = 0;
    bool visible = false;
    std::wstring tooltip;
#endif
};

struct OneUiOwnedCallback {
    OneUiVoidCallback callback = nullptr;
    OneUiDestroyCallback cleanup = nullptr;
    void* userData = nullptr;
    bool invoked = false;

    ~OneUiOwnedCallback() {
        if (!invoked && cleanup) {
            cleanup(userData);
        }
    }

    void run() {
        invoked = true;
        if (callback) {
            callback(userData);
        }
    }
};

namespace {

std::weak_ptr<oneui::StyleSheet> gDefaultStyleSheet;

#ifdef _WIN32
std::atomic<UINT> gTrayNextId{1};

template <std::size_t N>
void copyWideField(wchar_t (&target)[N], const wchar_t* text) {
    if (!text) {
        target[0] = L'\0';
        return;
    }
    std::wcsncpy(target, text, N - 1);
    target[N - 1] = L'\0';
}

NOTIFYICONDATAW trayData(const OneUiTray* tray) {
    NOTIFYICONDATAW data{};
    data.cbSize = sizeof(data);
    data.hWnd = tray ? tray->hwnd : nullptr;
    data.uID = tray ? tray->id : 0;
    return data;
}
#endif

std::wstring wideOrEmpty(const wchar_t* text) {
    return text ? std::wstring(text) : std::wstring();
}

constexpr std::uint32_t kReplacementCodePoint = 0xFFFD;

bool isUnicodeScalar(std::uint32_t codePoint) {
    return codePoint <= 0x10FFFF && !(codePoint >= 0xD800 && codePoint <= 0xDFFF);
}

void appendWideCodePoint(std::wstring& target, std::uint32_t codePoint) {
    if (!isUnicodeScalar(codePoint)) {
        codePoint = kReplacementCodePoint;
    }
#if WCHAR_MAX <= 0xFFFF
    if (codePoint <= 0xFFFF) {
        target.push_back(static_cast<wchar_t>(codePoint));
        return;
    }
    codePoint -= 0x10000;
    target.push_back(static_cast<wchar_t>(0xD800 + (codePoint >> 10)));
    target.push_back(static_cast<wchar_t>(0xDC00 + (codePoint & 0x3FF)));
#else
    target.push_back(static_cast<wchar_t>(codePoint));
#endif
}

void appendUtf8CodePoint(std::string& target, std::uint32_t codePoint) {
    if (!isUnicodeScalar(codePoint)) {
        codePoint = kReplacementCodePoint;
    }
    if (codePoint <= 0x7F) {
        target.push_back(static_cast<char>(codePoint));
    } else if (codePoint <= 0x7FF) {
        target.push_back(static_cast<char>(0xC0 | (codePoint >> 6)));
        target.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
    } else if (codePoint <= 0xFFFF) {
        target.push_back(static_cast<char>(0xE0 | (codePoint >> 12)));
        target.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
        target.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
    } else {
        target.push_back(static_cast<char>(0xF0 | (codePoint >> 18)));
        target.push_back(static_cast<char>(0x80 | ((codePoint >> 12) & 0x3F)));
        target.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
        target.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
    }
}

std::wstring utf8OrEmpty(OneUiUtf8String text) {
    if (!text.data || text.length == 0) {
        return {};
    }

    std::wstring result;
    result.reserve(text.length);
    const auto* bytes = reinterpret_cast<const unsigned char*>(text.data);
    for (std::size_t index = 0; index < text.length;) {
        const unsigned char first = bytes[index];
        std::uint32_t codePoint = 0;
        std::size_t sequenceLength = 1;
        std::uint32_t minimumCodePoint = 0;
        if (first <= 0x7F) {
            codePoint = first;
        } else if ((first & 0xE0) == 0xC0) {
            codePoint = first & 0x1F;
            sequenceLength = 2;
            minimumCodePoint = 0x80;
        } else if ((first & 0xF0) == 0xE0) {
            codePoint = first & 0x0F;
            sequenceLength = 3;
            minimumCodePoint = 0x800;
        } else if ((first & 0xF8) == 0xF0) {
            codePoint = first & 0x07;
            sequenceLength = 4;
            minimumCodePoint = 0x10000;
        } else {
            appendWideCodePoint(result, kReplacementCodePoint);
            ++index;
            continue;
        }

        bool valid = index + sequenceLength <= text.length;
        for (std::size_t offset = 1; valid && offset < sequenceLength; ++offset) {
            const unsigned char next = bytes[index + offset];
            if ((next & 0xC0) != 0x80) {
                valid = false;
                break;
            }
            codePoint = (codePoint << 6) | (next & 0x3F);
        }
        if (!valid || codePoint < minimumCodePoint || !isUnicodeScalar(codePoint)) {
            appendWideCodePoint(result, kReplacementCodePoint);
            ++index;
            continue;
        }

        appendWideCodePoint(result, codePoint);
        index += sequenceLength;
    }
    return result;
}

std::string utf8FromWide(const std::wstring& text) {
    std::string result;
    result.reserve(text.size());
    for (std::size_t index = 0; index < text.size(); ++index) {
        std::uint32_t codePoint = static_cast<std::uint32_t>(text[index]);
#if WCHAR_MAX <= 0xFFFF
        if (codePoint >= 0xD800 && codePoint <= 0xDBFF) {
            if (index + 1 < text.size()) {
                const std::uint32_t low = static_cast<std::uint32_t>(text[index + 1]);
                if (low >= 0xDC00 && low <= 0xDFFF) {
                    codePoint = 0x10000 + ((codePoint - 0xD800) << 10) + (low - 0xDC00);
                    ++index;
                } else {
                    codePoint = kReplacementCodePoint;
                }
            } else {
                codePoint = kReplacementCodePoint;
            }
        } else if (codePoint >= 0xDC00 && codePoint <= 0xDFFF) {
            codePoint = kReplacementCodePoint;
        }
#endif
        appendUtf8CodePoint(result, codePoint);
    }
    return result;
}

void copyUtf8Field(const std::string& value, char* buffer, std::size_t bufferLength) {
    if (!buffer || bufferLength == 0) {
        return;
    }
    const std::size_t count = std::min(bufferLength - 1, value.size());
    std::copy_n(value.data(), count, buffer);
    buffer[count] = '\0';
}

oneui::Insets toNativeInsets(OneUiInsets insets) {
    return oneui::Insets{insets.top, insets.right, insets.bottom, insets.left};
}

oneui::Color toNativeColor(OneUiColor color) {
    return oneui::Color{color.r, color.g, color.b, color.a};
}

oneui::TerminalUnderlineStyle toNativeTerminalUnderlineStyle(unsigned int style) {
    switch (style) {
    case 1:
        return oneui::TerminalUnderlineStyle::Single;
    case 2:
        return oneui::TerminalUnderlineStyle::Double;
    case 3:
        return oneui::TerminalUnderlineStyle::Curly;
    case 4:
        return oneui::TerminalUnderlineStyle::Dotted;
    case 5:
        return oneui::TerminalUnderlineStyle::Dashed;
    default:
        return oneui::TerminalUnderlineStyle::None;
    }
}

std::vector<std::string> splitClasses(const char* text) {
    std::vector<std::string> result;
    if (!text) {
        return result;
    }
    std::stringstream stream(text);
    std::string part;
    while (stream >> part) {
        result.push_back(part);
    }
    return result;
}

std::vector<std::wstring> splitWideItems(const wchar_t* text) {
    std::vector<std::wstring> result;
    if (!text) {
        return result;
    }
    std::wstringstream stream(text);
    std::wstring part;
    while (std::getline(stream, part, L'|')) {
        if (!part.empty()) {
            result.push_back(part);
        }
    }
    return result;
}

std::vector<std::wstring> splitWideBy(const std::wstring& text, wchar_t delimiter) {
    std::vector<std::wstring> result;
    std::wstringstream stream(text);
    std::wstring part;
    while (std::getline(stream, part, delimiter)) {
        result.push_back(part);
    }
    return result;
}

std::vector<oneui::ListItem> splitWideListItems(const wchar_t* text) {
    std::vector<oneui::ListItem> result;
    for (const auto& row : splitWideItems(text)) {
        const auto parts = splitWideBy(row, L'\t');
        oneui::ListItem item;
        if (!parts.empty()) {
            item.title = parts[0];
        }
        if (parts.size() > 1) {
            item.detail = parts[1];
        }
        if (!item.title.empty() || !item.detail.empty()) {
            result.push_back(std::move(item));
        }
    }
    return result;
}

std::vector<oneui::ListItem> listItemsFromUtf8(const OneUiListItemUtf8* items, std::size_t count) {
    std::vector<oneui::ListItem> result;
    if (!items || count == 0) {
        return result;
    }

    result.reserve(count);
    for (std::size_t index = 0; index < count; ++index) {
        oneui::ListItem item;
        item.title = utf8OrEmpty(items[index].title);
        item.detail = utf8OrEmpty(items[index].detail);
        if (!item.title.empty() || !item.detail.empty()) {
            result.push_back(std::move(item));
        }
    }
    return result;
}

std::vector<oneui::TreeItem> treeItemsFromUtf8(const OneUiTreeItemUtf8* items, std::size_t count) {
    std::vector<oneui::TreeItem> result;
    if (!items || count == 0) {
        return result;
    }

    result.reserve(count);
    for (std::size_t index = 0; index < count; ++index) {
        oneui::TreeItem item;
        item.id = utf8OrEmpty(items[index].id);
        item.parentId = utf8OrEmpty(items[index].parent_id);
        item.title = utf8OrEmpty(items[index].title);
        item.detail = utf8OrEmpty(items[index].detail);
        item.expanded = items[index].expanded != 0;
        result.push_back(std::move(item));
    }
    return result;
}

std::vector<oneui::TableColumn> splitWideTableColumns(const wchar_t* text) {
    std::vector<oneui::TableColumn> result;
    for (const auto& row : splitWideItems(text)) {
        const auto parts = splitWideBy(row, L'\t');
        if (parts.empty() || parts[0].empty()) {
            continue;
        }
        oneui::TableColumn column;
        column.header = parts[0];
        if (parts.size() > 1 && !parts[1].empty()) {
            wchar_t* end = nullptr;
            const float width = std::wcstof(parts[1].c_str(), &end);
            if (end != parts[1].c_str()) {
                column.width = width;
            }
        }
        result.push_back(std::move(column));
    }
    return result;
}

std::vector<std::vector<std::wstring>> splitWideTableRows(const wchar_t* text) {
    std::vector<std::vector<std::wstring>> result;
    if (!text) {
        return result;
    }
    std::wstringstream stream(text);
    std::wstring row;
    while (std::getline(stream, row, L'\n')) {
        if (!row.empty()) {
            result.push_back(splitWideBy(row, L'\t'));
        }
    }
    return result;
}

void copyError(const std::string& error, char* buffer, int len) {
    if (!buffer || len <= 0) {
        return;
    }
    const std::size_t size = static_cast<std::size_t>(len);
    const std::size_t count = std::min(size - 1, error.size());
    std::copy_n(error.c_str(), count, buffer);
    buffer[count] = '\0';
}

oneui::FocusRingStyleOverride toFocusRingOverride(const OneUiFocusRingStyle& style) {
    oneui::FocusRingStyleOverride result;
    result.color = toNativeColor(style.color);
    result.width = style.width;
    result.offset = style.offset;
    result.radius = style.radius;
    result.visible = style.visible != 0;
    return result;
}

oneui::ButtonStateStyleOverride toButtonStateOverride(const OneUiButtonStateStyle& style) {
    oneui::ButtonStateStyleOverride result;
    result.background = toNativeColor(style.background);
    result.foreground = toNativeColor(style.foreground);
    result.border = toNativeColor(style.border);
    result.borderWidth = style.border_width;
    result.radius = style.radius;
    result.focusRing = toFocusRingOverride(style.focus_ring);
    return result;
}

oneui::ButtonStyleOverride toButtonOverride(const OneUiButtonStyle& style) {
    oneui::ButtonStyleOverride result;
    result.normal = toButtonStateOverride(style.normal);
    result.hovered = toButtonStateOverride(style.hovered);
    result.pressed = toButtonStateOverride(style.pressed);
    result.disabled = toButtonStateOverride(style.disabled);
    result.focusVisible = toButtonStateOverride(style.focus_visible);
    return result;
}

oneui::InteractiveSurfaceStateStyle toInteractiveSurfaceStateStyle(
    const OneUiInteractiveSurfaceStateStyle& style) {
    oneui::InteractiveSurfaceStateStyle result;
    result.background = toNativeColor(style.background);
    result.border = toNativeColor(style.border);
    result.borderWidth = style.border_width;
    result.radius = style.radius;
    return result;
}

oneui::InteractiveSurfaceStyle toInteractiveSurfaceStyle(
    const OneUiInteractiveSurfaceStyle& style) {
    oneui::InteractiveSurfaceStyle result;
    result.normal = toInteractiveSurfaceStateStyle(style.normal);
    result.hovered = toInteractiveSurfaceStateStyle(style.hovered);
    result.pressed = toInteractiveSurfaceStateStyle(style.pressed);
    result.disabled = toInteractiveSurfaceStateStyle(style.disabled);
    result.focusVisible = toInteractiveSurfaceStateStyle(style.focus_visible);
    return result;
}

oneui::TextFieldStateStyleOverride toTextFieldStateOverride(const OneUiTextFieldStateStyle& style) {
    oneui::TextFieldStateStyleOverride result;
    result.background = toNativeColor(style.background);
    result.foreground = toNativeColor(style.foreground);
    result.placeholderForeground = toNativeColor(style.placeholder_foreground);
    result.border = toNativeColor(style.border);
    result.selectionBackground = toNativeColor(style.selection_background);
    result.caretColor = toNativeColor(style.caret_color);
    result.borderWidth = style.border_width;
    result.radius = style.radius;
    result.padding = toNativeInsets(style.padding);
    result.focusRing = toFocusRingOverride(style.focus_ring);
    return result;
}

oneui::TextFieldStyleOverride toTextFieldOverride(const OneUiTextFieldStyle& style) {
    oneui::TextFieldStyleOverride result;
    result.normal = toTextFieldStateOverride(style.normal);
    result.hovered = toTextFieldStateOverride(style.hovered);
    result.disabled = toTextFieldStateOverride(style.disabled);
    result.readOnly = toTextFieldStateOverride(style.read_only);
    result.focusVisible = toTextFieldStateOverride(style.focus_visible);
    return result;
}

oneui::PixelFormat toPixelFormat(OneUiPixelFormat format) {
    switch (format) {
    case OneUiPixelFormatRgba8888:
        return oneui::PixelFormat::Rgba8888;
    case OneUiPixelFormatNv12:
        return oneui::PixelFormat::Nv12;
    case OneUiPixelFormatBgra8888:
    default:
        return oneui::PixelFormat::Bgra8888;
    }
}

oneui::ScaleMode toScaleMode(OneUiVideoScaleMode mode) {
    switch (mode) {
    case OneUiVideoScaleModeActualSize:
        return oneui::ScaleMode::ActualSize;
    case OneUiVideoScaleModeFill:
        return oneui::ScaleMode::Fill;
    case OneUiVideoScaleModeStretch:
        return oneui::ScaleMode::Stretch;
    case OneUiVideoScaleModeFit:
    default:
        return oneui::ScaleMode::Fit;
    }
}

oneui::RemoteInputScaleMode toRemoteInputScaleMode(OneUiVideoScaleMode mode) {
    switch (mode) {
    case OneUiVideoScaleModeActualSize:
        return oneui::RemoteInputScaleMode::ActualSize;
    case OneUiVideoScaleModeFill:
        return oneui::RemoteInputScaleMode::Fill;
    case OneUiVideoScaleModeStretch:
        return oneui::RemoteInputScaleMode::Stretch;
    case OneUiVideoScaleModeFit:
    default:
        return oneui::RemoteInputScaleMode::Fit;
    }
}

OneUiPointerButton toCButton(oneui::PointerButton button) {
    switch (button) {
    case oneui::PointerButton::Left:
        return OneUiPointerButtonLeft;
    case oneui::PointerButton::Right:
        return OneUiPointerButtonRight;
    case oneui::PointerButton::Middle:
        return OneUiPointerButtonMiddle;
    case oneui::PointerButton::X1:
        return OneUiPointerButtonX1;
    case oneui::PointerButton::X2:
        return OneUiPointerButtonX2;
    case oneui::PointerButton::None:
    default:
        return OneUiPointerButtonNone;
    }
}

OneUiPointerButton toCButton(oneui::MouseButton button) {
    switch (button) {
    case oneui::MouseButton::Left:
        return OneUiPointerButtonLeft;
    case oneui::MouseButton::Right:
        return OneUiPointerButtonRight;
    case oneui::MouseButton::Middle:
        return OneUiPointerButtonMiddle;
    case oneui::MouseButton::None:
    default:
        return OneUiPointerButtonNone;
    }
}

OneUiTerminalPointerAction toCTerminalPointerAction(oneui::TerminalPointerAction action) {
    switch (action) {
    case oneui::TerminalPointerAction::Press:
        return OneUiTerminalPointerActionPress;
    case oneui::TerminalPointerAction::Release:
        return OneUiTerminalPointerActionRelease;
    case oneui::TerminalPointerAction::Wheel:
        return OneUiTerminalPointerActionWheel;
    case oneui::TerminalPointerAction::Move:
    default:
        return OneUiTerminalPointerActionMove;
    }
}

template <typename T>
T* asWidget(OneUiWidget* widget) {
    static_assert(std::is_base_of<oneui::Widget, T>::value, "T must be a OneUI widget");
    if (!widget || !widget->widget) {
        return nullptr;
    }
    return dynamic_cast<T*>(widget->widget.get());
}

OneUiWidget* wrap(std::shared_ptr<oneui::Widget> widget) {
    if (!widget) {
        return nullptr;
    }
    auto wrapper = std::make_unique<OneUiWidget>();
    wrapper->widget = std::move(widget);
    wrapper->styleSheet = gDefaultStyleSheet.lock();
    return wrapper.release();
}

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
    const oneui::StyleNode node = styleNodeFor(wrapper);
    applyPreferredSizeFromStyle(*wrapper->widget, wrapper->styleSheet->resolve(node));

    if (auto* button = dynamic_cast<oneui::Button*>(wrapper->widget.get())) {
        button->setStyleOverride(oneui::buttonStyleOverrideFromStyleSheet(*wrapper->styleSheet, node));
        return;
    }
    if (auto* badge = dynamic_cast<oneui::Badge*>(wrapper->widget.get())) {
        const oneui::StyleBox box = wrapper->styleSheet->resolve(node);
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
        const oneui::StyleBox box = wrapper->styleSheet->resolve(node);
        if (box.padding) {
            card->setPadding(*box.padding);
        }
        card->setStyleBox(box);
        return;
    }
    if (auto* iconBadge = dynamic_cast<oneui::IconBadge*>(wrapper->widget.get())) {
        iconBadge->setStyleBox(wrapper->styleSheet->resolve(node));
        return;
    }
    if (auto* popup = dynamic_cast<oneui::Popup*>(wrapper->widget.get())) {
        popup->setStyleOverride(
            oneui::popupStyleOverrideFromStyleSheet(*wrapper->styleSheet, node));
        return;
    }
    if (auto* menu = dynamic_cast<oneui::Menu*>(wrapper->widget.get())) {
        menu->setStyleSheet(wrapper->styleSheet, node);
        return;
    }
    if (auto* dialog = dynamic_cast<oneui::Dialog*>(wrapper->widget.get())) {
        dialog->setStyleSheet(wrapper->styleSheet, node);
        return;
    }
    if (auto* tile = dynamic_cast<oneui::Tile*>(wrapper->widget.get())) {
        tile->setStyleSheet(wrapper->styleSheet, node);
        return;
    }
    if (auto* toast = dynamic_cast<oneui::Toast*>(wrapper->widget.get())) {
        toast->setStyleSheet(wrapper->styleSheet, node);
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
        style.normal = stateOverride(wrapper->styleSheet->resolve(oneui::StyleNode{node.tag, node.classes, oneui::StyleStateNone}));
        style.hovered = stateOverride(wrapper->styleSheet->resolve(oneui::StyleNode{node.tag, node.classes, oneui::StyleStateHover}));
        style.pressed = stateOverride(wrapper->styleSheet->resolve(oneui::StyleNode{node.tag, node.classes, oneui::StyleStateActive}));
        style.selected = stateOverride(wrapper->styleSheet->resolve(oneui::StyleNode{node.tag, node.classes, oneui::StyleStateSelected}));
        style.disabled = stateOverride(wrapper->styleSheet->resolve(oneui::StyleNode{node.tag, node.classes, oneui::StyleStateDisabled}));
        radio->setStyleOverride(style);
        return;
    }
    if (auto* statusStrip = dynamic_cast<oneui::StatusStrip*>(wrapper->widget.get())) {
        statusStrip->setStyleSheet(wrapper->styleSheet, node);
        return;
    }
    if (auto* stateView = dynamic_cast<oneui::StateView*>(wrapper->widget.get())) {
        stateView->setStyleSheet(wrapper->styleSheet, node);
        return;
    }
    if (auto* tabs = dynamic_cast<oneui::Tabs*>(wrapper->widget.get())) {
        auto stateOverride = [](const oneui::StyleBox& box) {
            oneui::TabsStateStyleOverride state;
            state.background = box.background.color;
            state.border = box.borderColor;
            state.itemBackground = box.content.backgroundColor ? box.content.backgroundColor : box.background.color;
            state.itemForeground = box.foreground;
            state.selectedItemBackground = box.content.backgroundColor ? box.content.backgroundColor : box.background.color;
            state.selectedItemForeground = box.foreground;
            state.selectedItemBorder = box.borderColor;
            state.borderWidth = box.borderWidth;
            state.radius = box.radius;
            state.itemRadius = box.content.radius ? box.content.radius : box.radius;
            state.itemBorderWidth = box.borderWidth;
            if (box.padding) {
                state.itemInset = *box.padding;
            }
            if (box.outlineColor || box.outlineWidth || box.outlineOffset || box.radius) {
                oneui::FocusRingStyleOverride ring;
                ring.color = box.outlineColor;
                ring.width = box.outlineWidth;
                ring.offset = box.outlineOffset;
                ring.radius = box.radius;
                ring.visible = true;
                state.focusRing = ring;
            }
            return state;
        };
        oneui::TabsStyleOverride style;
        style.normal = stateOverride(wrapper->styleSheet->resolve(oneui::StyleNode{node.tag, node.classes, oneui::StyleStateNone}));
        style.hovered = stateOverride(wrapper->styleSheet->resolve(oneui::StyleNode{node.tag, node.classes, oneui::StyleStateHover}));
        style.pressed = stateOverride(wrapper->styleSheet->resolve(oneui::StyleNode{node.tag, node.classes, oneui::StyleStateActive}));
        style.selected = stateOverride(wrapper->styleSheet->resolve(oneui::StyleNode{node.tag, node.classes, oneui::StyleStateSelected}));
        style.disabled = stateOverride(wrapper->styleSheet->resolve(oneui::StyleNode{node.tag, node.classes, oneui::StyleStateDisabled}));
        style.focusVisible = stateOverride(wrapper->styleSheet->resolve(oneui::StyleNode{node.tag, node.classes, oneui::StyleStateFocus}));
        tabs->setStyleOverride(style);
        return;
    }
    if (auto* select = dynamic_cast<oneui::Select*>(wrapper->widget.get())) {
        select->setStyleOverride(
            oneui::selectStyleOverrideFromStyleSheet(*wrapper->styleSheet, node));
        return;
    }
    if (auto* virtualList = dynamic_cast<oneui::VirtualList*>(wrapper->widget.get())) {
        virtualList->setStyleOverride(oneui::listStyleOverrideFromStyleSheet(*wrapper->styleSheet, node));
        return;
    }
    if (auto* list = dynamic_cast<oneui::List*>(wrapper->widget.get())) {
        list->setStyleOverride(oneui::listStyleOverrideFromStyleSheet(*wrapper->styleSheet, node));
        return;
    }
    if (auto* tree = dynamic_cast<oneui::TreeView*>(wrapper->widget.get())) {
        tree->setStyleOverride(oneui::treeViewStyleOverrideFromStyleSheet(*wrapper->styleSheet, node));
        return;
    }
    if (auto* textField = dynamic_cast<oneui::TextField*>(wrapper->widget.get())) {
        textField->setStyleOverride(oneui::textFieldStyleOverrideFromStyleSheet(*wrapper->styleSheet, node));
        return;
    }
    if (auto* terminal = dynamic_cast<oneui::TerminalView*>(wrapper->widget.get())) {
        terminal->setStyleBox(wrapper->styleSheet->resolve(node));
        return;
    }
    if (auto* scrollView = dynamic_cast<oneui::ScrollView*>(wrapper->widget.get())) {
        const oneui::StyleBox box = wrapper->styleSheet->resolve(node);
        if (box.foreground || box.borderWidth) {
            scrollView->setScrollbarStyle(
                box.foreground.value_or(oneui::Color{148, 163, 184, 180}),
                box.borderWidth.value_or(4.0f));
        }
        return;
    }
    if (auto* grid = dynamic_cast<oneui::ReorderableGrid*>(wrapper->widget.get())) {
        grid->setStyleBox(wrapper->styleSheet->resolve(node));
        return;
    }
    if (auto* surface = dynamic_cast<oneui::InteractiveSurface*>(wrapper->widget.get())) {
        const oneui::StyleBox box = wrapper->styleSheet->resolve(node);
        surface->setStyle(oneui::interactiveSurfaceStyleFromStyleSheet(*wrapper->styleSheet, node));
        if (box.padding) {
            surface->setPadding(*box.padding);
        }
        return;
    }
    if (auto* panel = dynamic_cast<oneui::Panel*>(wrapper->widget.get())) {
        const oneui::StyleBox box = wrapper->styleSheet->resolve(node);
        if (box.padding) {
            panel->setPadding(*box.padding);
        }
        panel->setStyleBox(box);
        return;
    }
    if (auto* label = dynamic_cast<oneui::Label*>(wrapper->widget.get())) {
        const oneui::StyleBox box = wrapper->styleSheet->resolve(node);
        if (box.foreground) {
            label->setColor(*box.foreground);
        }
        if (box.fontSize) {
            label->setFontSize(*box.fontSize);
        }
        if (box.fontWeight) {
            label->setFontWeight(*box.fontWeight);
        }
        return;
    }
    if (auto* icon = dynamic_cast<oneui::IconView*>(wrapper->widget.get())) {
        const oneui::StyleBox box = wrapper->styleSheet->resolve(node);
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
        iconButton->setStyleSheet(wrapper->styleSheet, node);
        return;
    }
    if (auto* switchWidget = dynamic_cast<oneui::Switch*>(wrapper->widget.get())) {
        switchWidget->setStyleOverride(oneui::switchStyleOverrideFromStyleSheet(*wrapper->styleSheet, node));
        return;
    }
    if (auto* titleBar = dynamic_cast<oneui::WindowTitleBar*>(wrapper->widget.get())) {
        titleBar->setStyleSheet(wrapper->styleSheet);
        return;
    }
    if (auto* navItem = dynamic_cast<oneui::NavItem*>(wrapper->widget.get())) {
        navItem->setStyleSheet(wrapper->styleSheet);
        return;
    }
    if (auto* appShell = dynamic_cast<oneui::AppShell*>(wrapper->widget.get())) {
        const oneui::StyleBox box = wrapper->styleSheet->resolve(node);
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
        const oneui::StyleBox box = wrapper->styleSheet->resolve(node);
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
        const oneui::StyleBox box = wrapper->styleSheet->resolve(node);
        if (box.padding) {
            stack->setPadding(*box.padding);
        }
        if (box.gap) {
            stack->setGap(*box.gap);
        }
        stack->setStyleBox(box);
    }
}

void applyCurrentStyleSheet(OneUiWidget* wrapper) {
    if (!wrapper || !wrapper->widget) {
        return;
    }
    auto sheet = wrapper->styleSheet ? wrapper->styleSheet : gDefaultStyleSheet.lock();
    if (sheet) {
        applyStyleSheet(wrapper, std::move(sheet));
    }
}

} // namespace

const char* oneui_version(void) {
    return "0.1.0";
}

unsigned int oneui_utf8_abi_version(void) {
    return ONEUI_UTF8_ABI_VERSION;
}

OneUiWindow* oneui_window_create(const OneUiWindowOptions* options) {
    try {
        oneui::WindowOptions nativeOptions;
        nativeOptions.title = options && options->title ? wideOrEmpty(options->title) : L"OneUI";
        nativeOptions.width = options && options->width > 0 ? options->width : 1280;
        nativeOptions.height = options && options->height > 0 ? options->height : 800;
        nativeOptions.visible = options ? options->visible != 0 : false;
        nativeOptions.borderless = options ? options->borderless != 0 : false;
        nativeOptions.fullscreen = options ? options->fullscreen != 0 : false;
        nativeOptions.topmost = options ? options->topmost != 0 : false;
        nativeOptions.resizable = options ? options->resizable != 0 : true;

        auto wrapper = std::make_unique<OneUiWindow>();
        wrapper->window = oneui::Window::create(std::move(nativeOptions));
        if (!wrapper->window) {
            return nullptr;
        }
        return wrapper.release();
    } catch (...) {
        return nullptr;
    }
}

OneUiWindow* oneui_window_create_utf8(const OneUiWindowOptionsUtf8* options) {
    try {
        oneui::WindowOptions nativeOptions;
        nativeOptions.title = options ? utf8OrEmpty(options->title) : L"OneUI";
        nativeOptions.width = options && options->width > 0 ? options->width : 1280;
        nativeOptions.height = options && options->height > 0 ? options->height : 800;
        nativeOptions.visible = options ? options->visible != 0 : false;
        nativeOptions.borderless = options ? options->borderless != 0 : false;
        nativeOptions.fullscreen = options ? options->fullscreen != 0 : false;
        nativeOptions.topmost = options ? options->topmost != 0 : false;
        nativeOptions.resizable = options ? options->resizable != 0 : true;

        auto wrapper = std::make_unique<OneUiWindow>();
        wrapper->window = oneui::Window::create(std::move(nativeOptions));
        if (!wrapper->window) {
            return nullptr;
        }
        return wrapper.release();
    } catch (...) {
        return nullptr;
    }
}

void oneui_window_destroy(OneUiWindow* window) {
    delete window;
}

void oneui_window_initialize(OneUiWindow* window) {
    if (!window || !window->window) {
        return;
    }
    window->window->initialize();
}

void oneui_window_show(OneUiWindow* window) {
    if (!window || !window->window) {
        return;
    }
    window->window->show();
}

void oneui_window_activate(OneUiWindow* window) {
    if (!window || !window->window) {
        return;
    }
    window->window->activate();
}

int oneui_window_run(OneUiWindow* window) {
    if (!window || !window->window) {
        return -1;
    }
    try {
        return window->window->run();
    } catch (...) {
        return -1;
    }
}

void oneui_window_close(OneUiWindow* window) {
    if (!window || !window->window) {
        return;
    }
    window->window->close();
}

void oneui_window_request_close(OneUiWindow* window) {
    if (!window || !window->window) {
        return;
    }
    window->window->post([window] {
        if (window->window) {
            window->window->close();
        }
    });
}

void oneui_window_minimize(OneUiWindow* window) {
    if (!window || !window->window) {
        return;
    }
    window->window->minimize();
}

void oneui_window_toggle_maximize(OneUiWindow* window) {
    if (!window || !window->window) {
        return;
    }
    window->window->toggleMaximize();
}

void oneui_window_set_borderless(OneUiWindow* window, int borderless) {
    if (!window || !window->window) {
        return;
    }
    window->window->setBorderless(borderless != 0);
}

void oneui_window_set_title_bar_drag_metrics(OneUiWindow* window, float title_bar_height, float reserved_button_width) {
    if (!window || !window->window) {
        return;
    }
    window->window->setTitleBarDragMetrics(title_bar_height, reserved_button_width);
}

void oneui_window_set_corner_radius(OneUiWindow* window, float radius) {
    if (!window || !window->window) {
        return;
    }
    window->window->setCornerRadius(radius);
}

void oneui_window_set_close_to_tray(OneUiWindow* window, int close_to_tray) {
    if (!window || !window->window) {
        return;
    }
    window->window->setCloseToTray(close_to_tray != 0);
}

void oneui_window_post(OneUiWindow* window, OneUiVoidCallback callback, void* user_data) {
    (void)oneui_window_post_owned(window, callback, user_data, nullptr);
}

int oneui_window_post_owned(
    OneUiWindow* window,
    OneUiVoidCallback callback,
    void* user_data,
    OneUiDestroyCallback cleanup) {
    if (!window || !window->window || !callback) {
        if (cleanup) {
            cleanup(user_data);
        }
        return 0;
    }
    auto ownedCallback = std::make_shared<OneUiOwnedCallback>();
    ownedCallback->callback = callback;
    ownedCallback->cleanup = cleanup;
    ownedCallback->userData = user_data;
    return window->window->post([ownedCallback] {
        ownedCallback->run();
    }) ? 1 : 0;
}

void oneui_window_request_animation_frame(OneUiWindow* window, OneUiFrameCallback callback, void* user_data) {
    if (!window || !window->window || !callback) {
        return;
    }
    window->window->requestAnimationFrame([callback, user_data](double nowMs) {
        callback(nowMs, user_data);
    });
}

void oneui_window_set_title(OneUiWindow* window, const wchar_t* title) {
    if (!window || !window->window) {
        return;
    }
    window->window->setTitle(title ? std::wstring(title) : std::wstring());
}

void oneui_window_set_title_utf8(OneUiWindow* window, OneUiUtf8String title) {
    if (!window || !window->window) {
        return;
    }
    window->window->setTitle(utf8OrEmpty(title));
}

void* oneui_window_native_handle(OneUiWindow* window) {
    if (!window || !window->window) {
        return nullptr;
    }
    return window->window->nativeHandle();
}

int oneui_window_client_width(OneUiWindow* window) {
    if (!window || !window->window) {
        return 0;
    }
    return static_cast<int>(window->window->clientSize().width);
}

int oneui_window_client_height(OneUiWindow* window) {
    if (!window || !window->window) {
        return 0;
    }
    return static_cast<int>(window->window->clientSize().height);
}

int oneui_window_client_pixel_width(OneUiWindow* window) {
    if (!window || !window->window) {
        return 0;
    }
    return static_cast<int>(window->window->clientPixelSize().width);
}

int oneui_window_client_pixel_height(OneUiWindow* window) {
    if (!window || !window->window) {
        return 0;
    }
    return static_cast<int>(window->window->clientPixelSize().height);
}

float oneui_window_dpi_scale(OneUiWindow* window) {
    if (!window || !window->window) {
        return 1.0f;
    }
    return window->window->dpiScale();
}

void oneui_window_set_content(OneUiWindow* window, OneUiWidget* widget) {
    if (!window || !window->window || !widget || !widget->widget) {
        return;
    }
    if (window->styleSheet) {
        applyStyleSheet(widget, window->styleSheet);
    }
    window->window->setContent(widget->widget);
}

int oneui_window_request_focus(OneUiWindow* window, OneUiWidget* widget, int focus_visible) {
    if (!window || !window->window || !widget || !widget->widget) {
        return 0;
    }
    return window->window->requestFocus(widget->widget.get(), focus_visible != 0) ? 1 : 0;
}

void oneui_window_set_style_sheet(OneUiWindow* window, OneUiStyleSheet* style_sheet) {
    if (!window || !style_sheet || !style_sheet->sheet) {
        return;
    }
    window->styleSheet = style_sheet->sheet;
    gDefaultStyleSheet = style_sheet->sheet;
}

int oneui_window_pick_folder(OneUiWindow* window, const wchar_t* title, wchar_t* out, int outLen) {
#ifdef _WIN32
    if (!out || outLen <= 0) {
        return 0;
    }
    out[0] = L'\0';
    HWND owner = nullptr;
    if (window && window->window) {
        owner = reinterpret_cast<HWND>(window->window->nativeHandle());
    }

    const bool comInited = SUCCEEDED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE));
    int picked = 0;
    IFileOpenDialog* dialog = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog)))) {
        DWORD options = 0;
        dialog->GetOptions(&options);
        dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);
        if (title && title[0] != L'\0') {
            dialog->SetTitle(title);
        }
        if (SUCCEEDED(dialog->Show(owner))) {
            IShellItem* item = nullptr;
            if (SUCCEEDED(dialog->GetResult(&item)) && item) {
                PWSTR path = nullptr;
                if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path)) && path) {
                    lstrcpynW(out, path, outLen);
                    picked = 1;
                    CoTaskMemFree(path);
                }
                item->Release();
            }
        }
        dialog->Release();
    }
    if (comInited) {
        CoUninitialize();
    }
    return picked;
#else
    (void)window;
    (void)title;
    (void)out;
    (void)outLen;
    return 0;
#endif
}

int oneui_window_confirm(OneUiWindow* window, const wchar_t* title, const wchar_t* message) {
#ifdef _WIN32
    HWND owner = nullptr;
    if (window && window->window) {
        owner = reinterpret_cast<HWND>(window->window->nativeHandle());
    }
    const wchar_t* dialogTitle = title && title[0] != L'\0' ? title : L"Confirm";
    const wchar_t* dialogMessage = message && message[0] != L'\0' ? message : L"Continue?";
    const int result = MessageBoxW(
        owner,
        dialogMessage,
        dialogTitle,
        MB_ICONQUESTION | MB_YESNO | MB_DEFBUTTON2 | MB_APPLMODAL);
    return result == IDYES ? 1 : 0;
#else
    (void)window;
    (void)title;
    (void)message;
    return 0;
#endif
}

OneUiTray* oneui_tray_create(OneUiWindow* window, const wchar_t* tooltip) {
#ifdef _WIN32
    if (!window || !window->window) {
        return nullptr;
    }
    auto hwnd = reinterpret_cast<HWND>(window->window->nativeHandle());
    if (!hwnd) {
        return nullptr;
    }

    auto tray = std::make_unique<OneUiTray>();
    tray->hwnd = hwnd;
    tray->id = gTrayNextId.fetch_add(1);
    tray->tooltip = wideOrEmpty(tooltip);
    return tray.release();
#else
    (void)window;
    (void)tooltip;
    return nullptr;
#endif
}

void oneui_tray_destroy(OneUiTray* tray) {
    if (!tray) {
        return;
    }
    oneui_tray_hide(tray);
    delete tray;
}

int oneui_tray_show(OneUiTray* tray) {
#ifdef _WIN32
    if (!tray || !tray->hwnd) {
        return 0;
    }
    if (tray->visible) {
        return 1;
    }

    auto data = trayData(tray);
    // NIF_MESSAGE：让托盘点击/右键事件投递到窗口过程（Win32Window 里处理还原/菜单）。
    data.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE;
    data.uCallbackMessage = oneui::kTrayCallbackMessage;
    // 优先用可执行文件里嵌入的品牌图标（资源 ID 1）；取不到再退回系统默认图标。
    HICON appIcon = static_cast<HICON>(LoadImageW(GetModuleHandleW(nullptr), MAKEINTRESOURCEW(1), IMAGE_ICON,
                                                  GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR));
    data.hIcon = appIcon ? appIcon : LoadIconW(nullptr, IDI_APPLICATION);
    copyWideField(data.szTip, tray->tooltip.c_str());
    if (!Shell_NotifyIconW(NIM_ADD, &data)) {
        return 0;
    }

    data.uVersion = NOTIFYICON_VERSION_4;
    Shell_NotifyIconW(NIM_SETVERSION, &data);
    tray->visible = true;
    return 1;
#else
    (void)tray;
    return 0;
#endif
}

int oneui_tray_hide(OneUiTray* tray) {
#ifdef _WIN32
    if (!tray || !tray->hwnd || !tray->visible) {
        return 1;
    }

    auto data = trayData(tray);
    if (!Shell_NotifyIconW(NIM_DELETE, &data)) {
        return 0;
    }
    tray->visible = false;
    return 1;
#else
    (void)tray;
    return 0;
#endif
}

void oneui_tray_set_tooltip(OneUiTray* tray, const wchar_t* tooltip) {
#ifdef _WIN32
    if (!tray || !tray->hwnd) {
        return;
    }
    tray->tooltip = wideOrEmpty(tooltip);
    if (!tray->visible) {
        return;
    }

    auto data = trayData(tray);
    data.uFlags = NIF_TIP;
    copyWideField(data.szTip, tray->tooltip.c_str());
    Shell_NotifyIconW(NIM_MODIFY, &data);
#else
    (void)tray;
    (void)tooltip;
#endif
}

int oneui_tray_show_notification(OneUiTray* tray, const wchar_t* title, const wchar_t* message) {
#ifdef _WIN32
    if (!tray || !tray->hwnd) {
        return 0;
    }
    if (!tray->visible && !oneui_tray_show(tray)) {
        return 0;
    }

    auto data = trayData(tray);
    data.uFlags = NIF_INFO;
    data.dwInfoFlags = NIIF_INFO | NIIF_NOSOUND;
    copyWideField(data.szInfoTitle, title);
    copyWideField(data.szInfo, message);
    return Shell_NotifyIconW(NIM_MODIFY, &data) ? 1 : 0;
#else
    (void)tray;
    (void)title;
    (void)message;
    return 0;
#endif
}

void oneui_widget_destroy(OneUiWidget* widget) {
    delete widget;
}

void oneui_widget_set_preferred_size(OneUiWidget* widget, float width, float height) {
    if (!widget || !widget->widget) {
        return;
    }
    widget->widget->setPreferredSize(oneui::Size{width, height});
}

void oneui_widget_set_disabled(OneUiWidget* widget, int disabled) {
    if (!widget || !widget->widget) {
        return;
    }
    widget->widget->setDisabled(disabled != 0);
}

void oneui_widget_set_tab_stop(OneUiWidget* widget, int tab_stop) {
    if (!widget || !widget->widget) {
        return;
    }
    widget->widget->setTabStop(tab_stop != 0);
}

void oneui_widget_set_visible(OneUiWidget* widget, int visible) {
    if (!widget || !widget->widget) {
        return;
    }
    widget->widget->setVisible(visible != 0);
}

void oneui_widget_set_classes(OneUiWidget* widget, const char* classes) {
    if (!widget || !widget->widget) {
        return;
    }
    widget->classes = splitClasses(classes);
    applyCurrentStyleSheet(widget);
}

void oneui_widget_set_style_node(OneUiWidget* widget, const char* tag, const char* classes) {
    if (!widget || !widget->widget) {
        return;
    }
    widget->tag = tag ? tag : "";
    widget->classes = splitClasses(classes);
    applyCurrentStyleSheet(widget);
}

void oneui_widget_apply_style_sheet(OneUiWidget* widget, OneUiStyleSheet* style_sheet) {
    if (!widget || !style_sheet || !style_sheet->sheet) {
        return;
    }
    applyStyleSheet(widget, style_sheet->sheet);
}

OneUiStyleSheet* oneui_style_sheet_create(void) {
    return new OneUiStyleSheet();
}

void oneui_style_sheet_destroy(OneUiStyleSheet* style_sheet) {
    delete style_sheet;
}

void oneui_style_sheet_set_custom_property(OneUiStyleSheet* style_sheet, const char* name, const char* value) {
    if (!style_sheet || !style_sheet->sheet || !name || !value) {
        return;
    }
    style_sheet->sheet->setCustomProperty(name, value);
}

int oneui_style_sheet_add_css(OneUiStyleSheet* style_sheet, const char* css, char* error_buffer, int error_buffer_len) {
    if (!style_sheet || !style_sheet->sheet || !css) {
        copyError("Invalid OneUI style sheet or CSS input", error_buffer, error_buffer_len);
        return 0;
    }
    std::string error;
    const bool ok = style_sheet->sheet->addRulesFromCss(css, &error);
    if (!ok) {
        copyError(error, error_buffer, error_buffer_len);
        return 0;
    }
    copyError("", error_buffer, error_buffer_len);
    return 1;
}

int oneui_style_sheet_load_file(OneUiStyleSheet* style_sheet, const wchar_t* path, char* error_buffer, int error_buffer_len) {
    if (!style_sheet || !style_sheet->sheet || !path) {
        copyError("Invalid OneUI style sheet or file path", error_buffer, error_buffer_len);
        return 0;
    }
    FILE* file = nullptr;
    if (_wfopen_s(&file, path, L"rb") != 0 || !file) {
        copyError("Failed to open CSS file", error_buffer, error_buffer_len);
        return 0;
    }
    std::string content;
    char buffer[4096];
    while (const std::size_t read = std::fread(buffer, 1, sizeof(buffer), file)) {
        content.append(buffer, read);
    }
    std::fclose(file);
    return oneui_style_sheet_add_css(style_sheet, content.c_str(), error_buffer, error_buffer_len);
}

int oneui_clipboard_set_text(const wchar_t* text) {
    try {
        oneui::SystemClipboard clipboard;
        clipboard.setText(wideOrEmpty(text));
        return 1;
    } catch (...) {
        return 0;
    }
}

int oneui_clipboard_get_text(wchar_t* buffer, int buffer_len) {
    try {
        oneui::SystemClipboard clipboard;
        const std::wstring value = clipboard.text();
        const int required = static_cast<int>(value.size()) + 1;
        if (buffer && buffer_len > 0) {
            const int count = std::min(buffer_len - 1, static_cast<int>(value.size()));
            std::copy_n(value.c_str(), count, buffer);
            buffer[count] = L'\0';
        }
        return required;
    } catch (...) {
        if (buffer && buffer_len > 0) {
            buffer[0] = L'\0';
        }
        return 0;
    }
}

int oneui_clipboard_set_text_utf8(OneUiUtf8String text) {
    try {
        oneui::SystemClipboard clipboard;
        clipboard.setText(utf8OrEmpty(text));
        return 1;
    } catch (...) {
        return 0;
    }
}

std::size_t oneui_clipboard_get_text_utf8(char* buffer, std::size_t buffer_len) {
    try {
        oneui::SystemClipboard clipboard;
        const std::string value = utf8FromWide(clipboard.text());
        copyUtf8Field(value, buffer, buffer_len);
        return value.size() + 1;
    } catch (...) {
        if (buffer && buffer_len > 0) {
            buffer[0] = '\0';
        }
        return 0;
    }
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

OneUiWidget* oneui_overlay_host_create(void) {
    return wrap(std::make_shared<oneui::OverlayHost>());
}

void oneui_overlay_host_set_content(OneUiWidget* host, OneUiWidget* child) {
    auto* nativeHost = asWidget<oneui::OverlayHost>(host);
    if (!nativeHost) {
        return;
    }
    nativeHost->setContent(child ? child->widget : nullptr);
}

void oneui_overlay_host_add_overlay(OneUiWidget* host, OneUiWidget* child, int layer) {
    auto* nativeHost = asWidget<oneui::OverlayHost>(host);
    if (!nativeHost || !child || !child->widget) {
        return;
    }
    nativeHost->addOverlay(child->widget, layer);
}

void oneui_overlay_host_add_anchored_overlay(
    OneUiWidget* host,
    OneUiWidget* child,
    int layer,
    float width,
    float height,
    OneUiInsets margin,
    int horizontal_alignment,
    int vertical_alignment) {
    auto* nativeHost = asWidget<oneui::OverlayHost>(host);
    if (!nativeHost || !child || !child->widget) {
        return;
    }
    nativeHost->addAnchoredOverlay(
        child->widget,
        oneui::OverlayOptions::modeless(layer),
        oneui::Size{width, height},
        toNativeInsets(margin),
        horizontal_alignment,
        vertical_alignment);
}

void oneui_overlay_host_add_modal_anchored_overlay(
    OneUiWidget* host,
    OneUiWidget* child,
    int layer,
    float width,
    float height,
    OneUiInsets margin,
    int horizontal_alignment,
    int vertical_alignment) {
    auto* nativeHost = asWidget<oneui::OverlayHost>(host);
    if (!nativeHost || !child || !child->widget) {
        return;
    }
    nativeHost->addAnchoredOverlay(
        child->widget,
        oneui::OverlayOptions::modal(layer),
        oneui::Size{width, height},
        toNativeInsets(margin),
        horizontal_alignment,
        vertical_alignment);
}

int oneui_overlay_host_update_anchored_overlay(
    OneUiWidget* host,
    OneUiWidget* child,
    float width,
    float height,
    OneUiInsets margin,
    int horizontal_alignment,
    int vertical_alignment) {
    auto* nativeHost = asWidget<oneui::OverlayHost>(host);
    if (!nativeHost || !child || !child->widget) {
        return 0;
    }
    return nativeHost->updateAnchoredOverlay(
               child->widget.get(),
               oneui::Size{width, height},
               toNativeInsets(margin),
               horizontal_alignment,
               vertical_alignment)
               ? 1
               : 0;
}

int oneui_overlay_host_remove_overlay(OneUiWidget* host, OneUiWidget* child) {
    auto* nativeHost = asWidget<oneui::OverlayHost>(host);
    if (!nativeHost || !child || !child->widget) {
        return 0;
    }
    return nativeHost->removeOverlay(child->widget.get()) ? 1 : 0;
}

OneUiWidget* oneui_popup_create(void) {
    return wrap(std::make_shared<oneui::Popup>());
}

void oneui_popup_set_anchor(OneUiWidget* popup, OneUiWidget* anchor) {
    if (auto* nativePopup = asWidget<oneui::Popup>(popup)) {
        nativePopup->setAnchor(anchor ? anchor->widget : nullptr);
    }
}

void oneui_popup_set_content(OneUiWidget* popup, OneUiWidget* content) {
    if (auto* nativePopup = asWidget<oneui::Popup>(popup)) {
        nativePopup->setContent(content ? content->widget : nullptr);
    }
}

void oneui_popup_set_open(OneUiWidget* popup, int open) {
    if (auto* nativePopup = asWidget<oneui::Popup>(popup)) {
        nativePopup->setOpen(open != 0);
    }
}

int oneui_popup_is_open(OneUiWidget* popup) {
    if (const auto* nativePopup = asWidget<oneui::Popup>(popup)) {
        return nativePopup->isOpen() ? 1 : 0;
    }
    return 0;
}

void oneui_popup_set_anchor_rect(
    OneUiWidget* popup,
    float x,
    float y,
    float width,
    float height) {
    if (auto* nativePopup = asWidget<oneui::Popup>(popup)) {
        nativePopup->setAnchorRect(oneui::Rect{x, y, width, height});
    }
}

void oneui_popup_clear_anchor_rect(OneUiWidget* popup) {
    if (auto* nativePopup = asWidget<oneui::Popup>(popup)) {
        nativePopup->clearAnchorRect();
    }
}

void oneui_popup_set_preferred_placement(OneUiWidget* popup, int placement) {
    if (auto* nativePopup = asWidget<oneui::Popup>(popup)) {
        nativePopup->setPreferredPlacement(static_cast<oneui::PopupPreferredPlacement>(
            std::clamp(placement, 0, 5)));
    }
}

void oneui_popup_set_interaction_mode(OneUiWidget* popup, int mode) {
    if (auto* nativePopup = asWidget<oneui::Popup>(popup)) {
        nativePopup->setInteractionMode(static_cast<oneui::PopupInteractionMode>(
            std::clamp(mode, 0, 2)));
    }
}

OneUiWidget* oneui_log_view_create(void) {
    auto view = std::make_shared<oneui::LogView>();
    // 日志查看器默认接系统剪贴板，Ctrl+C 复制选中内容开箱即用。
    view->setClipboard(std::make_shared<oneui::SystemClipboard>());
    return wrap(view);
}

void oneui_log_view_append_line(OneUiWidget* view, const wchar_t* text, unsigned char r, unsigned char g, unsigned char b, unsigned char a) {
    auto* nativeView = asWidget<oneui::LogView>(view);
    if (!nativeView) {
        return;
    }
    nativeView->appendLine(text ? text : L"", oneui::Color{r, g, b, a});
}

void oneui_log_view_clear(OneUiWidget* view) {
    auto* nativeView = asWidget<oneui::LogView>(view);
    if (!nativeView) {
        return;
    }
    nativeView->clearLines();
}

float oneui_log_view_content_height(OneUiWidget* view) {
    auto* nativeView = asWidget<oneui::LogView>(view);
    if (!nativeView) {
        return 0.0f;
    }
    return nativeView->contentHeight();
}

void oneui_log_view_set_font_size(OneUiWidget* view, float size) {
    if (auto* nativeView = asWidget<oneui::LogView>(view)) {
        nativeView->setFontSize(size);
    }
}

void oneui_log_view_set_line_height(OneUiWidget* view, float height) {
    if (auto* nativeView = asWidget<oneui::LogView>(view)) {
        nativeView->setLineHeight(height);
    }
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

void oneui_scroll_view_scroll_to_bottom(OneUiWidget* view) {
    auto* nativeView = asWidget<oneui::ScrollView>(view);
    if (!nativeView) {
        return;
    }
    // 初次布局前 viewport 高度为 0，此处偏移会暂设为内容全高；
    // layoutChildren 会在布局时回夹到真正的“底部”位置。
    nativeView->setScrollOffset(nativeView->maxScrollOffset());
}

OneUiWidget* oneui_panel_create(void) {
    return wrap(std::make_shared<oneui::Panel>());
}

void oneui_panel_set_content(OneUiWidget* panel, OneUiWidget* child) {
    auto* nativePanel = asWidget<oneui::Panel>(panel);
    if (!nativePanel || !child || !child->widget) {
        return;
    }
    nativePanel->setContent(child->widget);
}

void oneui_panel_set_background(OneUiWidget* panel, unsigned char r, unsigned char g, unsigned char b, unsigned char a) {
    auto* nativePanel = asWidget<oneui::Panel>(panel);
    if (!nativePanel) {
        return;
    }
    nativePanel->setBackground(oneui::Color{r, g, b, a});
}

void oneui_panel_set_border(OneUiWidget* panel, unsigned char r, unsigned char g, unsigned char b, unsigned char a, float width) {
    auto* nativePanel = asWidget<oneui::Panel>(panel);
    if (!nativePanel) {
        return;
    }
    nativePanel->setBorder(oneui::Color{r, g, b, a});
    nativePanel->setBorderWidth(width);
}

void oneui_panel_set_radius(OneUiWidget* panel, float radius) {
    auto* nativePanel = asWidget<oneui::Panel>(panel);
    if (!nativePanel) {
        return;
    }
    nativePanel->setRadius(radius);
}

void oneui_panel_set_padding(OneUiWidget* panel, OneUiInsets insets) {
    auto* nativePanel = asWidget<oneui::Panel>(panel);
    if (!nativePanel) {
        return;
    }
    nativePanel->setPadding(toNativeInsets(insets));
}

void oneui_panel_set_shadow(
    OneUiWidget* panel,
    unsigned char r,
    unsigned char g,
    unsigned char b,
    unsigned char a,
    float offset_x,
    float offset_y,
    float blur_radius,
    float spread_radius) {
    auto* nativePanel = asWidget<oneui::Panel>(panel);
    if (!nativePanel) {
        return;
    }
    nativePanel->setShadow(oneui::BoxShadow{
        oneui::Color{r, g, b, a},
        oneui::Point{offset_x, offset_y},
        blur_radius,
        spread_radius});
}

OneUiWidget* oneui_label_create(const wchar_t* text) {
    return wrap(std::make_shared<oneui::Label>(wideOrEmpty(text)));
}

OneUiWidget* oneui_label_create_utf8(OneUiUtf8String text) {
    return wrap(std::make_shared<oneui::Label>(utf8OrEmpty(text)));
}

void oneui_label_set_text(OneUiWidget* label, const wchar_t* text) {
    auto* nativeLabel = asWidget<oneui::Label>(label);
    if (!nativeLabel) {
        return;
    }
    nativeLabel->setText(wideOrEmpty(text));
}

void oneui_label_set_text_utf8(OneUiWidget* label, OneUiUtf8String text) {
    auto* nativeLabel = asWidget<oneui::Label>(label);
    if (!nativeLabel) {
        return;
    }
    nativeLabel->setText(utf8OrEmpty(text));
}

void oneui_label_set_color(OneUiWidget* label, unsigned char r, unsigned char g, unsigned char b, unsigned char a) {
    auto* nativeLabel = asWidget<oneui::Label>(label);
    if (!nativeLabel) {
        return;
    }
    nativeLabel->setColor(toNativeColor(OneUiColor{r, g, b, a}));
}

void oneui_label_set_font_size(OneUiWidget* label, float font_size) {
    auto* nativeLabel = asWidget<oneui::Label>(label);
    if (!nativeLabel) {
        return;
    }
    nativeLabel->setFontSize(font_size);
}

void oneui_label_set_font_weight(OneUiWidget* label, int font_weight) {
    auto* nativeLabel = asWidget<oneui::Label>(label);
    if (!nativeLabel) {
        return;
    }
    nativeLabel->setFontWeight(std::clamp(font_weight, 100, 900));
}

void oneui_label_set_align(OneUiWidget* label, int align) {
    auto* nativeLabel = asWidget<oneui::Label>(label);
    if (!nativeLabel) {
        return;
    }
    switch (align) {
    case 1:
        nativeLabel->setAlign(oneui::TextAlign::Center);
        break;
    case 2:
        nativeLabel->setAlign(oneui::TextAlign::Right);
        break;
    default:
        nativeLabel->setAlign(oneui::TextAlign::Left);
        break;
    }
}

OneUiWidget* oneui_icon_create(int symbol) {
    const auto clamped = std::clamp(symbol, 0, static_cast<int>(oneui::IconSymbol::Trash));
    return wrap(std::make_shared<oneui::IconView>(static_cast<oneui::IconSymbol>(clamped)));
}

void oneui_icon_set_symbol(OneUiWidget* icon, int symbol) {
    auto* nativeIcon = asWidget<oneui::IconView>(icon);
    if (!nativeIcon) {
        return;
    }
    const auto clamped = std::clamp(symbol, 0, static_cast<int>(oneui::IconSymbol::Trash));
    nativeIcon->setSymbol(static_cast<oneui::IconSymbol>(clamped));
}

void oneui_icon_set_color(OneUiWidget* icon, unsigned char r, unsigned char g, unsigned char b, unsigned char a) {
    auto* nativeIcon = asWidget<oneui::IconView>(icon);
    if (!nativeIcon) {
        return;
    }
    nativeIcon->setColor(oneui::Color{r, g, b, a});
}

void oneui_icon_set_accent(OneUiWidget* icon, unsigned char r, unsigned char g, unsigned char b, unsigned char a) {
    auto* nativeIcon = asWidget<oneui::IconView>(icon);
    if (!nativeIcon) {
        return;
    }
    nativeIcon->setAccent(oneui::Color{r, g, b, a});
}

void oneui_icon_set_stroke_width(OneUiWidget* icon, float width) {
    auto* nativeIcon = asWidget<oneui::IconView>(icon);
    if (!nativeIcon) {
        return;
    }
    nativeIcon->setStrokeWidth(width);
}

OneUiWidget* oneui_icon_button_create(int symbol) {
    const auto clamped = std::clamp(symbol, 0, static_cast<int>(oneui::IconSymbol::Trash));
    return wrap(std::make_shared<oneui::IconButton>(static_cast<oneui::IconSymbol>(clamped)));
}

void oneui_icon_button_set_symbol(OneUiWidget* icon_button, int symbol) {
    auto* nativeIconButton = asWidget<oneui::IconButton>(icon_button);
    if (!nativeIconButton) {
        return;
    }
    const auto clamped = std::clamp(symbol, 0, static_cast<int>(oneui::IconSymbol::Trash));
    nativeIconButton->setSymbol(static_cast<oneui::IconSymbol>(clamped));
}

void oneui_icon_button_set_on_click(OneUiWidget* icon_button, OneUiVoidCallback callback, void* user_data) {
    auto* nativeIconButton = asWidget<oneui::IconButton>(icon_button);
    if (!nativeIconButton) {
        return;
    }
    if (!callback) {
        nativeIconButton->setOnClick(nullptr);
        return;
    }
    nativeIconButton->setOnClick([callback, user_data] {
        callback(user_data);
    });
}

OneUiWidget* oneui_switch_create(const wchar_t* text) {
    return wrap(std::make_shared<oneui::Switch>(wideOrEmpty(text)));
}

void oneui_switch_set_text(OneUiWidget* switch_widget, const wchar_t* text) {
    auto* nativeSwitch = asWidget<oneui::Switch>(switch_widget);
    if (!nativeSwitch) {
        return;
    }
    nativeSwitch->setText(wideOrEmpty(text));
}

void oneui_switch_set_checked(OneUiWidget* switch_widget, int checked) {
    auto* nativeSwitch = asWidget<oneui::Switch>(switch_widget);
    if (!nativeSwitch) {
        return;
    }
    nativeSwitch->setChecked(checked != 0);
}

int oneui_switch_checked(OneUiWidget* switch_widget) {
    auto* nativeSwitch = asWidget<oneui::Switch>(switch_widget);
    if (!nativeSwitch) {
        return 0;
    }
    return nativeSwitch->checked() ? 1 : 0;
}

void oneui_switch_set_on_changed(OneUiWidget* switch_widget, OneUiBoolCallback callback, void* user_data) {
    auto* nativeSwitch = asWidget<oneui::Switch>(switch_widget);
    if (!nativeSwitch) {
        return;
    }
    if (!callback) {
        nativeSwitch->setOnChanged(nullptr);
        return;
    }
    nativeSwitch->setOnChanged([callback, user_data](bool checked) {
        callback(checked ? 1 : 0, user_data);
    });
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
    const auto clamped = std::clamp(symbol, 0, static_cast<int>(oneui::IconSymbol::Trash));
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

OneUiWidget* oneui_nav_item_create(const wchar_t* text, int symbol, int selected) {
    auto widget = std::make_shared<oneui::NavItem>(
        wideOrEmpty(text),
        static_cast<oneui::IconSymbol>(symbol));
    widget->setSelected(selected != 0);
    return wrap(std::move(widget));
}

void oneui_nav_item_set_text(OneUiWidget* nav_item, const wchar_t* text) {
    auto* nativeNavItem = asWidget<oneui::NavItem>(nav_item);
    if (!nativeNavItem) {
        return;
    }
    nativeNavItem->setText(wideOrEmpty(text));
}

void oneui_nav_item_set_symbol(OneUiWidget* nav_item, int symbol) {
    auto* nativeNavItem = asWidget<oneui::NavItem>(nav_item);
    if (!nativeNavItem) {
        return;
    }
    nativeNavItem->setSymbol(static_cast<oneui::IconSymbol>(symbol));
}

void oneui_nav_item_set_selected(OneUiWidget* nav_item, int selected) {
    auto* nativeNavItem = asWidget<oneui::NavItem>(nav_item);
    if (!nativeNavItem) {
        return;
    }
    nativeNavItem->setSelected(selected != 0);
}

void oneui_nav_item_set_on_click(OneUiWidget* nav_item, OneUiVoidCallback callback, void* user_data) {
    auto* nativeNavItem = asWidget<oneui::NavItem>(nav_item);
    if (!nativeNavItem) {
        return;
    }
    if (!callback) {
        nativeNavItem->setOnClick(nullptr);
        return;
    }
    nativeNavItem->setOnClick([callback, user_data] {
        callback(user_data);
    });
}

OneUiWidget* oneui_badge_create(const wchar_t* text, int variant) {
    return wrap(std::make_shared<oneui::Badge>(
        wideOrEmpty(text),
        static_cast<oneui::BadgeVariant>(variant)));
}

void oneui_badge_set_text(OneUiWidget* badge, const wchar_t* text) {
    auto* nativeBadge = asWidget<oneui::Badge>(badge);
    if (!nativeBadge) {
        return;
    }
    nativeBadge->setText(wideOrEmpty(text));
}

void oneui_badge_set_variant(OneUiWidget* badge, int variant) {
    auto* nativeBadge = asWidget<oneui::Badge>(badge);
    if (!nativeBadge) {
        return;
    }
    nativeBadge->setVariant(static_cast<oneui::BadgeVariant>(variant));
}

OneUiWidget* oneui_icon_badge_create(int symbol) {
    const auto clamped = std::clamp(symbol, 0, static_cast<int>(oneui::IconSymbol::Trash));
    return wrap(std::make_shared<oneui::IconBadge>(static_cast<oneui::IconSymbol>(clamped)));
}

void oneui_icon_badge_set_symbol(OneUiWidget* icon_badge, int symbol) {
    auto* nativeIconBadge = asWidget<oneui::IconBadge>(icon_badge);
    if (!nativeIconBadge) {
        return;
    }
    const auto clamped = std::clamp(symbol, 0, static_cast<int>(oneui::IconSymbol::Trash));
    nativeIconBadge->setSymbol(static_cast<oneui::IconSymbol>(clamped));
}

void oneui_icon_badge_set_accent(OneUiWidget* icon_badge, unsigned char r, unsigned char g, unsigned char b, unsigned char a) {
    auto* nativeIconBadge = asWidget<oneui::IconBadge>(icon_badge);
    if (!nativeIconBadge) {
        return;
    }
    nativeIconBadge->setAccent(oneui::Color{r, g, b, a});
}

void oneui_icon_badge_set_stroke_width(OneUiWidget* icon_badge, float width) {
    auto* nativeIconBadge = asWidget<oneui::IconBadge>(icon_badge);
    if (!nativeIconBadge) {
        return;
    }
    nativeIconBadge->setStrokeWidth(width);
}

OneUiWidget* oneui_menu_create(void) {
    return wrap(std::make_shared<oneui::Menu>());
}

void oneui_menu_add_header(OneUiWidget* menu, const wchar_t* title, const wchar_t* subtitle) {
    auto* nativeMenu = asWidget<oneui::Menu>(menu);
    if (!nativeMenu) {
        return;
    }
    nativeMenu->addHeader(wideOrEmpty(title), wideOrEmpty(subtitle));
}

int oneui_menu_add_item(OneUiWidget* menu, const wchar_t* text, int icon_symbol, int danger) {
    auto* nativeMenu = asWidget<oneui::Menu>(menu);
    if (!nativeMenu) {
        return -1;
    }
    std::optional<oneui::IconSymbol> icon;
    if (icon_symbol >= 0) {
        const auto clamped = std::clamp(icon_symbol, 0, static_cast<int>(oneui::IconSymbol::Trash));
        icon = static_cast<oneui::IconSymbol>(clamped);
    }
    return nativeMenu->addItem(wideOrEmpty(text), icon, danger != 0);
}

void oneui_menu_add_separator(OneUiWidget* menu) {
    auto* nativeMenu = asWidget<oneui::Menu>(menu);
    if (!nativeMenu) {
        return;
    }
    nativeMenu->addSeparator();
}

void oneui_menu_set_item_disabled(OneUiWidget* menu, int index, int disabled) {
    auto* nativeMenu = asWidget<oneui::Menu>(menu);
    if (!nativeMenu) {
        return;
    }
    nativeMenu->setItemDisabled(index, disabled != 0);
}

float oneui_menu_preferred_height(OneUiWidget* menu) {
    auto* nativeMenu = asWidget<oneui::Menu>(menu);
    if (!nativeMenu) {
        return 0.0f;
    }
    return nativeMenu->preferredHeight();
}

void oneui_menu_set_on_activated(OneUiWidget* menu, OneUiIntCallback callback, void* user_data) {
    auto* nativeMenu = asWidget<oneui::Menu>(menu);
    if (!nativeMenu) {
        return;
    }
    if (!callback) {
        nativeMenu->setOnItemActivated(nullptr);
        return;
    }
    nativeMenu->setOnItemActivated([callback, user_data](int value) {
        callback(value, user_data);
    });
}

OneUiWidget* oneui_dialog_create(const wchar_t* title, const wchar_t* subtitle) {
    return wrap(std::make_shared<oneui::Dialog>(wideOrEmpty(title), wideOrEmpty(subtitle)));
}

void oneui_dialog_set_title(OneUiWidget* dialog, const wchar_t* title) {
    auto* nativeDialog = asWidget<oneui::Dialog>(dialog);
    if (!nativeDialog) {
        return;
    }
    nativeDialog->setTitle(wideOrEmpty(title));
}

void oneui_dialog_set_subtitle(OneUiWidget* dialog, const wchar_t* subtitle) {
    auto* nativeDialog = asWidget<oneui::Dialog>(dialog);
    if (!nativeDialog) {
        return;
    }
    nativeDialog->setSubtitle(wideOrEmpty(subtitle));
}

void oneui_dialog_set_icon(OneUiWidget* dialog, int symbol) {
    auto* nativeDialog = asWidget<oneui::Dialog>(dialog);
    if (!nativeDialog) {
        return;
    }
    if (symbol < 0) {
        nativeDialog->clearIconSymbol();
        return;
    }
    const auto clamped = std::clamp(symbol, 0, static_cast<int>(oneui::IconSymbol::Trash));
    nativeDialog->setIconSymbol(static_cast<oneui::IconSymbol>(clamped));
}

void oneui_dialog_set_close_visible(OneUiWidget* dialog, int visible) {
    auto* nativeDialog = asWidget<oneui::Dialog>(dialog);
    if (!nativeDialog) {
        return;
    }
    nativeDialog->setCloseVisible(visible != 0);
}

void oneui_dialog_set_on_close(OneUiWidget* dialog, OneUiVoidCallback callback, void* user_data) {
    auto* nativeDialog = asWidget<oneui::Dialog>(dialog);
    if (!nativeDialog) {
        return;
    }
    nativeDialog->setOnClose(callback ? std::function<void()>([callback, user_data] {
        callback(user_data);
    }) : nullptr);
}

void oneui_dialog_set_content(OneUiWidget* dialog, OneUiWidget* child) {
    auto* nativeDialog = asWidget<oneui::Dialog>(dialog);
    if (!nativeDialog) {
        return;
    }
    nativeDialog->setContent(child ? child->widget : nullptr);
}

void oneui_dialog_set_actions(OneUiWidget* dialog, OneUiWidget* child) {
    auto* nativeDialog = asWidget<oneui::Dialog>(dialog);
    if (!nativeDialog) {
        return;
    }
    nativeDialog->setActions(child ? child->widget : nullptr);
}

OneUiWidget* oneui_segmented_control_create() {
    return wrap(std::make_shared<oneui::Tabs>());
}

void oneui_segmented_control_set_items(OneUiWidget* segmented_control, const wchar_t* items) {
    if (auto* nativeTabs = asWidget<oneui::Tabs>(segmented_control)) {
        nativeTabs->setItems(splitWideItems(items));
    }
}

void oneui_segmented_control_set_selected_index(OneUiWidget* segmented_control, int index) {
    if (auto* nativeTabs = asWidget<oneui::Tabs>(segmented_control)) {
        nativeTabs->setSelectedIndex(index);
    }
}

int oneui_segmented_control_selected_index(OneUiWidget* segmented_control) {
    if (auto* nativeTabs = asWidget<oneui::Tabs>(segmented_control)) {
        return nativeTabs->selectedIndex();
    }
    return 0;
}

void oneui_segmented_control_set_on_changed(OneUiWidget* segmented_control, OneUiIntCallback callback, void* user_data) {
    auto* nativeTabs = asWidget<oneui::Tabs>(segmented_control);
    if (!nativeTabs) {
        return;
    }
    if (!callback) {
        nativeTabs->setOnChanged(nullptr);
        return;
    }
    nativeTabs->setOnChanged([callback, user_data](int value) {
        callback(value, user_data);
    });
}

OneUiWidget* oneui_tabs_create() {
    return wrap(std::make_shared<oneui::Tabs>());
}

void oneui_tabs_set_items_utf8(
    OneUiWidget* tabs,
    const OneUiUtf8String* items,
    std::size_t count) {
    auto* nativeTabs = asWidget<oneui::Tabs>(tabs);
    if (!nativeTabs) {
        return;
    }
    std::vector<std::wstring> values;
    if (items && count > 0) {
        values.reserve(count);
        for (std::size_t index = 0; index < count; ++index) {
            values.push_back(utf8OrEmpty(items[index]));
        }
    }
    nativeTabs->setItems(std::move(values));
}

void oneui_tabs_set_selected_index(OneUiWidget* tabs, int index) {
    if (auto* nativeTabs = asWidget<oneui::Tabs>(tabs)) {
        nativeTabs->setSelectedIndex(index);
    }
}

int oneui_tabs_selected_index(OneUiWidget* tabs) {
    if (auto* nativeTabs = asWidget<oneui::Tabs>(tabs)) {
        return nativeTabs->selectedIndex();
    }
    return 0;
}

void oneui_tabs_set_on_changed(OneUiWidget* tabs, OneUiIntCallback callback, void* user_data) {
    auto* nativeTabs = asWidget<oneui::Tabs>(tabs);
    if (!nativeTabs) {
        return;
    }
    if (!callback) {
        nativeTabs->setOnChanged(nullptr);
        return;
    }
    nativeTabs->setOnChanged([callback, user_data](int value) {
        callback(value, user_data);
    });
}

OneUiWidget* oneui_select_create() {
    return wrap(std::make_shared<oneui::Select>());
}

void oneui_select_set_items_utf8(
    OneUiWidget* select,
    const OneUiUtf8String* items,
    std::size_t count) {
    auto* nativeSelect = asWidget<oneui::Select>(select);
    if (!nativeSelect) {
        return;
    }
    std::vector<std::wstring> values;
    if (items && count > 0) {
        values.reserve(count);
        for (std::size_t index = 0; index < count; ++index) {
            values.push_back(utf8OrEmpty(items[index]));
        }
    }
    nativeSelect->setItems(std::move(values));
}

void oneui_select_set_selected_index(OneUiWidget* select, int index) {
    if (auto* nativeSelect = asWidget<oneui::Select>(select)) {
        nativeSelect->setSelectedIndex(index);
    }
}

int oneui_select_selected_index(OneUiWidget* select) {
    if (auto* nativeSelect = asWidget<oneui::Select>(select)) {
        return nativeSelect->selectedIndex();
    }
    return -1;
}

void oneui_select_set_on_changed(
    OneUiWidget* select,
    OneUiIntCallback callback,
    void* user_data) {
    auto* nativeSelect = asWidget<oneui::Select>(select);
    if (!nativeSelect) {
        return;
    }
    if (!callback) {
        nativeSelect->setOnChanged(nullptr);
        return;
    }
    nativeSelect->setOnChanged([callback, user_data](int value) {
        callback(value, user_data);
    });
}

OneUiWidget* oneui_list_create() {
    return wrap(std::make_shared<oneui::List>());
}

void oneui_list_set_items(OneUiWidget* list, const wchar_t* items) {
    auto* nativeList = asWidget<oneui::List>(list);
    if (!nativeList) {
        return;
    }
    nativeList->setItems(splitWideListItems(items));
}

void oneui_list_set_items_utf8(
    OneUiWidget* list,
    const OneUiListItemUtf8* items,
    std::size_t count) {
    auto* nativeList = asWidget<oneui::List>(list);
    if (!nativeList) {
        return;
    }
    nativeList->setItems(listItemsFromUtf8(items, count));
}

void oneui_list_set_selection_required(OneUiWidget* list, int required) {
    auto* nativeList = asWidget<oneui::List>(list);
    if (!nativeList) {
        return;
    }
    nativeList->setSelectionRequired(required != 0);
}

void oneui_list_set_selected_index(OneUiWidget* list, int index) {
    auto* nativeList = asWidget<oneui::List>(list);
    if (!nativeList) {
        return;
    }
    nativeList->setSelectedIndex(index);
}

int oneui_list_selected_index(OneUiWidget* list) {
    auto* nativeList = asWidget<oneui::List>(list);
    if (!nativeList) {
        return 0;
    }
    return nativeList->selectedIndex();
}

void oneui_list_set_on_changed(OneUiWidget* list, OneUiIntCallback callback, void* user_data) {
    auto* nativeList = asWidget<oneui::List>(list);
    if (!nativeList) {
        return;
    }
    if (!callback) {
        nativeList->setOnChanged(nullptr);
        return;
    }
    nativeList->setOnChanged([callback, user_data](int value) {
        callback(value, user_data);
    });
}

OneUiWidget* oneui_virtual_list_create() {
    return wrap(std::make_shared<oneui::VirtualList>());
}

void oneui_virtual_list_set_items_utf8(
    OneUiWidget* list,
    const OneUiListItemUtf8* items,
    std::size_t count) {
    if (auto* nativeList = asWidget<oneui::VirtualList>(list)) {
        nativeList->setItems(listItemsFromUtf8(items, count));
    }
}

int oneui_virtual_list_update_item_utf8(
    OneUiWidget* list,
    std::size_t index,
    const OneUiListItemUtf8* item) {
    auto* nativeList = asWidget<oneui::VirtualList>(list);
    if (!nativeList || !item) {
        return 0;
    }
    return nativeList->updateItem(index, oneui::ListItem{
        utf8OrEmpty(item->title),
        utf8OrEmpty(item->detail),
    }) ? 1 : 0;
}

void oneui_virtual_list_set_selected_index(OneUiWidget* list, int index) {
    if (auto* nativeList = asWidget<oneui::VirtualList>(list)) {
        nativeList->setSelectedIndex(index);
    }
}

int oneui_virtual_list_selected_index(OneUiWidget* list) {
    if (const auto* nativeList = asWidget<oneui::VirtualList>(list)) {
        return nativeList->selectedIndex();
    }
    return 0;
}

void oneui_virtual_list_set_selection_mode(OneUiWidget* list, int mode) {
    if (auto* nativeList = asWidget<oneui::VirtualList>(list)) {
        nativeList->setSelectionMode(
            mode == 1 ? oneui::SelectionMode::Multiple : oneui::SelectionMode::Single);
    }
}

void oneui_virtual_list_set_selected_indices(
    OneUiWidget* list,
    const int* indices,
    std::size_t count) {
    auto* nativeList = asWidget<oneui::VirtualList>(list);
    if (!nativeList) {
        return;
    }
    std::vector<int> values;
    if (indices && count > 0) {
        values.assign(indices, indices + count);
    }
    nativeList->setSelectedIndices(std::move(values));
}

std::size_t oneui_virtual_list_selected_indices(
    OneUiWidget* list,
    int* buffer,
    std::size_t buffer_len) {
    auto* nativeList = asWidget<oneui::VirtualList>(list);
    if (!nativeList) {
        return 0;
    }
    const auto& values = nativeList->selectedIndices();
    if (buffer && buffer_len > 0) {
        std::copy_n(values.begin(), std::min(buffer_len, values.size()), buffer);
    }
    return values.size();
}

void oneui_virtual_list_set_row_height(OneUiWidget* list, float height) {
    if (auto* nativeList = asWidget<oneui::VirtualList>(list)) {
        nativeList->setRowHeight(height);
    }
}

void oneui_virtual_list_set_scroll_offset(OneUiWidget* list, float offset) {
    if (auto* nativeList = asWidget<oneui::VirtualList>(list)) {
        nativeList->setScrollOffset(offset);
    }
}

float oneui_virtual_list_scroll_offset(OneUiWidget* list) {
    if (const auto* nativeList = asWidget<oneui::VirtualList>(list)) {
        return nativeList->scrollOffset();
    }
    return 0.0f;
}

float oneui_virtual_list_max_scroll_offset(OneUiWidget* list) {
    if (const auto* nativeList = asWidget<oneui::VirtualList>(list)) {
        return nativeList->maxScrollOffset();
    }
    return 0.0f;
}

void oneui_virtual_list_set_on_changed(OneUiWidget* list, OneUiIntCallback callback, void* user_data) {
    auto* nativeList = asWidget<oneui::VirtualList>(list);
    if (!nativeList) {
        return;
    }
    if (!callback) {
        nativeList->setOnChanged(nullptr);
        return;
    }
    nativeList->setOnChanged([callback, user_data](int value) {
        callback(value, user_data);
    });
}

void oneui_virtual_list_set_on_selection_changed(
    OneUiWidget* list,
    OneUiIntArrayCallback callback,
    void* user_data) {
    auto* nativeList = asWidget<oneui::VirtualList>(list);
    if (!nativeList) {
        return;
    }
    if (!callback) {
        nativeList->setOnSelectionChanged(nullptr);
        return;
    }
    nativeList->setOnSelectionChanged([callback, user_data](const std::vector<int>& values) {
        callback(values.data(), values.size(), user_data);
    });
}

void oneui_virtual_list_set_on_activated(
    OneUiWidget* list,
    OneUiIntCallback callback,
    void* user_data) {
    auto* nativeList = asWidget<oneui::VirtualList>(list);
    if (!nativeList) {
        return;
    }
    nativeList->setOnActivated(callback ? std::function<void(int)>{[callback, user_data](int index) {
        callback(index, user_data);
    }} : nullptr);
}

void oneui_virtual_list_set_on_edit_requested(
    OneUiWidget* list,
    OneUiIntCallback callback,
    void* user_data) {
    auto* nativeList = asWidget<oneui::VirtualList>(list);
    if (!nativeList) {
        return;
    }
    nativeList->setOnEditRequested(callback ? std::function<void(int)>{[callback, user_data](int index) {
        callback(index, user_data);
    }} : nullptr);
}

void oneui_virtual_list_set_on_context_menu_requested(
    OneUiWidget* list,
    OneUiIndexPointCallback callback,
    void* user_data) {
    auto* nativeList = asWidget<oneui::VirtualList>(list);
    if (!nativeList) {
        return;
    }
    nativeList->setOnContextMenuRequested(
        callback ? std::function<void(int, oneui::Point)>{[callback, user_data](int index, oneui::Point point) {
            callback(index, point.x, point.y, user_data);
        }} : nullptr);
}

void oneui_virtual_list_set_reorder_enabled(OneUiWidget* list, int enabled) {
    if (auto* nativeList = asWidget<oneui::VirtualList>(list)) {
        nativeList->setReorderEnabled(enabled != 0);
    }
}

int oneui_virtual_list_reorder_enabled(OneUiWidget* list) {
    if (const auto* nativeList = asWidget<oneui::VirtualList>(list)) {
        return nativeList->reorderEnabled() ? 1 : 0;
    }
    return 0;
}

void oneui_virtual_list_set_on_reorder_requested(
    OneUiWidget* list,
    OneUiReorderRequestedCallback callback,
    void* user_data) {
    auto* nativeList = asWidget<oneui::VirtualList>(list);
    if (!nativeList) {
        return;
    }
    nativeList->setOnReorderRequested(
        callback ? std::function<void(int, int)>{[callback, user_data](int source, int target) {
            callback(source, target, user_data);
        }} : nullptr);
}

OneUiWidget* oneui_tree_view_create() {
    return wrap(std::make_shared<oneui::TreeView>());
}

void oneui_tree_view_set_items_utf8(
    OneUiWidget* tree_view,
    const OneUiTreeItemUtf8* items,
    std::size_t count) {
    if (auto* nativeTree = asWidget<oneui::TreeView>(tree_view)) {
        nativeTree->setItems(treeItemsFromUtf8(items, count));
    }
}

void oneui_tree_view_set_selected_id_utf8(OneUiWidget* tree_view, OneUiUtf8String id) {
    if (auto* nativeTree = asWidget<oneui::TreeView>(tree_view)) {
        nativeTree->setSelectedId(utf8OrEmpty(id));
    }
}

float oneui_tree_view_content_height(OneUiWidget* tree_view) {
    if (const auto* nativeTree = asWidget<oneui::TreeView>(tree_view)) {
        return nativeTree->contentHeight();
    }
    return 0.0f;
}

std::size_t oneui_tree_view_selected_id_utf8(OneUiWidget* tree_view, char* buffer, std::size_t buffer_len) {
    const auto* nativeTree = asWidget<oneui::TreeView>(tree_view);
    if (!nativeTree) {
        return 0;
    }
    const std::string id = utf8FromWide(nativeTree->selectedId());
    copyUtf8Field(id, buffer, buffer_len);
    return id.size() + 1;
}

void oneui_tree_view_set_on_selection_changed_utf8(
    OneUiWidget* tree_view,
    OneUiUtf8TextCallback callback,
    void* user_data) {
    auto* nativeTree = asWidget<oneui::TreeView>(tree_view);
    if (!nativeTree) {
        return;
    }
    if (!callback) {
        nativeTree->setOnSelectionChanged(nullptr);
        return;
    }
    nativeTree->setOnSelectionChanged([callback, user_data](const std::wstring& id) {
        const std::string utf8 = utf8FromWide(id);
        callback(utf8.data(), utf8.size(), user_data);
    });
}

void oneui_tree_view_set_on_expansion_changed_utf8(
    OneUiWidget* tree_view,
    OneUiTreeExpansionCallback callback,
    void* user_data) {
    auto* nativeTree = asWidget<oneui::TreeView>(tree_view);
    if (!nativeTree) {
        return;
    }
    if (!callback) {
        nativeTree->setOnExpansionChanged(nullptr);
        return;
    }
    nativeTree->setOnExpansionChanged([callback, user_data](const std::wstring& id, bool expanded) {
        const std::string utf8 = utf8FromWide(id);
        callback(utf8.data(), utf8.size(), expanded ? 1 : 0, user_data);
    });
}

void oneui_tree_view_set_reorder_enabled(OneUiWidget* tree_view, int enabled) {
    if (auto* nativeTree = asWidget<oneui::TreeView>(tree_view)) {
        nativeTree->setReorderEnabled(enabled != 0);
    }
}

int oneui_tree_view_reorder_enabled(OneUiWidget* tree_view) {
    if (const auto* nativeTree = asWidget<oneui::TreeView>(tree_view)) {
        return nativeTree->reorderEnabled() ? 1 : 0;
    }
    return 0;
}

void oneui_tree_view_set_on_reorder_requested_utf8(
    OneUiWidget* tree_view,
    OneUiTreeReorderRequestedCallback callback,
    void* user_data) {
    auto* nativeTree = asWidget<oneui::TreeView>(tree_view);
    if (!nativeTree) {
        return;
    }
    nativeTree->setOnReorderRequested(
        callback
            ? std::function<void(const std::wstring&, const std::wstring&)>{
                [callback, user_data](const std::wstring& source, const std::wstring& target) {
                    const std::string sourceUtf8 = utf8FromWide(source);
                    const std::string targetUtf8 = utf8FromWide(target);
                    callback(
                        sourceUtf8.data(),
                        sourceUtf8.size(),
                        targetUtf8.data(),
                        targetUtf8.size(),
                        user_data);
                }}
            : nullptr);
}

OneUiWidget* oneui_table_create() {
    return wrap(std::make_shared<oneui::Table>());
}

void oneui_table_set_columns(OneUiWidget* table, const wchar_t* columns) {
    auto* nativeTable = asWidget<oneui::Table>(table);
    if (!nativeTable) {
        return;
    }
    nativeTable->setColumns(splitWideTableColumns(columns));
}

void oneui_table_set_rows(OneUiWidget* table, const wchar_t* rows) {
    auto* nativeTable = asWidget<oneui::Table>(table);
    if (!nativeTable) {
        return;
    }
    nativeTable->setRows(splitWideTableRows(rows));
}

OneUiWidget* oneui_card_create() {
    return wrap(std::make_shared<oneui::Card>());
}

void oneui_card_set_content(OneUiWidget* card, OneUiWidget* child) {
    auto* nativeCard = asWidget<oneui::Card>(card);
    if (!nativeCard) {
        return;
    }
    nativeCard->setContent(child ? child->widget : nullptr);
}

OneUiWidget* oneui_reorderable_grid_create(void) {
    return wrap(std::make_shared<oneui::ReorderableGrid>());
}

void oneui_reorderable_grid_clear_items(OneUiWidget* grid) {
    if (auto* nativeGrid = asWidget<oneui::ReorderableGrid>(grid)) {
        nativeGrid->clearItems();
    }
}

void oneui_reorderable_grid_add_item_utf8(
    OneUiWidget* grid,
    OneUiUtf8String id,
    OneUiWidget* child) {
    auto* nativeGrid = asWidget<oneui::ReorderableGrid>(grid);
    if (!nativeGrid || !child || !child->widget) {
        return;
    }
    nativeGrid->addItem(oneui::ReorderableGridItem{
        utf8OrEmpty(id), child->widget});
}

int oneui_reorderable_grid_move_item_utf8(
    OneUiWidget* grid,
    OneUiUtf8String source_id,
    int target_index) {
    if (auto* nativeGrid = asWidget<oneui::ReorderableGrid>(grid)) {
        return nativeGrid->moveItem(utf8OrEmpty(source_id), target_index) ? 1 : 0;
    }
    return 0;
}

void oneui_reorderable_grid_set_column_count(OneUiWidget* grid, int columns) {
    if (auto* nativeGrid = asWidget<oneui::ReorderableGrid>(grid)) {
        nativeGrid->setColumnCount(columns);
    }
}

void oneui_reorderable_grid_set_gaps(
    OneUiWidget* grid,
    float column_gap,
    float row_gap) {
    if (auto* nativeGrid = asWidget<oneui::ReorderableGrid>(grid)) {
        nativeGrid->setColumnGap(column_gap);
        nativeGrid->setRowGap(row_gap);
    }
}

void oneui_reorderable_grid_set_item_height(OneUiWidget* grid, float height) {
    if (auto* nativeGrid = asWidget<oneui::ReorderableGrid>(grid)) {
        nativeGrid->setItemHeight(height);
    }
}

float oneui_reorderable_grid_content_height(OneUiWidget* grid) {
    if (const auto* nativeGrid = asWidget<oneui::ReorderableGrid>(grid)) {
        return nativeGrid->contentHeight();
    }
    return 0.0f;
}

void oneui_reorderable_grid_set_reorder_enabled(OneUiWidget* grid, int enabled) {
    if (auto* nativeGrid = asWidget<oneui::ReorderableGrid>(grid)) {
        nativeGrid->setReorderEnabled(enabled != 0);
    }
}

int oneui_reorderable_grid_reorder_enabled(OneUiWidget* grid) {
    if (const auto* nativeGrid = asWidget<oneui::ReorderableGrid>(grid)) {
        return nativeGrid->reorderEnabled() ? 1 : 0;
    }
    return 0;
}

void oneui_reorderable_grid_set_on_reorder_requested_utf8(
    OneUiWidget* grid,
    OneUiGridReorderRequestedCallback callback,
    void* user_data) {
    auto* nativeGrid = asWidget<oneui::ReorderableGrid>(grid);
    if (!nativeGrid) {
        return;
    }
    nativeGrid->setOnReorderRequested(
        callback
            ? std::function<void(const std::wstring&, int)>{
                [callback, user_data](const std::wstring& source, int target) {
                    const std::string sourceUtf8 = utf8FromWide(source);
                    callback(
                        sourceUtf8.data(), sourceUtf8.size(), target, user_data);
                }}
            : nullptr);
}

OneUiWidget* oneui_interactive_surface_create() {
    return wrap(std::make_shared<oneui::InteractiveSurface>());
}

void oneui_interactive_surface_set_content(OneUiWidget* surface, OneUiWidget* child) {
    auto* nativeSurface = asWidget<oneui::InteractiveSurface>(surface);
    if (!nativeSurface) {
        return;
    }
    nativeSurface->setContent(child ? child->widget : nullptr);
}

void oneui_interactive_surface_set_padding(OneUiWidget* surface, OneUiInsets padding) {
    auto* nativeSurface = asWidget<oneui::InteractiveSurface>(surface);
    if (nativeSurface) {
        nativeSurface->setPadding(toNativeInsets(padding));
    }
}

void oneui_interactive_surface_set_style(
    OneUiWidget* surface,
    const OneUiInteractiveSurfaceStyle* style) {
    auto* nativeSurface = asWidget<oneui::InteractiveSurface>(surface);
    if (!nativeSurface || !style) {
        return;
    }
    nativeSurface->setStyle(toInteractiveSurfaceStyle(*style));
}

void oneui_interactive_surface_set_on_click(
    OneUiWidget* surface,
    OneUiVoidCallback callback,
    void* user_data) {
    auto* nativeSurface = asWidget<oneui::InteractiveSurface>(surface);
    if (!nativeSurface) {
        return;
    }
    if (!callback) {
        nativeSurface->setOnClick(nullptr);
        return;
    }
    nativeSurface->setOnClick([callback, user_data] {
        callback(user_data);
    });
}

void oneui_interactive_surface_set_on_pointer_activated(
    OneUiWidget* surface,
    OneUiPointerCallback callback,
    void* user_data) {
    auto* nativeSurface = asWidget<oneui::InteractiveSurface>(surface);
    if (!nativeSurface) {
        return;
    }
    if (!callback) {
        nativeSurface->setOnPointerActivated(nullptr);
        return;
    }
    nativeSurface->setOnPointerActivated([callback, user_data](const oneui::MouseEvent& event) {
        const OneUiPointerEvent value{
            event.position.x,
            event.position.y,
            static_cast<int>(event.button),
            event.clickCount,
            event.shift ? 1 : 0,
            event.control ? 1 : 0,
            event.alt ? 1 : 0,
        };
        callback(&value, user_data);
    });
}

void oneui_interactive_surface_set_on_context_menu_requested(
    OneUiWidget* surface,
    OneUiPointerCallback callback,
    void* user_data) {
    auto* nativeSurface = asWidget<oneui::InteractiveSurface>(surface);
    if (!nativeSurface) {
        return;
    }
    if (!callback) {
        nativeSurface->setOnContextMenuRequested(nullptr);
        return;
    }
    nativeSurface->setOnContextMenuRequested([callback, user_data](const oneui::MouseEvent& event) {
        const OneUiPointerEvent value{
            event.position.x,
            event.position.y,
            static_cast<int>(event.button),
            event.clickCount,
            event.shift ? 1 : 0,
            event.control ? 1 : 0,
            event.alt ? 1 : 0,
        };
        callback(&value, user_data);
    });
}

OneUiWidget* oneui_realtime_frame_view_create(void) {
    auto* wrapper = wrap(std::make_shared<oneui::RealtimeFrameView>());
    if (wrapper) {
        wrapper->tag = "realtime-frame-view";
    }
    return wrapper;
}

void oneui_realtime_frame_view_set_scale_mode(OneUiWidget* frame_view, OneUiVideoScaleMode scale_mode) {
    auto* nativeFrameView = asWidget<oneui::RealtimeFrameView>(frame_view);
    if (!nativeFrameView) {
        return;
    }
    nativeFrameView->setScaleMode(toScaleMode(scale_mode));
}

void oneui_realtime_frame_view_set_background(OneUiWidget* frame_view, unsigned char r, unsigned char g, unsigned char b, unsigned char a) {
    auto* nativeFrameView = asWidget<oneui::RealtimeFrameView>(frame_view);
    if (!nativeFrameView) {
        return;
    }
    nativeFrameView->setBackgroundColor(oneui::Color{r, g, b, a});
}

void oneui_realtime_frame_view_submit_frame(
    OneUiWidget* frame_view,
    const void* pixels,
    int width,
    int height,
    int stride,
    OneUiPixelFormat pixel_format,
    unsigned long long frame_id,
    unsigned long long timestamp_us) {
    auto* nativeFrameView = asWidget<oneui::RealtimeFrameView>(frame_view);
    if (!nativeFrameView) {
        return;
    }
    oneui::VideoFrame frame;
    frame.data = pixels;
    frame.width = width;
    frame.height = height;
    frame.stride = stride;
    frame.format = toPixelFormat(pixel_format);
    frame.frameId = static_cast<std::uint64_t>(frame_id);
    frame.timestampUs = static_cast<std::uint64_t>(timestamp_us);
    nativeFrameView->submitFrame(frame);
}

OneUiWidget* oneui_remote_input_region_create(void) {
    auto* wrapper = wrap(std::make_shared<oneui::RemoteInputRegion>());
    if (wrapper) {
        wrapper->tag = "remote-input-region";
        applyCurrentStyleSheet(wrapper);
    }
    return wrapper;
}

void oneui_remote_input_region_set_remote_size(OneUiWidget* region, float width, float height) {
    if (auto* nativeRegion = asWidget<oneui::RemoteInputRegion>(region)) {
        nativeRegion->setRemoteSize(oneui::Size{width, height});
    }
}

void oneui_remote_input_region_set_scale_mode(OneUiWidget* region, OneUiVideoScaleMode scale_mode) {
    if (auto* nativeRegion = asWidget<oneui::RemoteInputRegion>(region)) {
        nativeRegion->setScaleMode(toRemoteInputScaleMode(scale_mode));
    }
}

void oneui_remote_input_region_set_on_pointer(
    OneUiWidget* region,
    OneUiRemotePointerCallback callback,
    void* user_data) {
    if (auto* nativeRegion = asWidget<oneui::RemoteInputRegion>(region)) {
        if (!callback) {
            nativeRegion->setOnPointer(nullptr);
            return;
        }
        nativeRegion->setOnPointer([callback, user_data](const oneui::RemotePointerEvent& event) {
            OneUiRemotePointerEvent cEvent{};
            cEvent.window_x = event.windowPosition.x;
            cEvent.window_y = event.windowPosition.y;
            cEvent.content_x = event.contentPosition.x;
            cEvent.content_y = event.contentPosition.y;
            cEvent.normalized_x = event.normalizedPosition.x;
            cEvent.normalized_y = event.normalizedPosition.y;
            cEvent.remote_x = event.remotePosition.x;
            cEvent.remote_y = event.remotePosition.y;
            cEvent.button = toCButton(event.button);
            cEvent.pressed = event.pressed ? 1 : 0;
            cEvent.wheel_delta_x = event.wheelDeltaX;
            cEvent.wheel_delta_y = event.wheelDeltaY;
            callback(&cEvent, user_data);
        });
    }
}

void oneui_remote_input_region_set_on_raw_key(
    OneUiWidget* region,
    OneUiRawKeyCallback callback,
    void* user_data) {
    if (auto* nativeRegion = asWidget<oneui::RemoteInputRegion>(region)) {
        if (!callback) {
            nativeRegion->setOnRawKey(nullptr);
            return;
        }
        nativeRegion->setOnRawKey([callback, user_data](const oneui::RawKeyEvent& event) {
            OneUiRawKeyEvent cEvent{};
            cEvent.virtual_key = event.virtualKey;
            cEvent.scan_code = event.scanCode;
            cEvent.pressed = event.pressed ? 1 : 0;
            cEvent.repeat = event.repeat ? 1 : 0;
            cEvent.extended = event.extended ? 1 : 0;
            cEvent.alt = event.alt ? 1 : 0;
            cEvent.ctrl = event.ctrl ? 1 : 0;
            cEvent.shift = event.shift ? 1 : 0;
            cEvent.win = event.win ? 1 : 0;
            callback(&cEvent, user_data);
        });
    }
}

void oneui_remote_input_region_release_all_inputs(OneUiWidget* region) {
    if (auto* nativeRegion = asWidget<oneui::RemoteInputRegion>(region)) {
        nativeRegion->releaseAllInputs();
    }
}

OneUiWidget* oneui_terminal_view_create(void) {
    auto terminal = std::make_shared<oneui::TerminalView>();
    terminal->setClipboard(std::make_shared<oneui::SystemClipboard>());
    auto* wrapper = wrap(std::move(terminal));
    if (wrapper) {
        wrapper->tag = "terminal-view";
        applyCurrentStyleSheet(wrapper);
    }
    return wrapper;
}

void oneui_terminal_view_set_font_size(OneUiWidget* view, float size) {
    if (auto* nativeView = asWidget<oneui::TerminalView>(view)) {
        nativeView->setFontSize(size);
    }
}

void oneui_terminal_view_set_line_height(OneUiWidget* view, float multiplier) {
    if (auto* nativeView = asWidget<oneui::TerminalView>(view)) {
        nativeView->setLineHeight(multiplier);
    }
}

void oneui_terminal_view_set_cursor_style(OneUiWidget* view, OneUiTerminalCursorStyle style) {
    if (auto* nativeView = asWidget<oneui::TerminalView>(view)) {
        const auto nativeStyle = style == OneUiTerminalCursorStyleBar
                                     ? oneui::TerminalCursorStyle::Bar
                                     : (style == OneUiTerminalCursorStyleUnderline
                                            ? oneui::TerminalCursorStyle::Underline
                                            : oneui::TerminalCursorStyle::Block);
        nativeView->setCursorStyle(nativeStyle);
    }
}

void oneui_terminal_view_set_cursor_blinking(OneUiWidget* view, int enabled) {
    if (auto* nativeView = asWidget<oneui::TerminalView>(view)) {
        nativeView->setCursorBlinking(enabled != 0);
    }
}

void oneui_terminal_view_set_copy_on_select(OneUiWidget* view, int enabled) {
    if (auto* nativeView = asWidget<oneui::TerminalView>(view)) {
        nativeView->setCopyOnSelect(enabled != 0);
    }
}

void oneui_terminal_view_set_palette(
    OneUiWidget* view,
    OneUiColor background,
    OneUiColor foreground,
    OneUiColor cursor) {
    if (auto* nativeView = asWidget<oneui::TerminalView>(view)) {
        nativeView->setPalette(toNativeColor(background), toNativeColor(foreground), toNativeColor(cursor));
    }
}

void oneui_terminal_view_set_grid_utf8(
    OneUiWidget* view,
    unsigned short rows,
    unsigned short columns,
    const OneUiTerminalCellUtf8* cells,
    size_t cell_count) {
    auto* nativeView = asWidget<oneui::TerminalView>(view);
    if (!nativeView) {
        return;
    }

    constexpr std::size_t kMaximumCells = 2'000'000;
    const std::size_t requested = static_cast<std::size_t>(rows) * static_cast<std::size_t>(columns);
    if (requested > kMaximumCells) {
        nativeView->setGrid(0, 0, {});
        return;
    }

    const std::size_t copied = cells ? std::min(cell_count, requested) : 0;
    std::vector<oneui::TerminalCell> nativeCells;
    nativeCells.reserve(copied);
    for (std::size_t index = 0; index < copied; ++index) {
        const OneUiTerminalCellUtf8& cell = cells[index];
        nativeCells.push_back(oneui::TerminalCell{
            utf8OrEmpty(cell.text),
            toNativeColor(cell.foreground),
            toNativeColor(cell.background),
            cell.style,
            cell.hyperlink_id,
            toNativeTerminalUnderlineStyle(cell.underline_style),
            toNativeColor(cell.underline_color),
            cell.underline_color_set != 0,
        });
    }
    nativeView->setGrid(rows, columns, std::move(nativeCells));
}

void oneui_terminal_view_update_cells_utf8(
    OneUiWidget* view,
    size_t first_cell,
    const OneUiTerminalCellUtf8* cells,
    size_t cell_count) {
    auto* nativeView = asWidget<oneui::TerminalView>(view);
    if (!nativeView || !cells || cell_count == 0 || first_cell >= nativeView->cellCount()) {
        return;
    }

    const std::size_t copied = std::min(cell_count, nativeView->cellCount() - first_cell);
    std::vector<oneui::TerminalCell> nativeCells;
    nativeCells.reserve(copied);
    for (std::size_t index = 0; index < copied; ++index) {
        const OneUiTerminalCellUtf8& cell = cells[index];
        nativeCells.push_back(oneui::TerminalCell{
            utf8OrEmpty(cell.text),
            toNativeColor(cell.foreground),
            toNativeColor(cell.background),
            cell.style,
            cell.hyperlink_id,
            toNativeTerminalUnderlineStyle(cell.underline_style),
            toNativeColor(cell.underline_color),
            cell.underline_color_set != 0,
        });
    }
    nativeView->updateCells(first_cell, std::move(nativeCells));
}

void oneui_terminal_view_set_cursor(
    OneUiWidget* view,
    unsigned short row,
    unsigned short column,
    int visible) {
    if (auto* nativeView = asWidget<oneui::TerminalView>(view)) {
        nativeView->setCursor(oneui::TerminalCursor{row, column, visible != 0});
    }
}

void oneui_terminal_view_select_all(OneUiWidget* view) {
    if (auto* nativeView = asWidget<oneui::TerminalView>(view)) {
        nativeView->selectAll();
    }
}

void oneui_terminal_view_set_selection(
    OneUiWidget* view,
    unsigned short start_row,
    unsigned short start_column,
    unsigned short end_row,
    unsigned short end_column) {
    if (auto* nativeView = asWidget<oneui::TerminalView>(view)) {
        nativeView->setSelection(start_row, start_column, end_row, end_column);
    }
}

void oneui_terminal_view_clear_selection(OneUiWidget* view) {
    if (auto* nativeView = asWidget<oneui::TerminalView>(view)) {
        nativeView->clearSelection();
    }
}

int oneui_terminal_view_has_selection(OneUiWidget* view) {
    if (auto* nativeView = asWidget<oneui::TerminalView>(view)) {
        return nativeView->hasSelection() ? 1 : 0;
    }
    return 0;
}

std::size_t oneui_terminal_view_get_selected_text_utf8(
    OneUiWidget* view,
    char* buffer,
    std::size_t buffer_len) {
    if (auto* nativeView = asWidget<oneui::TerminalView>(view)) {
        const std::string value = utf8FromWide(nativeView->selectedText());
        copyUtf8Field(value, buffer, buffer_len);
        return value.size() + 1;
    }
    if (buffer && buffer_len > 0) {
        buffer[0] = '\0';
    }
    return 0;
}

void oneui_terminal_view_set_on_text_input_utf8(
    OneUiWidget* view,
    OneUiUtf8TextCallback callback,
    void* user_data) {
    if (auto* nativeView = asWidget<oneui::TerminalView>(view)) {
        if (!callback) {
            nativeView->setOnTextInput(nullptr);
            return;
        }
        nativeView->setOnTextInput([callback, user_data](const std::wstring& text) {
            const std::string utf8 = utf8FromWide(text);
            callback(utf8.data(), utf8.size(), user_data);
        });
    }
}

void oneui_terminal_view_set_on_paste_utf8(
    OneUiWidget* view,
    OneUiUtf8TextCallback callback,
    void* user_data) {
    if (auto* nativeView = asWidget<oneui::TerminalView>(view)) {
        if (!callback) {
            nativeView->setOnPaste(nullptr);
            return;
        }
        nativeView->setOnPaste([callback, user_data](const std::wstring& text) {
            const std::string utf8 = utf8FromWide(text);
            callback(utf8.data(), utf8.size(), user_data);
        });
    }
}

void oneui_terminal_view_set_on_raw_key(
    OneUiWidget* view,
    OneUiRawKeyCallback callback,
    void* user_data) {
    if (auto* nativeView = asWidget<oneui::TerminalView>(view)) {
        if (!callback) {
            nativeView->setOnRawKey(nullptr);
            return;
        }
        nativeView->setOnRawKey([callback, user_data](const oneui::KeyEvent& event) {
            OneUiRawKeyEvent cEvent{};
            cEvent.virtual_key = event.virtualKey;
            cEvent.scan_code = event.scanCode;
            cEvent.pressed = event.pressed ? 1 : 0;
            cEvent.repeat = event.repeat ? 1 : 0;
            cEvent.extended = event.extended ? 1 : 0;
            cEvent.alt = event.alt ? 1 : 0;
            cEvent.ctrl = event.control ? 1 : 0;
            cEvent.shift = event.shift ? 1 : 0;
            cEvent.win = event.win ? 1 : 0;
            callback(&cEvent, user_data);
        });
    }
}

void oneui_terminal_view_set_scroll_rows_per_wheel(OneUiWidget* view, float rows) {
    if (auto* nativeView = asWidget<oneui::TerminalView>(view)) {
        nativeView->setScrollRowsPerWheel(rows);
    }
}

void oneui_terminal_view_set_on_scroll(
    OneUiWidget* view,
    OneUiIntCallback callback,
    void* user_data) {
    if (auto* nativeView = asWidget<oneui::TerminalView>(view)) {
        if (!callback) {
            nativeView->setOnScroll(nullptr);
            return;
        }
        nativeView->setOnScroll([callback, user_data](int rows) {
            callback(rows, user_data);
        });
    }
}

void oneui_terminal_view_set_mouse_reporting(OneUiWidget* view, int enabled) {
    if (auto* nativeView = asWidget<oneui::TerminalView>(view)) {
        nativeView->setMouseReporting(enabled != 0);
    }
}

void oneui_terminal_view_set_on_pointer(
    OneUiWidget* view,
    OneUiTerminalPointerCallback callback,
    void* user_data) {
    if (auto* nativeView = asWidget<oneui::TerminalView>(view)) {
        if (!callback) {
            nativeView->setOnPointer(nullptr);
            return;
        }
        nativeView->setOnPointer([callback, user_data](const oneui::TerminalPointerEvent& event) {
            const OneUiTerminalPointerEvent cEvent{
                toCTerminalPointerAction(event.action),
                toCButton(event.button),
                event.row,
                event.column,
                event.wheelDelta,
                event.shift ? 1 : 0,
                event.control ? 1 : 0,
                event.alt ? 1 : 0,
            };
            callback(&cEvent, user_data);
        });
    }
}

void oneui_terminal_view_set_on_hyperlink(
    OneUiWidget* view,
    OneUiTerminalHyperlinkCallback callback,
    void* user_data) {
    if (auto* nativeView = asWidget<oneui::TerminalView>(view)) {
        if (!callback) {
            nativeView->setOnHyperlink(nullptr);
            return;
        }
        nativeView->setOnHyperlink([callback, user_data](std::uint32_t hyperlinkId) {
            callback(hyperlinkId, user_data);
        });
    }
}

void oneui_terminal_view_set_on_viewport_changed(
    OneUiWidget* view,
    OneUiTerminalViewportCallback callback,
    void* user_data) {
    if (auto* nativeView = asWidget<oneui::TerminalView>(view)) {
        if (!callback) {
            nativeView->setOnViewportChanged(nullptr);
            return;
        }
        nativeView->setOnViewportChanged([callback, user_data](oneui::TerminalViewport viewport) {
            callback(viewport.rows, viewport.columns, user_data);
        });
    }
}

void oneui_terminal_view_set_on_focus_changed(
    OneUiWidget* view,
    OneUiBoolCallback callback,
    void* user_data) {
    if (auto* nativeView = asWidget<oneui::TerminalView>(view)) {
        if (!callback) {
            nativeView->setOnFocusChanged(nullptr);
            return;
        }
        nativeView->setOnFocusChanged([callback, user_data](bool focused) {
            callback(focused ? 1 : 0, user_data);
        });
    }
}

OneUiWidget* oneui_tile_create(const wchar_t* title, const wchar_t* subtitle) {
    return wrap(std::make_shared<oneui::Tile>(wideOrEmpty(title), wideOrEmpty(subtitle)));
}

void oneui_tile_set_title(OneUiWidget* tile, const wchar_t* title) {
    if (auto* nativeTile = asWidget<oneui::Tile>(tile)) {
        nativeTile->setTitle(wideOrEmpty(title));
    }
}

void oneui_tile_set_subtitle(OneUiWidget* tile, const wchar_t* subtitle) {
    if (auto* nativeTile = asWidget<oneui::Tile>(tile)) {
        nativeTile->setSubtitle(wideOrEmpty(subtitle));
    }
}

void oneui_tile_set_leading_symbol(OneUiWidget* tile, int symbol) {
    if (auto* nativeTile = asWidget<oneui::Tile>(tile)) {
        const auto clamped = std::clamp(symbol, 0, static_cast<int>(oneui::IconSymbol::Trash));
        nativeTile->setLeadingSymbol(static_cast<oneui::IconSymbol>(clamped));
    }
}

void oneui_tile_clear_leading_symbol(OneUiWidget* tile) {
    if (auto* nativeTile = asWidget<oneui::Tile>(tile)) {
        nativeTile->clearLeadingSymbol();
    }
}

void oneui_tile_set_trailing_symbol(OneUiWidget* tile, int symbol) {
    if (auto* nativeTile = asWidget<oneui::Tile>(tile)) {
        const auto clamped = std::clamp(symbol, 0, static_cast<int>(oneui::IconSymbol::Trash));
        nativeTile->setTrailingSymbol(static_cast<oneui::IconSymbol>(clamped));
    }
}

void oneui_tile_clear_trailing_symbol(OneUiWidget* tile) {
    if (auto* nativeTile = asWidget<oneui::Tile>(tile)) {
        nativeTile->clearTrailingSymbol();
    }
}

void oneui_tile_set_on_click(OneUiWidget* tile, OneUiVoidCallback callback, void* user_data) {
    auto* nativeTile = asWidget<oneui::Tile>(tile);
    if (!nativeTile) {
        return;
    }
    if (!callback) {
        nativeTile->setOnClick(nullptr);
        return;
    }
    nativeTile->setOnClick([callback, user_data] {
        callback(user_data);
    });
}

OneUiWidget* oneui_status_strip_create(const wchar_t* title, const wchar_t* message) {
    return wrap(std::make_shared<oneui::StatusStrip>(wideOrEmpty(title), wideOrEmpty(message)));
}

void oneui_status_strip_set_title(OneUiWidget* status_strip, const wchar_t* title) {
    if (auto* nativeStatusStrip = asWidget<oneui::StatusStrip>(status_strip)) {
        nativeStatusStrip->setTitle(wideOrEmpty(title));
    }
}

void oneui_status_strip_set_message(OneUiWidget* status_strip, const wchar_t* message) {
    if (auto* nativeStatusStrip = asWidget<oneui::StatusStrip>(status_strip)) {
        nativeStatusStrip->setMessage(wideOrEmpty(message));
    }
}

void oneui_status_strip_set_primary_action(OneUiWidget* status_strip, const wchar_t* text) {
    if (auto* nativeStatusStrip = asWidget<oneui::StatusStrip>(status_strip)) {
        nativeStatusStrip->setPrimaryAction(wideOrEmpty(text));
    }
}

void oneui_status_strip_set_secondary_action(OneUiWidget* status_strip, const wchar_t* text) {
    if (auto* nativeStatusStrip = asWidget<oneui::StatusStrip>(status_strip)) {
        nativeStatusStrip->setSecondaryAction(wideOrEmpty(text));
    }
}

void oneui_status_strip_set_on_primary_action(OneUiWidget* status_strip, OneUiVoidCallback callback, void* user_data) {
    auto* nativeStatusStrip = asWidget<oneui::StatusStrip>(status_strip);
    if (!nativeStatusStrip) {
        return;
    }
    nativeStatusStrip->setOnPrimaryAction(callback ? std::function<void()>([callback, user_data] {
        callback(user_data);
    }) : nullptr);
}

void oneui_status_strip_set_on_secondary_action(OneUiWidget* status_strip, OneUiVoidCallback callback, void* user_data) {
    auto* nativeStatusStrip = asWidget<oneui::StatusStrip>(status_strip);
    if (!nativeStatusStrip) {
        return;
    }
    nativeStatusStrip->setOnSecondaryAction(callback ? std::function<void()>([callback, user_data] {
        callback(user_data);
    }) : nullptr);
}

OneUiWidget* oneui_state_view_create(const wchar_t* title, const wchar_t* message) {
    return wrap(std::make_shared<oneui::StateView>(wideOrEmpty(title), wideOrEmpty(message)));
}

void oneui_state_view_set_title(OneUiWidget* state_view, const wchar_t* title) {
    if (auto* nativeStateView = asWidget<oneui::StateView>(state_view)) {
        nativeStateView->setTitle(wideOrEmpty(title));
    }
}

void oneui_state_view_set_message(OneUiWidget* state_view, const wchar_t* message) {
    if (auto* nativeStateView = asWidget<oneui::StateView>(state_view)) {
        nativeStateView->setMessage(wideOrEmpty(message));
    }
}

void oneui_state_view_set_icon(OneUiWidget* state_view, int symbol) {
    if (auto* nativeStateView = asWidget<oneui::StateView>(state_view)) {
        nativeStateView->setIcon(static_cast<oneui::IconSymbol>(symbol));
    }
}

void oneui_state_view_set_action(OneUiWidget* state_view, const wchar_t* text) {
    if (auto* nativeStateView = asWidget<oneui::StateView>(state_view)) {
        nativeStateView->setAction(wideOrEmpty(text));
    }
}

void oneui_state_view_set_on_action(OneUiWidget* state_view, OneUiVoidCallback callback, void* user_data) {
    auto* nativeStateView = asWidget<oneui::StateView>(state_view);
    if (!nativeStateView) {
        return;
    }
    nativeStateView->setOnAction(callback ? std::function<void()>([callback, user_data] {
        callback(user_data);
    }) : nullptr);
}

OneUiWidget* oneui_toast_create(const wchar_t* title, const wchar_t* message) {
    return wrap(std::make_shared<oneui::Toast>(wideOrEmpty(title), wideOrEmpty(message)));
}

void oneui_toast_set_title(OneUiWidget* toast, const wchar_t* title) {
    if (auto* nativeToast = asWidget<oneui::Toast>(toast)) {
        nativeToast->setTitle(wideOrEmpty(title));
    }
}

void oneui_toast_set_message(OneUiWidget* toast, const wchar_t* message) {
    if (auto* nativeToast = asWidget<oneui::Toast>(toast)) {
        nativeToast->setMessage(wideOrEmpty(message));
    }
}

void oneui_toast_set_primary_action(OneUiWidget* toast, const wchar_t* text) {
    if (auto* nativeToast = asWidget<oneui::Toast>(toast)) {
        nativeToast->setPrimaryAction(wideOrEmpty(text));
    }
}

void oneui_toast_set_secondary_action(OneUiWidget* toast, const wchar_t* text) {
    if (auto* nativeToast = asWidget<oneui::Toast>(toast)) {
        nativeToast->setSecondaryAction(wideOrEmpty(text));
    }
}

void oneui_toast_set_icon_symbol(OneUiWidget* toast, int symbol) {
    if (auto* nativeToast = asWidget<oneui::Toast>(toast)) {
        const int maxSymbol = static_cast<int>(oneui::IconSymbol::Trash);
        const int clamped = std::clamp(symbol, 0, maxSymbol);
        nativeToast->setIconSymbol(static_cast<oneui::IconSymbol>(clamped));
    }
}

void oneui_toast_clear_icon_symbol(OneUiWidget* toast) {
    if (auto* nativeToast = asWidget<oneui::Toast>(toast)) {
        nativeToast->clearIconSymbol();
    }
}

void oneui_toast_set_close_visible(OneUiWidget* toast, int visible) {
    if (auto* nativeToast = asWidget<oneui::Toast>(toast)) {
        nativeToast->setCloseVisible(visible != 0);
    }
}

void oneui_toast_set_on_primary_action(OneUiWidget* toast, OneUiVoidCallback callback, void* user_data) {
    auto* nativeToast = asWidget<oneui::Toast>(toast);
    if (!nativeToast) {
        return;
    }
    nativeToast->setOnPrimaryAction(callback ? std::function<void()>([callback, user_data] {
        callback(user_data);
    }) : nullptr);
}

void oneui_toast_set_on_secondary_action(OneUiWidget* toast, OneUiVoidCallback callback, void* user_data) {
    auto* nativeToast = asWidget<oneui::Toast>(toast);
    if (!nativeToast) {
        return;
    }
    nativeToast->setOnSecondaryAction(callback ? std::function<void()>([callback, user_data] {
        callback(user_data);
    }) : nullptr);
}

void oneui_toast_set_on_close(OneUiWidget* toast, OneUiVoidCallback callback, void* user_data) {
    auto* nativeToast = asWidget<oneui::Toast>(toast);
    if (!nativeToast) {
        return;
    }
    nativeToast->setOnClose(callback ? std::function<void()>([callback, user_data] {
        callback(user_data);
    }) : nullptr);
}

OneUiWidget* oneui_radio_group_create() {
    return wrap(std::make_shared<oneui::RadioGroup>());
}

void oneui_radio_group_set_items(OneUiWidget* radio_group, const wchar_t* items) {
    auto* nativeRadio = asWidget<oneui::RadioGroup>(radio_group);
    if (!nativeRadio) {
        return;
    }
    nativeRadio->setItems(splitWideItems(items));
}

void oneui_radio_group_set_selected_index(OneUiWidget* radio_group, int index) {
    auto* nativeRadio = asWidget<oneui::RadioGroup>(radio_group);
    if (!nativeRadio) {
        return;
    }
    nativeRadio->setSelectedIndex(index);
}

int oneui_radio_group_selected_index(OneUiWidget* radio_group) {
    auto* nativeRadio = asWidget<oneui::RadioGroup>(radio_group);
    if (!nativeRadio) {
        return 0;
    }
    return nativeRadio->selectedIndex();
}

void oneui_radio_group_set_orientation(OneUiWidget* radio_group, int orientation) {
    auto* nativeRadio = asWidget<oneui::RadioGroup>(radio_group);
    if (!nativeRadio) {
        return;
    }
    nativeRadio->setOrientation(orientation == 1
        ? oneui::RadioGroup::Orientation::Horizontal
        : oneui::RadioGroup::Orientation::Vertical);
}

void oneui_radio_group_set_on_changed(OneUiWidget* radio_group, OneUiIntCallback callback, void* user_data) {
    auto* nativeRadio = asWidget<oneui::RadioGroup>(radio_group);
    if (!nativeRadio) {
        return;
    }
    if (!callback) {
        nativeRadio->setOnChanged(nullptr);
        return;
    }
    nativeRadio->setOnChanged([callback, user_data](int value) {
        callback(value, user_data);
    });
}

OneUiWidget* oneui_text_field_create(const wchar_t* placeholder) {
    return wrap(std::make_shared<oneui::TextField>(wideOrEmpty(placeholder)));
}

OneUiWidget* oneui_text_field_create_utf8(OneUiUtf8String placeholder) {
    return wrap(std::make_shared<oneui::TextField>(utf8OrEmpty(placeholder)));
}

OneUiWidget* oneui_text_area_create_utf8(OneUiUtf8String placeholder) {
    return wrap(std::make_shared<oneui::TextArea>(utf8OrEmpty(placeholder)));
}

void oneui_text_field_set_text(OneUiWidget* text_field, const wchar_t* text) {
    auto* nativeTextField = asWidget<oneui::TextField>(text_field);
    if (!nativeTextField) {
        return;
    }
    nativeTextField->setText(wideOrEmpty(text));
}

void oneui_text_field_set_text_utf8(OneUiWidget* text_field, OneUiUtf8String text) {
    auto* nativeTextField = asWidget<oneui::TextField>(text_field);
    if (!nativeTextField) {
        return;
    }
    nativeTextField->setText(utf8OrEmpty(text));
}

void oneui_text_field_set_read_only(OneUiWidget* text_field, int read_only) {
    auto* nativeTextField = asWidget<oneui::TextField>(text_field);
    if (!nativeTextField) {
        return;
    }
    nativeTextField->setReadOnly(read_only != 0);
}

void oneui_text_field_set_password_mode(OneUiWidget* text_field, int enabled) {
    auto* nativeTextField = asWidget<oneui::TextField>(text_field);
    if (!nativeTextField) {
        return;
    }
    nativeTextField->setPasswordMode(enabled != 0);
}

void oneui_text_field_set_password_mask(OneUiWidget* text_field, unsigned int codepoint) {
    auto* nativeTextField = asWidget<oneui::TextField>(text_field);
    if (!nativeTextField || codepoint == 0 || codepoint > 0xFFFFu) {
        return;
    }
    nativeTextField->setPasswordMask(static_cast<wchar_t>(codepoint));
}

void oneui_text_field_set_multiline(OneUiWidget* text_field, int multiline) {
    auto* nativeTextField = asWidget<oneui::TextField>(text_field);
    if (!nativeTextField) {
        return;
    }
    nativeTextField->setMultiline(multiline != 0);
}

void oneui_text_field_set_line_height(OneUiWidget* text_field, float line_height) {
    auto* nativeTextField = asWidget<oneui::TextField>(text_field);
    if (!nativeTextField) {
        return;
    }
    nativeTextField->setLineHeight(line_height);
}

void oneui_text_field_set_prefix_icon(OneUiWidget* text_field, int symbol) {
    auto* nativeTextField = asWidget<oneui::TextField>(text_field);
    if (!nativeTextField) {
        return;
    }
    const auto clamped = std::clamp(symbol, 0, static_cast<int>(oneui::IconSymbol::Trash));
    nativeTextField->setPrefixIcon(static_cast<oneui::IconSymbol>(clamped));
}

void oneui_text_field_clear_prefix_icon(OneUiWidget* text_field) {
    auto* nativeTextField = asWidget<oneui::TextField>(text_field);
    if (!nativeTextField) {
        return;
    }
    nativeTextField->setPrefixIcon(std::nullopt);
}

void oneui_text_field_set_suffix_icon(OneUiWidget* text_field, int symbol) {
    auto* nativeTextField = asWidget<oneui::TextField>(text_field);
    if (!nativeTextField) {
        return;
    }
    const auto clamped = std::clamp(symbol, 0, static_cast<int>(oneui::IconSymbol::Trash));
    nativeTextField->setSuffixIcon(static_cast<oneui::IconSymbol>(clamped));
}

void oneui_text_field_clear_suffix_icon(OneUiWidget* text_field) {
    auto* nativeTextField = asWidget<oneui::TextField>(text_field);
    if (!nativeTextField) {
        return;
    }
    nativeTextField->setSuffixIcon(std::nullopt);
}

void oneui_text_field_set_on_changed(OneUiWidget* text_field, OneUiTextCallback callback, void* user_data) {
    auto* nativeTextField = asWidget<oneui::TextField>(text_field);
    if (!nativeTextField) {
        return;
    }
    if (!callback) {
        nativeTextField->setOnChanged(nullptr);
        return;
    }
    nativeTextField->setOnChanged([callback, user_data](const std::wstring& text) {
        callback(text.c_str(), user_data);
    });
}

void oneui_text_field_set_on_changed_utf8(OneUiWidget* text_field, OneUiUtf8TextCallback callback, void* user_data) {
    auto* nativeTextField = asWidget<oneui::TextField>(text_field);
    if (!nativeTextField) {
        return;
    }
    if (!callback) {
        nativeTextField->setOnChanged(nullptr);
        return;
    }
    nativeTextField->setOnChanged([callback, user_data](const std::wstring& text) {
        const std::string utf8 = utf8FromWide(text);
        callback(utf8.data(), utf8.size(), user_data);
    });
}

void oneui_text_field_set_style(OneUiWidget* text_field, const OneUiTextFieldStyle* style) {
    auto* nativeTextField = asWidget<oneui::TextField>(text_field);
    if (!nativeTextField || !style) {
        return;
    }
    nativeTextField->setStyleOverride(toTextFieldOverride(*style));
}

void oneui_text_field_clear_style(OneUiWidget* text_field) {
    auto* nativeTextField = asWidget<oneui::TextField>(text_field);
    if (!nativeTextField) {
        return;
    }
    nativeTextField->clearStyleOverride();
}

OneUiWidget* oneui_search_box_create(const wchar_t* placeholder) {
    auto field = std::make_shared<oneui::TextField>(wideOrEmpty(placeholder));
    field->setPrefixIcon(oneui::IconSymbol::Search);
    field->setSuffixIcon(oneui::IconSymbol::ChevronDown);
    return wrap(std::move(field));
}

OneUiWidget* oneui_search_box_create_utf8(OneUiUtf8String placeholder) {
    auto field = std::make_shared<oneui::TextField>(utf8OrEmpty(placeholder));
    field->setPrefixIcon(oneui::IconSymbol::Search);
    field->setSuffixIcon(oneui::IconSymbol::ChevronDown);
    return wrap(std::move(field));
}

OneUiWidget* oneui_button_create(const wchar_t* text) {
    return wrap(std::make_shared<oneui::Button>(wideOrEmpty(text)));
}

OneUiWidget* oneui_button_create_utf8(OneUiUtf8String text) {
    return wrap(std::make_shared<oneui::Button>(utf8OrEmpty(text)));
}

void oneui_button_set_text(OneUiWidget* button, const wchar_t* text) {
    auto* nativeButton = asWidget<oneui::Button>(button);
    if (!nativeButton) {
        return;
    }
    nativeButton->setText(wideOrEmpty(text));
}

void oneui_button_set_text_utf8(OneUiWidget* button, OneUiUtf8String text) {
    auto* nativeButton = asWidget<oneui::Button>(button);
    if (!nativeButton) {
        return;
    }
    nativeButton->setText(utf8OrEmpty(text));
}

void oneui_button_set_icon(OneUiWidget* button, int symbol) {
    auto* nativeButton = asWidget<oneui::Button>(button);
    if (!nativeButton) {
        return;
    }
    if (symbol < 0) {
        nativeButton->clearIcon();
        return;
    }
    const auto clamped = std::clamp(symbol, 0, static_cast<int>(oneui::IconSymbol::Trash));
    nativeButton->setIcon(static_cast<oneui::IconSymbol>(clamped));
}

void oneui_button_set_content_align(OneUiWidget* button, int align) {
    auto* nativeButton = asWidget<oneui::Button>(button);
    if (!nativeButton) {
        return;
    }
    const auto clamped = std::clamp(align, 0, 2);
    nativeButton->setContentAlign(static_cast<oneui::TextAlign>(clamped));
}

void oneui_button_set_trailing_text_utf8(OneUiWidget* button, OneUiUtf8String text) {
    auto* nativeButton = asWidget<oneui::Button>(button);
    if (!nativeButton) {
        return;
    }
    nativeButton->setTrailingText(utf8OrEmpty(text));
}

void oneui_button_set_variant(OneUiWidget* button, OneUiButtonVariant variant) {
    auto* nativeButton = asWidget<oneui::Button>(button);
    if (!nativeButton) {
        return;
    }
    nativeButton->setVariant(variant == OneUiButtonVariantSecondary
        ? oneui::ButtonVariant::Secondary
        : oneui::ButtonVariant::Primary);
}

void oneui_button_set_on_click(OneUiWidget* button, OneUiVoidCallback callback, void* user_data) {
    auto* nativeButton = asWidget<oneui::Button>(button);
    if (!nativeButton) {
        return;
    }
    if (!callback) {
        nativeButton->setOnClick(nullptr);
        return;
    }
    nativeButton->setOnClick([callback, user_data] {
        callback(user_data);
    });
}

void oneui_button_set_style(OneUiWidget* button, const OneUiButtonStyle* style) {
    auto* nativeButton = asWidget<oneui::Button>(button);
    if (!nativeButton || !style) {
        return;
    }
    nativeButton->setStyleOverride(toButtonOverride(*style));
}

void oneui_button_clear_style(OneUiWidget* button) {
    auto* nativeButton = asWidget<oneui::Button>(button);
    if (!nativeButton) {
        return;
    }
    nativeButton->clearStyleOverride();
}
