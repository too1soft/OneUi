#include "oneui_c_api_controls_internal.h"
#include "oneui_c_api_error.h"
#include "oneui/controls/interactive_surface.h"
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

namespace {

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

} // namespace

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

void oneui_interactive_surface_set_on_pointer_moved(
    OneUiWidget* surface,
    OneUiPointerCallback callback,
    void* user_data) {
    auto* nativeSurface = asWidget<oneui::InteractiveSurface>(surface);
    if (!nativeSurface) {
        return;
    }
    if (!callback) {
        nativeSurface->setOnPointerMoved(nullptr);
        return;
    }
    nativeSurface->setOnPointerMoved([callback, user_data](const oneui::MouseEvent& event) {
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

void oneui_interactive_surface_set_on_hover_changed(
    OneUiWidget* surface,
    OneUiBoolCallback callback,
    void* user_data) {
    auto* nativeSurface = asWidget<oneui::InteractiveSurface>(surface);
    if (!nativeSurface) {
        return;
    }
    if (!callback) {
        nativeSurface->setOnHoverChanged(nullptr);
        return;
    }
    nativeSurface->setOnHoverChanged([callback, user_data](bool hovered) {
        callback(hovered ? 1 : 0, user_data);
    });
}

void oneui_interactive_surface_set_on_focus_changed(
    OneUiWidget* surface,
    OneUiBoolCallback callback,
    void* user_data) {
    auto* nativeSurface = asWidget<oneui::InteractiveSurface>(surface);
    if (!nativeSurface) {
        return;
    }
    if (!callback) {
        nativeSurface->setOnFocusChanged(nullptr);
        return;
    }
    nativeSurface->setOnFocusChanged([callback, user_data](bool hovered) {
        callback(hovered ? 1 : 0, user_data);
    });
}

void oneui_interactive_surface_set_on_drag(OneUiWidget* surface, OneUiPointerDragCallback callback, float threshold, void* user_data) {
    auto* native = asWidget<oneui::InteractiveSurface>(surface);
    if (!native) return;
    if (!callback) { native->setOnDrag(nullptr); return; }
    native->setOnDrag([callback,user_data](const oneui::PointerDragEvent& event) {
        const auto& e = event.pointer;
        const OneUiPointerEvent value{e.position.x,e.position.y,static_cast<int>(e.button),e.clickCount,e.shift?1:0,e.control?1:0,e.alt?1:0};
        callback(static_cast<int>(event.phase),event.origin.x,event.origin.y,&value,user_data);
    },threshold);
}

void oneui_interactive_surface_set_pointer_cursor(OneUiWidget* surface, int cursor) {
    if (auto* native = asWidget<oneui::InteractiveSurface>(surface)) {
        if (cursor>=0 && cursor<=9) native->setPointerCursor(static_cast<oneui::CursorKind>(cursor));
    }
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
