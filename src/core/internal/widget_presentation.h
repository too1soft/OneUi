#pragma once
#include "oneui/controls/table.h"
#include "oneui/layout/stack.h"
#include <stdexcept>

namespace oneui { class IconButton; }
namespace oneui::internal {
struct TablePresentation {
    std::vector<TableColumn> authored;
    std::optional<std::vector<float>> widths;
    std::optional<float> selectionWidth;
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
// Owned by Widget's existing lifetime control block. No address-keyed global
// registry, no strong child references, and no Widget/derived layout changes.
// Widget is noncopyable (its existing unique_ptr member); access follows the
// same owning UI thread as its ordinary mutable presentation state.
struct WidgetLifetime {
    std::unique_ptr<WidgetPresentation> presentation;
    void operator()(int* token) { presentation.reset(); delete token; }
};
struct WidgetPresentationAccess {
    static WidgetPresentation* find(const Widget& widget) {
        auto owner = std::get_deleter<WidgetLifetime>(widget.lifetime_);
        return owner ? owner->presentation.get() : nullptr;
    }
    static WidgetPresentation& get(Widget& widget) {
        auto owner = std::get_deleter<WidgetLifetime>(widget.lifetime_);
        if (!owner) throw std::logic_error("Widget presentation lifetime unavailable");
        if (!owner->presentation) owner->presentation = std::make_unique<WidgetPresentation>();
        return *owner->presentation;
    }
    static TablePresentation& table(Table& widget, const std::vector<TableColumn>& initial) {
        auto& value = get(widget).table;
        if (!value) { value = std::make_unique<TablePresentation>(); value->authored = initial; }
        return *value;
    }
};
} // namespace oneui::internal
