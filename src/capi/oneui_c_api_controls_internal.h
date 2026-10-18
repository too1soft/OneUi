#pragma once

#include "oneui_c_api_internal.h"
#include "oneui_c_api_widget_internal.h"
#include "oneui/style.h"
#include <type_traits>

namespace oneui { enum class PointerButton; }

namespace oneui::capi {

// These operations retain the single registry and style-scope lifetime owner.
OneUiWidget* wrap(std::shared_ptr<oneui::Widget> widget);
void syncStyleBinding(const OneUiWidget* wrapper);
void applyStyleSheet(OneUiWidget* wrapper, std::shared_ptr<oneui::StyleSheet> sheet);
void applyCurrentStyleSheet(OneUiWidget* wrapper);
oneui::StyleNode styleNodeFor(const OneUiWidget* widget);
int clampIconSymbol(int symbol);
void copyUtf8Field(const std::string& value, char* buffer, std::size_t bufferLength);
oneui::FocusRingStyleOverride toFocusRingOverride(const OneUiFocusRingStyle& style);
OneUiPointerButton toCButton(oneui::PointerButton button);
OneUiPointerButton toCButton(oneui::MouseButton button);

template <typename T>
T* asWidget(OneUiWidget* widget) {
    static_assert(std::is_base_of<oneui::Widget, T>::value, "T must be a OneUI widget");
    if (!widget || !widget->widget) {
        return nullptr;
    }
    return dynamic_cast<T*>(widget->widget.get());
}

} // namespace oneui::capi
