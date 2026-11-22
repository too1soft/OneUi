#pragma once
#include "oneui/controls/table.h"
#include "oneui/layout/stack.h"
#include <type_traits>

namespace oneui { class IconButton; }
namespace oneui::internal {
struct TableRowProjection {
    struct Payload {
        std::vector<TableCell> rich;
        std::vector<std::wstring> plain;
    };
    std::vector<std::shared_ptr<const std::vector<TableCell>>> source;
    std::vector<std::shared_ptr<Payload>> payloads;
    std::vector<std::size_t> active;
};
struct TablePresentation {
    std::vector<TableColumn> authored;
    std::optional<std::vector<float>> widths;
    std::optional<float> selectionWidth;
    std::unique_ptr<TableRowProjection> rows;
};
struct FlexPresentation {
    std::weak_ptr<Widget> child;
    StackFlexPresentation value;
};
struct WidgetPresentation {
    std::unique_ptr<TablePresentation> table;
    std::map<const Widget*, FlexPresentation> flex;
    std::shared_ptr<IconButton> paneHeaderMore;
    float tabsLastPaintWidth = -1.0f;
};
// The public token aliases the first member of the object in its control block.
// Optional presentation state is allocated only when a widget needs it.
struct WidgetLifetime {
    int token = 0;
    WidgetPresentation* presentation = nullptr;
    ~WidgetLifetime() { delete presentation; }
    WidgetLifetime() = default;
    WidgetLifetime(const WidgetLifetime&) = delete;
};
static_assert(std::is_standard_layout_v<WidgetLifetime>);

struct WidgetPresentationAccess {
    static WidgetLifetime& owner(const Widget& widget) {
        // A standard-layout object and its first member are pointer-interconvertible.
        return *reinterpret_cast<WidgetLifetime*>(widget.lifetime_.get());
    }
    static WidgetPresentation* find(const Widget& widget) {
        return owner(widget).presentation;
    }
    static WidgetPresentation& get(Widget& widget) {
        auto& presentation = owner(widget).presentation;
        if (!presentation) presentation = new WidgetPresentation();
        return *presentation;
    }
    static TablePresentation& table(Table& widget, const std::vector<TableColumn>& initial) {
        auto& value = get(widget).table;
        if (!value) { value = std::make_unique<TablePresentation>(); value->authored = initial; }
        return *value;
    }
};
} // namespace oneui::internal
