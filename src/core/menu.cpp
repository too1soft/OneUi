#include "oneui/controls/menu.h"

#include "oneui/canvas.h"
#include "oneui/style.h"

#include <algorithm>
#include <utility>

namespace oneui {
namespace {

constexpr float kHeaderHeight = 46.0f;
constexpr float kItemHeight = 34.0f;
constexpr float kSeparatorHeight = 9.0f;
constexpr float kItemIconSize = 15.0f;
constexpr float kItemPaddingX = 10.0f;
constexpr float kItemGap = 8.0f;

// 未接 StyleSheet 时的默认外观：浅色中性（docs/03-style.md）。
StyleSheet defaultMenuSheet() {
    StyleSheet sheet;
    std::string error;
    sheet.addRulesFromCss(R"css(
        menu {
            background: #ffffff;
            border-color: #e2e5ea;
            border-width: 1px;
            border-radius: 12px;
            color: #202124;
            padding: 6px;
            box-shadow: 0px 10px 28px 0px #0f172a29;
        }
        .menu-item {
            background: #00000000;
            border-color: #00000000;
            border-radius: 8px;
            color: #3c4350;
            font-size: 13px;
        }
        .menu-item:hover { background: #f1f3f6; color: #202124; }
        .menu-item:active { background: #e7eaef; color: #202124; }
        .menu-item:disabled { color: #b6bcc7; }
        .menu-item.danger { color: #c8353b; }
        .menu-item.danger:hover { background: #fcebec; color: #c8353b; }
        .menu-item.danger:active { background: #f9dbdc; color: #b02a30; }
        .menu-separator { background: #eef0f4; }
    )css", &error);
    return sheet;
}

const StyleSheet& fallbackSheet() {
    static const StyleSheet sheet = defaultMenuSheet();
    return sheet;
}

} // namespace

Menu::Menu() {
    setPreferredSize(Size{220.0f, 120.0f});
    setAccessibleRole(AccessibilityRole::Popup);
}

void Menu::addHeader(std::wstring title, std::wstring subtitle) {
    Entry entry;
    entry.kind = Entry::Kind::Header;
    entry.title = std::move(title);
    entry.subtitle = std::move(subtitle);
    entries_.push_back(std::move(entry));
    invalidate();
}

int Menu::addItem(std::wstring text, std::optional<IconSymbol> icon, bool danger) {
    Entry entry;
    entry.kind = Entry::Kind::Item;
    entry.title = std::move(text);
    entry.icon = icon;
    entry.danger = danger;
    entry.itemIndex = itemCount_++;
    entries_.push_back(std::move(entry));
    invalidate();
    return itemCount_ - 1;
}

void Menu::addSeparator() {
    Entry entry;
    entry.kind = Entry::Kind::Separator;
    entries_.push_back(std::move(entry));
    invalidate();
}

void Menu::setItemDisabled(int index, bool disabled) {
    for (auto& entry : entries_) {
        if (entry.kind == Entry::Kind::Item && entry.itemIndex == index) {
            entry.disabled = disabled;
            invalidate();
            return;
        }
    }
}

void Menu::setOnItemActivated(std::function<void(int)> callback) {
    onItemActivated_ = std::move(callback);
}

void Menu::setStyleSheet(std::shared_ptr<StyleSheet> sheet, StyleNode node) {
    styleSheet_ = std::move(sheet);
    styleNode_ = std::move(node);
    invalidate();
}

float Menu::entryHeight(const Entry& entry) const {
    switch (entry.kind) {
    case Entry::Kind::Header:
        return kHeaderHeight;
    case Entry::Kind::Separator:
        return kSeparatorHeight;
    case Entry::Kind::Item:
    default:
        return kItemHeight;
    }
}

Insets Menu::surfacePadding() const {
    return resolvedSurface().padding.value_or(Insets{6.0f});
}

float Menu::preferredHeight() const {
    float height = surfacePadding().vertical();
    for (const auto& entry : entries_) {
        height += entryHeight(entry);
    }
    return height;
}

StyleBox Menu::resolvedSurface() const {
    const StyleSheet& sheet = styleSheet_ ? *styleSheet_ : fallbackSheet();
    StyleNode node = styleNode_;
    node.state = disabled() ? StyleStateDisabled : StyleStateNone;
    return sheet.resolve(node);
}

StyleBox Menu::resolvedItemStyle(const Entry& entry, bool hovered, bool pressed) const {
    const StyleSheet& sheet = styleSheet_ ? *styleSheet_ : fallbackSheet();
    StyleNode node{"button", {"menu-item"}, StyleStateNone};
    if (entry.danger) {
        node.classes.push_back("danger");
    }
    if (entry.disabled || disabled()) {
        node.state |= StyleStateDisabled;
    } else if (pressed) {
        node.state |= StyleStateActive;
    } else if (hovered) {
        node.state |= StyleStateHover;
    }
    return sheet.resolve(node);
}

Rect Menu::entryRect(int entryIndex) const {
    const Rect content = frame().inset(surfacePadding());
    float y = content.y;
    for (int i = 0; i < static_cast<int>(entries_.size()); ++i) {
        const float height = entryHeight(entries_[static_cast<std::size_t>(i)]);
        if (i == entryIndex) {
            return Rect{content.x, y, content.width, height};
        }
        y += height;
    }
    return Rect{};
}

int Menu::entryAt(Point point) const {
    for (int i = 0; i < static_cast<int>(entries_.size()); ++i) {
        const Entry& entry = entries_[static_cast<std::size_t>(i)];
        if (entry.kind != Entry::Kind::Item || entry.disabled) {
            continue;
        }
        if (entryRect(i).contains(point)) {
            return i;
        }
    }
    return -1;
}

void Menu::paint(Canvas& canvas) {
    const StyleBox surface = resolvedSurface();
    paintStyleBox(canvas, frame(), surface);

    const Color headerTitle = surface.foreground.value_or(Color{32, 33, 36});
    const Color headerSubtitle{152, 162, 179};

    for (int i = 0; i < static_cast<int>(entries_.size()); ++i) {
        const Entry& entry = entries_[static_cast<std::size_t>(i)];
        const Rect rect = entryRect(i);
        switch (entry.kind) {
        case Entry::Kind::Header: {
            const Rect inner = rect.inset(Insets{6.0f, kItemPaddingX});
            canvas.drawTextStyledEllipsized(entry.title, Rect{inner.x, inner.y, inner.width, 18.0f}, headerTitle, 13.0f, TextAlign::Left, 600);
            canvas.drawTextEllipsized(entry.subtitle, Rect{inner.x, inner.y + 19.0f, inner.width, 14.0f}, headerSubtitle, 11.0f, TextAlign::Left);
            break;
        }
        case Entry::Kind::Separator: {
            const StyleSheet& sheet = styleSheet_ ? *styleSheet_ : fallbackSheet();
            const StyleBox line = sheet.resolve(StyleNode{"separator", {"menu-separator"}, StyleStateNone});
            const Color color = line.background.color.value_or(Color{238, 240, 244});
            canvas.fillRect(Rect{rect.x + 4.0f, rect.y + rect.height / 2.0f, rect.width - 8.0f, 1.0f}, color);
            break;
        }
        case Entry::Kind::Item: {
            const StyleBox item = resolvedItemStyle(entry, hoveredEntry_ == i, pressedEntry_ == i);
            paintStyleBox(canvas, rect, item);
            const Color foreground = item.foreground.value_or(Color{60, 67, 80});
            float textX = rect.x + kItemPaddingX;
            if (entry.icon) {
                const Rect iconRect{
                    rect.x + kItemPaddingX,
                    rect.y + (rect.height - kItemIconSize) / 2.0f,
                    kItemIconSize,
                    kItemIconSize};
                paintIcon(canvas, *entry.icon, iconRect, foreground, Color{0, 0, 0, 0}, 1.5f);
                textX += kItemIconSize + kItemGap;
            }
            canvas.drawTextStyledEllipsized(
                entry.title,
                Rect{textX, rect.y, std::max(0.0f, rect.x + rect.width - kItemPaddingX - textX), rect.height},
                foreground,
                item.fontSize.value_or(13.0f),
                TextAlign::Left,
                item.fontWeight.value_or(400));
            break;
        }
        }
    }
}

bool Menu::onMouseMove(const MouseEvent& event) {
    const int next = interactive() ? entryAt(event.position) : -1;
    if (next == hoveredEntry_) {
        return false;
    }
    hoveredEntry_ = next;
    if (hoveredEntry_ < 0) {
        pressedEntry_ = -1;
    }
    invalidate();
    return true;
}

bool Menu::onMouseDown(const MouseEvent& event) {
    const int entry = interactive() ? entryAt(event.position) : -1;
    if (entry < 0) {
        return false;
    }
    pressedEntry_ = entry;
    setFocused(true);
    invalidate();
    return true;
}

bool Menu::onMouseUp(const MouseEvent& event) {
    if (pressedEntry_ < 0) {
        return false;
    }
    const int pressed = pressedEntry_;
    pressedEntry_ = -1;
    invalidate();
    if (!interactive() || entryAt(event.position) != pressed) {
        return true;
    }
    const Entry& entry = entries_[static_cast<std::size_t>(pressed)];
    if (onItemActivated_ && entry.itemIndex >= 0) {
        onItemActivated_(entry.itemIndex);
    }
    return true;
}

CursorKind Menu::cursor(Point point) const {
    return interactive() && entryAt(point) >= 0 ? CursorKind::Pointer : CursorKind::Default;
}

bool Menu::isFocusable() const {
    return !disabled();
}

bool Menu::hasInteractionState() const {
    return hoveredEntry_ >= 0 || pressedEntry_ >= 0;
}

void Menu::resetInteractionState() {
    hoveredEntry_ = -1;
    pressedEntry_ = -1;
}

} // namespace oneui
