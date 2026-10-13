#include "oneui_c_api_controls_internal.h"
#include "oneui_c_api_error.h"
#include "oneui/controls/popup.h"
#include "oneui/layout/overlay_host.h"
#include "oneui/layout/panel.h"
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

void oneui_popup_set_on_closed(OneUiWidget* popup, OneUiVoidCallback callback, void* user_data) {
    auto nativePopup = asWidget<oneui::Popup>(popup);
    if (!nativePopup) return;
    if (!callback) { nativePopup->setOnClosed({}); return; }
    nativePopup->setOnClosed([callback, user_data] { callback(user_data); });
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
