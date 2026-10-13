#include "oneui_c_api_controls_internal.h"
#include "oneui_c_api_error.h"
#include "oneui_c_api_snapshot_internal.h"
#include "oneui_c_api_window_internal.h"
#include "oneui/ui_style_diagnostics.h"
#include "oneui/native_interop.h"
#include "oneui/view.h"
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

#include <mutex>
std::shared_ptr<oneui::Widget> oneui::retainNativeWidget(const OneUiWidget* handle) {
    return handle ? handle->widget : nullptr;
}

struct OneUiFocusBookmark {
    std::weak_ptr<oneui::Widget> target;
    bool focusVisible = false;
};


namespace {

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

} // namespace

namespace {
std::weak_ptr<oneui::StyleSheet> gDefaultStyleSheet;
std::mutex gWidgetRegistryMutex;
std::vector<std::shared_ptr<OneUiWidgetStyleBinding>> gWidgetRegistry;

} // namespace

namespace oneui::capi {

OneUiWidget* wrap(std::shared_ptr<oneui::Widget> widget) {
    if (!widget) {
        return nullptr;
    }
    auto wrapper = std::make_unique<OneUiWidget>();
    wrapper->widget = std::move(widget);
    wrapper->styleSheet = gDefaultStyleSheet.lock();
    wrapper->styleBinding = std::make_shared<OneUiWidgetStyleBinding>();
    wrapper->styleBinding->widget = wrapper->widget;
    wrapper->styleBinding->styleSheet = wrapper->styleSheet;
    const std::weak_ptr<OneUiWidgetStyleBinding> weakBinding = wrapper->styleBinding;
    wrapper->widget->setOnStyleScopeChanged([weakBinding] {
        const auto binding = weakBinding.lock();
        if (!binding) return;
        const auto widget = binding->widget.lock();
        const auto declared = binding->styleSheet.lock();
        if (!widget || !declared) return;
        OneUiWidget mounted{widget, declared, binding->tag, binding->classes, binding};
        applyStyleSheet(&mounted, declared);
    });
    OneUiWidget* raw = wrapper.release();
    {
        std::lock_guard<std::mutex> lock(gWidgetRegistryMutex);
        // Dynamic clients rebuild views without reloading their stylesheet.
        // Pruning only on stylesheet refresh retained every dead binding for
        // the entire process lifetime. Amortize cleanup during allocation.
        static unsigned allocationsSincePrune = 0;
        if (++allocationsSincePrune >= 128) {
            allocationsSincePrune = 0;
            gWidgetRegistry.erase(std::remove_if(gWidgetRegistry.begin(), gWidgetRegistry.end(),
                [](const auto& binding) { return !binding || binding->widget.expired(); }), gWidgetRegistry.end());
        }
        gWidgetRegistry.push_back(raw->styleBinding);
    }
    if (raw->styleSheet) {
        applyStyleSheet(raw, raw->styleSheet);
    }
    return raw;
}

void syncStyleBinding(const OneUiWidget* wrapper) {
    if (!wrapper || !wrapper->styleBinding) {
        return;
    }
    wrapper->styleBinding->widget = wrapper->widget;
    wrapper->styleBinding->styleSheet = wrapper->styleSheet;
    wrapper->styleBinding->tag = wrapper->tag;
    wrapper->styleBinding->classes = wrapper->classes;
}

DebugStyleMap debugStyleBindings() {
    DebugStyleMap bindingsByWidget;
    for(const auto& entry:oneui::ui::styleDiagnostics()) {
        if(const auto widget=entry.widget.lock())
            bindingsByWidget[widget.get()]={entry.node.tag,entry.node.classes,{},entry.box};
    }
    std::lock_guard<std::mutex> lock(gWidgetRegistryMutex);
    bindingsByWidget.reserve(gWidgetRegistry.size());
    for (const auto& binding : gWidgetRegistry) {
        if (!binding) {
            continue;
        }
        const auto mounted = binding->widget.lock();
        if (mounted) {
            bindingsByWidget[mounted.get()] = DebugStyleBinding{
                binding->tag,
                binding->classes,
                mounted->styleScope() ? mounted->styleScope() : binding->styleSheet.lock(),
            };
        }
    }
    return bindingsByWidget;
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

} // namespace oneui::capi

void oneui_window_set_style_sheet(OneUiWindow* window, OneUiStyleSheet* style_sheet) {
    if (!window || !style_sheet || !style_sheet->sheet) {
        return;
    }
    window->styleSheet = style_sheet->sheet;
    gDefaultStyleSheet = style_sheet->sheet;
}

void oneui_window_refresh_style_sheet(OneUiWindow* window) {
    if (!window || !window->window || !window->styleSheet) {
        return;
    }
    std::vector<std::shared_ptr<OneUiWidgetStyleBinding>> bindings;
    {
        std::lock_guard<std::mutex> lock(gWidgetRegistryMutex);
        gWidgetRegistry.erase(
            std::remove_if(
                gWidgetRegistry.begin(),
                gWidgetRegistry.end(),
                [](const auto& binding) {
                    return !binding || binding->widget.expired();
                }),
            gWidgetRegistry.end());
        bindings = gWidgetRegistry;
    }
    for (const auto& binding : bindings) {
        auto widget = binding->widget.lock();
        auto styleSheet = binding->styleSheet.lock();
        if (widget && styleSheet == window->styleSheet) {
            OneUiWidget mountedWidget;
            mountedWidget.widget = std::move(widget);
            mountedWidget.styleSheet = std::move(styleSheet);
            mountedWidget.styleBinding = binding;
            mountedWidget.tag = binding->tag;
            mountedWidget.classes = binding->classes;
            applyStyleSheet(&mountedWidget, window->styleSheet);
        }
    }
    window->window->requestRedraw();
}

void oneui_widget_destroy(OneUiWidget* widget) {
    delete widget;
}

void oneui_widget_set_on_size_changed(OneUiWidget* widget, OneUiClientSizeChangedCallback callback, void* user_data) {
    if (!widget || !widget->widget) return;
    if (!callback) widget->widget->setOnSizeChanged({});
    else widget->widget->setOnSizeChanged([callback,user_data](oneui::Size size) { callback(size.width,size.height,user_data); });
}

void oneui_widget_set_preferred_size(OneUiWidget* widget, float width, float height) {
    if (!widget || !widget->widget) {
        return;
    }
    widget->widget->setPreferredSize(oneui::Size{width, height});
}

OneUiRect oneui_widget_frame(const OneUiWidget* widget) {
    if (!widget || !widget->widget) {
        return OneUiRect{};
    }
    const oneui::Rect frame = widget->widget->frame();
    return OneUiRect{frame.x, frame.y, frame.width, frame.height};
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

int oneui_widget_focused(const OneUiWidget* widget) {
    return widget && widget->widget && widget->widget->focused() ? 1 : 0;
}

OneUiFocusBookmark* oneui_widget_capture_focus(const OneUiWidget* root) {
    if (!root || !root->widget) return nullptr;
    auto leaf = root->widget;
    while (auto next = leaf->activeFocusChild()) leaf = std::move(next);
    if (!leaf->focused() || !leaf->isFocusable()) return nullptr;
    return new OneUiFocusBookmark{leaf, leaf->focusVisible()};
}

int oneui_focus_bookmark_restore(const OneUiFocusBookmark* bookmark, OneUiWidget* root) {
    if (!bookmark || !root || !root->widget || !root->widget->visible() || root->widget->disabled()) return 0;
    auto target = bookmark->target.lock();
    if (!target || !target->visible() || target->disabled()) return 0;
    if (target == root->widget && target->isFocusable()) {
        target->onFocusChanged(true);
        target->setFocusVisible(bookmark->focusVisible);
        return 1;
    }
    auto view = std::dynamic_pointer_cast<oneui::View>(root->widget);
    return view && view->requestFocus(target.get(), bookmark->focusVisible) ? 1 : 0;
}

void oneui_focus_bookmark_destroy(OneUiFocusBookmark* bookmark) { delete bookmark; }

void oneui_widget_set_tooltip(OneUiWidget* widget, const wchar_t* tooltip) {
    if (!widget || !widget->widget) {
        return;
    }
    widget->widget->setTooltip(wideOrEmpty(tooltip));
}

void oneui_widget_set_accessible_name(OneUiWidget* widget, const wchar_t* name) {
    if (widget && widget->widget) widget->widget->setAccessibleName(wideOrEmpty(name));
}

void oneui_widget_set_accessible_description(OneUiWidget* widget, const wchar_t* description) {
    if (widget && widget->widget) widget->widget->setAccessibleDescription(wideOrEmpty(description));
}

void oneui_widget_set_accessible_selected(OneUiWidget* widget, int selected) {
    if (!widget || !widget->widget) return;
    auto state = widget->widget->accessibilityState();
    state.selected = selected != 0;
    widget->widget->setAccessibilityState(state);
}

void oneui_widget_set_classes(OneUiWidget* widget, const char* classes) {
    if (!widget || !widget->widget) {
        return;
    }
    widget->classes = splitClasses(classes);
    syncStyleBinding(widget);
    applyCurrentStyleSheet(widget);
}

void oneui_widget_set_style_node(OneUiWidget* widget, const char* tag, const char* classes) {
    if (!widget || !widget->widget) {
        return;
    }
    widget->tag = tag ? tag : "";
    widget->classes = splitClasses(classes);
    syncStyleBinding(widget);
    applyCurrentStyleSheet(widget);
}

void oneui_widget_apply_style_sheet(OneUiWidget* widget, OneUiStyleSheet* style_sheet) {
    if (!widget || !style_sheet || !style_sheet->sheet) {
        return;
    }
    applyStyleSheet(widget, style_sheet->sheet);
}

int oneui_widget_measure(OneUiWidget* widget, float max_width, float max_height, float* width, float* height) {
    if (!width || !height) return 0;
    *width = *height = 0;
    if (!widget || !widget->widget || std::isnan(max_width) || max_width < 0 ||
        std::isnan(max_height) || max_height < 0) return 0;
    try {
        const auto size = widget->widget->measure({max_width, max_height});
        *width = size.width; *height = size.height;
        return 1;
    } catch (...) { oneui::capi::recordCurrentException(__func__); return 0; }
}

int oneui_widget_natural_size(OneUiWidget* widget, float* width, float* height) {
    if (!width || !height) return 0;
    *width = *height = 0;
    if (!widget || !widget->widget) return 0;
    try {
        const auto size = widget->widget->naturalSize();
        *width = size.width; *height = size.height;
        return 1;
    } catch (...) { oneui::capi::recordCurrentException(__func__); return 0; }
}
