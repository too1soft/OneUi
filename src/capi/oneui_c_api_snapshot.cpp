#include "oneui_c_api_controls_internal.h"
#include "oneui_c_api_error.h"
#include "oneui/controls/button.h"
#include "oneui/controls/card.h"
#include "oneui/controls/dialog.h"
#include "oneui/controls/icon_button.h"
#include "oneui/controls/label.h"
#include "oneui/controls/list.h"
#include "oneui/controls/virtual_list.h"
#include "oneui/controls/nav_item.h"
#include "oneui/controls/popup.h"
#include "oneui/controls/progress_bar.h"
#include "oneui/controls/slider.h"
#include "oneui/controls/radio_group.h"
#include "oneui/controls/select.h"
#include "oneui/controls/state_view.h"
#include "oneui/controls/switch.h"
#include "oneui/controls/table.h"
#include "oneui/controls/tabs.h"
#include "oneui/controls/text_field.h"
#include "oneui/controls/terminal_view.h"
#include "oneui/controls/tree_view.h"
#include "oneui/layout/overlay_host.h"
#include "oneui/layout/panel.h"
#include "oneui/layout/scroll_view.h"
#include "oneui/layout/stack.h"
#include "oneui_c_api_snapshot_internal.h"
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

#include <unordered_set>
namespace {

const char* accessibilityRoleName(oneui::AccessibilityRole role) {
    using oneui::AccessibilityRole;
    switch (role) {
    case AccessibilityRole::Button: return "button";
    case AccessibilityRole::Text: return "text";
    case AccessibilityRole::TextBox: return "textbox";
    case AccessibilityRole::CheckBox: return "checkbox";
    case AccessibilityRole::RadioButton: return "radio";
    case AccessibilityRole::RadioGroup: return "radiogroup";
    case AccessibilityRole::ComboBox: return "combobox";
    case AccessibilityRole::Slider: return "slider";
    case AccessibilityRole::ProgressBar: return "progressbar";
    case AccessibilityRole::Tab: return "tab";
    case AccessibilityRole::TabList: return "tablist";
    case AccessibilityRole::List: return "list";
    case AccessibilityRole::ListItem: return "listitem";
    case AccessibilityRole::Table: return "table";
    case AccessibilityRole::Row: return "row";
    case AccessibilityRole::Cell: return "cell";
    case AccessibilityRole::Popup: return "popup";
    case AccessibilityRole::Window: return "window";
    case AccessibilityRole::Custom: return "custom";
    default: return "none";
    }
}

void appendJsonString(std::ostringstream& stream, const std::string& value) {
    static constexpr char hex[] = "0123456789abcdef";
    stream << '"';
    for (const unsigned char character : value) {
        switch (character) {
        case '"': stream << "\\\""; break;
        case '\\': stream << "\\\\"; break;
        case '\b': stream << "\\b"; break;
        case '\f': stream << "\\f"; break;
        case '\n': stream << "\\n"; break;
        case '\r': stream << "\\r"; break;
        case '\t': stream << "\\t"; break;
        default:
            if (character < 0x20) {
                stream << "\\u00" << hex[(character >> 4) & 0x0f] << hex[character & 0x0f];
            } else {
                stream << static_cast<char>(character);
            }
        }
    }
    stream << '"';
}

void appendJsonNumber(std::ostringstream& stream, float value) {
    stream << (std::isfinite(value) ? value : 0.0f);
}

void appendJsonInsets(std::ostringstream& stream, const std::optional<oneui::Insets>& insets) {
    if (!insets) {
        stream << "null";
        return;
    }
    stream << '{';
    stream << "\"top\":"; appendJsonNumber(stream, insets->top);
    stream << ",\"right\":"; appendJsonNumber(stream, insets->right);
    stream << ",\"bottom\":"; appendJsonNumber(stream, insets->bottom);
    stream << ",\"left\":"; appendJsonNumber(stream, insets->left);
    stream << '}';
}

void appendJsonColor(std::ostringstream& stream, const std::optional<oneui::Color>& color) {
    if (!color) {
        stream << "null";
        return;
    }
    stream << '[' << static_cast<unsigned int>(color->r) << ','
           << static_cast<unsigned int>(color->g) << ','
           << static_cast<unsigned int>(color->b) << ','
           << static_cast<unsigned int>(color->a) << ']';
}

std::string debugTagFor(const oneui::Widget* widget, const DebugStyleBinding& binding) {
    if (!binding.tag.empty()) {
        return binding.tag;
    }
    if (dynamic_cast<const oneui::Button*>(widget) ||
        dynamic_cast<const oneui::IconButton*>(widget) ||
        dynamic_cast<const oneui::NavItem*>(widget)) return "button";
    if (dynamic_cast<const oneui::TextField*>(widget)) return "input";
    if (dynamic_cast<const oneui::Label*>(widget)) return "label";
    if (dynamic_cast<const oneui::Select*>(widget)) return "select";
    if (dynamic_cast<const oneui::Switch*>(widget)) return "switch";
    if (dynamic_cast<const oneui::Dialog*>(widget)) return "dialog";
    if (dynamic_cast<const oneui::Popup*>(widget)) return "popup";
    if (dynamic_cast<const oneui::ScrollView*>(widget)) return "scroll-view";
    if (dynamic_cast<const oneui::OverlayHost*>(widget)) return "overlay-host";
    if (dynamic_cast<const oneui::Stack*>(widget)) return "stack";
    if (dynamic_cast<const oneui::Card*>(widget)) return "card";
    if (dynamic_cast<const oneui::Panel*>(widget)) return "panel";
    if (dynamic_cast<const oneui::List*>(widget)) return "list";
    if (dynamic_cast<const oneui::VirtualList*>(widget)) return "virtual-list";
    if (dynamic_cast<const oneui::Table*>(widget)) return "table";
    if (dynamic_cast<const oneui::TreeView*>(widget)) return "tree-view";
    if (dynamic_cast<const oneui::TerminalView*>(widget)) return "terminal-view";
    if (dynamic_cast<const oneui::View*>(widget)) return "view";
    return "widget";
}

std::vector<std::pair<std::shared_ptr<oneui::Widget>, bool>> debugChildrenOf(
    const oneui::Widget* widget,
    bool effectiveVisible) {
    std::vector<std::pair<std::shared_ptr<oneui::Widget>, bool>> children;
    if (const auto* overlayHost = dynamic_cast<const oneui::OverlayHost*>(widget)) {
        if (const auto content = overlayHost->content()) {
            children.emplace_back(content, effectiveVisible && content->visible());
        }
        for (const auto& entry : overlayHost->overlays()) {
            children.emplace_back(
                entry.child,
                effectiveVisible && entry.child && entry.child->visible());
        }
    }
    if (const auto* view = dynamic_cast<const oneui::View*>(widget)) {
        for (const auto& child : view->children()) {
            children.emplace_back(
                child,
                effectiveVisible && child && child->visible());
        }
    }
    if (const auto* popup = dynamic_cast<const oneui::Popup*>(widget)) {
        if (const auto anchor = popup->anchor()) {
            children.emplace_back(anchor, effectiveVisible && anchor->visible());
        }
        if (const auto content = popup->content()) {
            children.emplace_back(
                content,
                effectiveVisible && popup->isOpen() && content->visible());
        }
    }
    return children;
}

void appendDebugTabItems(std::ostringstream& stream, const oneui::Tabs& tabs) {
    const auto& items = tabs.items();
    stream << '[';
    for (std::size_t index = 0; index < items.size(); ++index) {
        if (index > 0) stream << ',';
        const oneui::Rect frame = tabs.itemFrame(static_cast<int>(index));
        stream << "{\"index\":" << index << ",\"title\":";
        appendJsonString(stream, utf8FromWide(items[index]));
        stream << ",\"detail\":\"\",\"selected\":"
               << (tabs.selectedIndex() == static_cast<int>(index) ? "true" : "false");
        stream << ",\"frame\":{\"x\":"; appendJsonNumber(stream, frame.x);
        stream << ",\"y\":"; appendJsonNumber(stream, frame.y);
        stream << ",\"width\":"; appendJsonNumber(stream, frame.width);
        stream << ",\"height\":"; appendJsonNumber(stream, frame.height);
        const oneui::Rect textFrame = tabs.itemTextFrame(static_cast<int>(index));
        stream << "},\"textFrame\":{\"x\":"; appendJsonNumber(stream, textFrame.x);
        stream << ",\"y\":"; appendJsonNumber(stream, textFrame.y);
        stream << ",\"width\":"; appendJsonNumber(stream, textFrame.width);
        stream << ",\"height\":"; appendJsonNumber(stream, textFrame.height);
        stream << "},\"paintFontSize\":";
        appendJsonNumber(stream, tabs.itemPaintFontSize(static_cast<int>(index)));
        stream << ",\"paintFontWeight\":" << tabs.itemPaintFontWeight(static_cast<int>(index));
        const bool editing = tabs.editingIndex() == static_cast<int>(index);
        stream << ",\"editing\":" << (editing ? "true" : "false");
        if (editing) {
            const auto editor = tabs.editorFrame();
            stream << ",\"editorFrame\":{\"x\":"; appendJsonNumber(stream, editor.x);
            stream << ",\"y\":"; appendJsonNumber(stream, editor.y);
            stream << ",\"width\":"; appendJsonNumber(stream, editor.width);
            stream << ",\"height\":"; appendJsonNumber(stream, editor.height);
            stream << '}';
        }
        stream << ",\"textAlign\":\""
               << (tabs.itemTextAlign(static_cast<int>(index)) == oneui::TextAlign::Center
                       ? "center"
                       : "left")
               << "\"}";
    }
    stream << ']';
}

template <typename Item>
void appendDebugListItems(
    std::ostringstream& stream,
    const std::vector<Item>& items,
    const std::function<oneui::Rect(int)>& frameForIndex,
    const std::function<bool(int)>& selectedForIndex) {
    stream << '[';
    for (std::size_t index = 0; index < items.size(); ++index) {
        if (index > 0) stream << ',';
        const oneui::Rect frame = frameForIndex(static_cast<int>(index));
        stream << "{\"index\":" << index << ",\"title\":";
        appendJsonString(stream, utf8FromWide(items[index].title));
        stream << ",\"detail\":";
        appendJsonString(stream, utf8FromWide(items[index].detail));
        stream << ",\"selected\":"
               << (selectedForIndex(static_cast<int>(index)) ? "true" : "false");
        stream << ",\"frame\":{\"x\":"; appendJsonNumber(stream, frame.x);
        stream << ",\"y\":"; appendJsonNumber(stream, frame.y);
        stream << ",\"width\":"; appendJsonNumber(stream, frame.width);
        stream << ",\"height\":"; appendJsonNumber(stream, frame.height);
        stream << "}}";
    }
    stream << ']';
}

void appendDebugTableRows(std::ostringstream& stream, const oneui::Table& table) {
    const auto& rows = table.rows();
    const auto& selected = table.selectedIndices();
    stream << '[';
    for (std::size_t index = 0; index < rows.size(); ++index) {
        if (index > 0) stream << ',';
        const oneui::Rect frame = table.rowFrame(static_cast<int>(index));
        stream << "{\"index\":" << index << ",\"cells\":[";
        for (std::size_t cell = 0; cell < rows[index].size(); ++cell) {
            if (cell > 0) stream << ',';
            appendJsonString(stream, utf8FromWide(rows[index][cell]));
        }
        const bool isSelected = std::find(
            selected.begin(), selected.end(), static_cast<int>(index)) != selected.end();
        stream << "],\"selected\":" << (isSelected ? "true" : "false");
        stream << ",\"frame\":{\"x\":"; appendJsonNumber(stream, frame.x);
        stream << ",\"y\":"; appendJsonNumber(stream, frame.y);
        stream << ",\"width\":"; appendJsonNumber(stream, frame.width);
        stream << ",\"height\":"; appendJsonNumber(stream, frame.height);
        stream << "}}";
    }
    stream << ']';
}

void appendDebugNode(
    std::ostringstream& stream,
    const oneui::Widget* widget,
    std::size_t id,
    std::optional<std::size_t> parentId,
    std::size_t depth,
    bool effectiveVisible,
    const DebugStyleMap& styleBindings,
    bool& firstNode) {
    static const DebugStyleBinding emptyBinding;
    const auto styleBinding = styleBindings.find(widget);
    const DebugStyleBinding& binding = styleBinding == styleBindings.end()
        ? emptyBinding
        : styleBinding->second;
    const oneui::AccessibilityInfo accessibility = widget->accessibilityInfo();
    oneui::StylePseudoMask state = oneui::StyleStateNone;
    if (accessibility.state.disabled) state |= oneui::StyleStateDisabled;
    if (accessibility.state.focused) state |= oneui::StyleStateFocus;
    if (accessibility.state.selected || accessibility.state.checked) state |= oneui::StyleStateSelected;
    const std::string tag = debugTagFor(widget, binding);
    const oneui::StyleBox style = binding.resolved ? *binding.resolved : binding.styleSheet
        ? binding.styleSheet->resolve(oneui::StyleNode{tag, binding.classes, state})
        : oneui::StyleBox{};
    const oneui::Rect frame = widget->frame();
    const oneui::Size preferred = widget->preferredSize();

    if (!firstNode) stream << ',';
    firstNode = false;
    stream << '{';
    stream << "\"id\":" << id << ",\"parentId\":";
    if (parentId) stream << *parentId; else stream << "null";
    stream << ",\"depth\":" << depth << ",\"tag\":";
    appendJsonString(stream, tag);
    stream << ",\"classes\":[";
    for (std::size_t index = 0; index < binding.classes.size(); ++index) {
        if (index > 0) stream << ',';
        appendJsonString(stream, binding.classes[index]);
    }
    stream << "],\"role\":";
    appendJsonString(stream, accessibilityRoleName(accessibility.role));
    stream << ",\"name\":";
    appendJsonString(stream, utf8FromWide(accessibility.name));
    if (const auto* state=dynamic_cast<const oneui::StateView*>(widget)) {
        stream << ",\"stateAction\":"; appendJsonString(stream,utf8FromWide(state->actionText()));
        const auto action=state->actionFrame();
        stream << ",\"stateActionFrame\":{\"x\":"; appendJsonNumber(stream,action.x);
        stream << ",\"y\":"; appendJsonNumber(stream,action.y);
        stream << ",\"width\":"; appendJsonNumber(stream,action.width);
        stream << ",\"height\":"; appendJsonNumber(stream,action.height); stream << "}";
    }
    stream << ",\"description\":";
    appendJsonString(stream, utf8FromWide(accessibility.description));
    stream << ",\"valueLength\":" << accessibility.value.size();
    stream << ",\"visible\":" << (widget->visible() ? "true" : "false");
    stream << ",\"effectiveVisible\":" << (effectiveVisible ? "true" : "false");
    stream << ",\"disabled\":" << (widget->disabled() ? "true" : "false");
    stream << ",\"focused\":" << (widget->focused() ? "true" : "false");
    stream << ",\"checked\":" << (accessibility.state.checked ? "true" : "false");
    stream << ",\"selected\":" << (accessibility.state.selected ? "true" : "false");
    stream << ",\"frame\":{\"x\":"; appendJsonNumber(stream, frame.x);
    stream << ",\"y\":"; appendJsonNumber(stream, frame.y);
    stream << ",\"width\":"; appendJsonNumber(stream, frame.width);
    stream << ",\"height\":"; appendJsonNumber(stream, frame.height);
    stream << "},\"preferred\":{\"width\":"; appendJsonNumber(stream, preferred.width);
    stream << ",\"height\":"; appendJsonNumber(stream, preferred.height);
    stream << "},\"style\":{\"foreground\":"; appendJsonColor(stream, style.foreground);
    stream << ",\"background\":"; appendJsonColor(stream, style.background.color);
    stream << ",\"placeholder\":"; appendJsonColor(stream, style.placeholderColor);
    stream << ",\"border\":"; appendJsonColor(stream, style.borderColor);
    stream << ",\"padding\":"; appendJsonInsets(stream, style.padding);
    stream << ",\"width\":";
    if (style.width) appendJsonNumber(stream, *style.width); else stream << "null";
    stream << ",\"height\":";
    if (style.height) appendJsonNumber(stream, *style.height); else stream << "null";
    stream << ",\"fontSize\":";
    if (style.fontSize) appendJsonNumber(stream, *style.fontSize); else stream << "null";
    stream << ",\"detailFontSize\":";
    if (style.detailFontSize) appendJsonNumber(stream, *style.detailFontSize); else stream << "null";
    stream << ",\"textInset\":";
    if (style.textInset) appendJsonNumber(stream, *style.textInset); else stream << "null";
    stream << ",\"titleOffsetY\":";
    if (style.titleOffsetY) appendJsonNumber(stream, *style.titleOffsetY); else stream << "null";
    stream << ",\"detailOffsetY\":";
    if (style.detailOffsetY) appendJsonNumber(stream, *style.detailOffsetY); else stream << "null";
    stream << '}';
    stream << ",\"text\":";
    if (const auto* label = dynamic_cast<const oneui::Label*>(widget)) {
        appendJsonString(stream, utf8FromWide(label->text()));
        const auto natural = label->naturalTextSize();
        stream << ",\"naturalTextSize\":{\"width\":";
        appendJsonNumber(stream, natural.width);
        stream << ",\"height\":";
        appendJsonNumber(stream, natural.height);
        stream << "},\"paintFontSize\":";
        appendJsonNumber(stream, label->fontSize());
        stream << ",\"paintFontWeight\":" << label->fontWeight();
        stream << ",\"textAlign\":";
        appendJsonString(stream, label->textAlign() == oneui::TextAlign::Right ? "right" :
            label->textAlign() == oneui::TextAlign::Center ? "center" : "left");
    } else {
        stream << "null";
    }
    stream << ",\"rowHeight\":";
    if (const auto* list = dynamic_cast<const oneui::VirtualList*>(widget)) {
        appendJsonNumber(stream, list->rowHeight());
    } else if (const auto* table = dynamic_cast<const oneui::Table*>(widget)) {
        appendJsonNumber(stream, table->rowHeight());
    } else {
        stream << "null";
    }
    stream << ",\"items\":";
    if (const auto* list = dynamic_cast<const oneui::List*>(widget)) {
        appendDebugListItems(
            stream,
            list->items(),
            [list](int index) { return list->itemFrame(index); },
            [list](int index) { return list->selectedIndex() == index; });
    } else if (const auto* list = dynamic_cast<const oneui::VirtualList*>(widget)) {
        const std::vector<int>& selected = list->selectedIndices();
        appendDebugListItems(
            stream,
            list->items(),
            [list](int index) { return list->itemFrame(index); },
            [list, &selected](int index) {
                return list->selectedIndex() == index ||
                    std::find(selected.begin(), selected.end(), index) != selected.end();
            });
    } else if (const auto* table = dynamic_cast<const oneui::Table*>(widget)) {
        appendDebugTableRows(stream, *table);
    } else if (const auto* tabs = dynamic_cast<const oneui::Tabs*>(widget)) {
        appendDebugTabItems(stream, *tabs);
    } else {
        stream << "null";
    }
    stream << '}';
}

std::string buildLayoutSnapshot(const OneUiWindow& window) {
    std::ostringstream stream;
    const oneui::Size client = window.window->clientSize();
    stream << "{\"schemaVersion\":1,\"dpiScale\":";
    appendJsonNumber(stream, window.window->dpiScale());
    stream << ",\"client\":{\"width\":"; appendJsonNumber(stream, client.width);
    stream << ",\"height\":"; appendJsonNumber(stream, client.height);
    stream << "},\"nodes\":[";

    struct PendingNode {
        std::shared_ptr<oneui::Widget> widget;
        std::optional<std::size_t> parentId;
        std::size_t depth = 0;
        bool effectiveVisible = false;
    };
    std::vector<PendingNode> pending;
    if (window.rootContent) {
        pending.push_back(PendingNode{
            window.rootContent,
            std::nullopt,
            0,
            window.rootContent->visible(),
        });
    }
    std::unordered_set<const oneui::Widget*> visited;
    const DebugStyleMap styleBindings = debugStyleBindings();
    bool firstNode = true;
    std::size_t cursor = 0;
    while (cursor < pending.size()) {
        const PendingNode current = pending[cursor];
        const std::size_t nodeId = cursor + 1;
        ++cursor;
        if (!current.widget || !visited.insert(current.widget.get()).second) {
            continue;
        }
        appendDebugNode(
            stream,
            current.widget.get(),
            nodeId,
            current.parentId,
            current.depth,
            current.effectiveVisible,
            styleBindings,
            firstNode);
        for (const auto& [child, childVisible] :
             debugChildrenOf(current.widget.get(), current.effectiveVisible)) {
            if (child && visited.find(child.get()) == visited.end()) {
                pending.push_back(PendingNode{child, nodeId, current.depth + 1, childVisible});
            }
        }
    }
    stream << "]}";
    return stream.str();
}

} // namespace

int oneui_window_layout_snapshot_utf8(
    OneUiWindow* window,
    char* buffer,
    size_t buffer_len,
    size_t* required_len) {
    if (!window || !window->window) {
        if (required_len) {
            *required_len = 0;
        }
        return 0;
    }
    if (!buffer || !window->hasPendingLayoutSnapshot) {
        window->window->prepareLayoutSnapshot();
        window->pendingLayoutSnapshot = buildLayoutSnapshot(*window);
        window->hasPendingLayoutSnapshot = true;
    }
    const std::string& snapshot = window->pendingLayoutSnapshot;
    const size_t required = snapshot.size() + 1;
    if (required_len) {
        *required_len = required;
    }
    if (!buffer || buffer_len < required) {
        return -2;
    }
    std::memcpy(buffer, snapshot.data(), snapshot.size());
    buffer[snapshot.size()] = '\0';
    window->pendingLayoutSnapshot.clear();
    window->hasPendingLayoutSnapshot = false;
    return 1;
}

int oneui_window_capture_frame_png_utf8(OneUiWindow* window, const char* path_utf8) {
    if (!window || !window->window || !path_utf8 || !*path_utf8) return 0;
    return window->window->captureFramePng(utf8OrEmpty({path_utf8, std::strlen(path_utf8)})) ? 1 : 0;
}

int oneui_window_export_developer_report_utf8(OneUiWindow* window, const char* path_utf8) {
    if (!window || !window->window || !path_utf8 || !*path_utf8) return 0;
    return window->window->exportDeveloperReport(utf8OrEmpty({path_utf8, std::strlen(path_utf8)})) ? 1 : 0;
}
